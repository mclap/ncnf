/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005  Netli, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */
/*
 * Implementation of construction, cloning and destruction.
 */
#include "headers.h"
#include "ncnf_int.h"

/*
 * Basic constructor for the configuration object.
 * Takes optional type and name/value, and inserts their copies into
 * the newly created structure.
 */
struct ncnf_obj_s *
_ncnf_obj_new(void *mr, enum obj_class obj_class, const bstr_t type, const bstr_t value, int config_line) {
	struct ncnf_obj_s *nobj;

	nobj = calloc(1, sizeof(struct ncnf_obj_s));
	if(nobj == NULL)
		return NULL;

	/*
	 * Initializing basic header data.
	 */

	nobj->obj_class = obj_class;
	if(type) nobj->type = bstr_ref(type);
	if(value) nobj->value = bstr_ref(value);
	nobj->config_line = config_line;
	nobj->mr = mr;

	return nobj;
}


/*
 * Kill everything alive within the configuration object structure.
 */
void
_ncnf_obj_destroy(struct ncnf_obj_s *obj) {

	assert(obj->obj_class != NOBJ_INVALID);

	/*
	 * Clear the common object header.
	 */

	bstr_free(obj->type);
	bstr_free(obj->value);

	/*
	 * Class-dependent destruction.
 	 */

	switch(obj->obj_class) {
	case NOBJ_ROOT:
	case NOBJ_COMPLEX:
		{
			enum collections_e c;
			for(c = 0; c < MAX_COLLECTIONS; c++)
				_ncnf_coll_clear(obj->mr,
					&obj->m_collection[c], 1);
		}
		break;
	case NOBJ_REFERENCE:
		assert(obj->m_ref_type);
		assert(obj->m_ref_value);

		bstr_free(obj->m_ref_type);
		bstr_free(obj->m_ref_value);

		obj->m_ref_type = 0;
		obj->m_ref_value = NULL;

		bstr_free(obj->m_new_ref_type);
		obj->m_new_ref_type = NULL;
		bstr_free(obj->m_new_ref_value);
		obj->m_new_ref_value = NULL;
		break;
	case NOBJ_ITERATOR:
		/* Do not destroy contained objects */
		_ncnf_coll_clear(obj->mr, &obj->m_iterator_collection, 0);
		break;
	case NOBJ_LAZY_NOTIF:
	case NOBJ_ATTRIBUTE:
	case NOBJ_INSERTION:
		/* Do nothing */
		break;
	case NOBJ_INVALID:	/* Mostly for switch()/enum integration */
		/* Do nothing */
		break;
	}


	obj->obj_class = NOBJ_INVALID;	/* Post invalidity */
	free(obj);
}


/*
 * Insert an object into an object.
 */
int
_ncnf_attach_obj(struct ncnf_obj_s *to, struct ncnf_obj_s *what, int relaxed_ns) {
	collection_t *coll;


	switch(to->obj_class) {
	case NOBJ_ROOT:
	case NOBJ_COMPLEX:

		coll = NULL;

		switch(what->obj_class) {
		case NOBJ_COMPLEX:
		case NOBJ_REFERENCE:
			coll = &to->m_collection[COLLECTION_OBJECTS];
			goto insert;
		case NOBJ_ATTRIBUTE:
			coll = &to->m_collection[COLLECTION_ATTRIBUTES];
			goto insert;
		case NOBJ_INSERTION:
			coll = &to->m_collection[COLLECTION_INSERTS];
			goto insert;
		case NOBJ_LAZY_NOTIF:
			coll = &to->m_collection[COLLECTION_LAZY_NOTIF];
			goto insert;
	insert:
			what->parent = to;
			return _ncnf_coll_insert(to->mr, coll, what,
				(relaxed_ns ? MERGE_NOFLAGS : MERGE_DUPCHECK)
				| MERGE_EMPTYSRC);
		case NOBJ_ROOT:
			{
				enum collections_e c;
				for(c = 0; c < MAX_COLLECTIONS; c++) {
					if(_ncnf_coll_join(to->mr,
						&to->m_collection[c],
						&what->m_collection[c],
						to,
						(relaxed_ns ? MERGE_NOFLAGS : MERGE_DUPCHECK)
						| MERGE_EMPTYSRC
					)) {
						_ncnf_obj_destroy(what);
						return -1;
					}
				}
			}
			return 0;
		case NOBJ_ITERATOR:
		case NOBJ_INVALID:
			/* Never happens */
			assert(0);
		}

		/* Fall through */
	default:
		/* Cant attach object to the non-complex class */
		errno = EINVAL;
		return -1;
	}


}

/*
 * Create a full copy of the given object.
 */
struct ncnf_obj_s *
_ncnf_obj_clone(void *mr, struct ncnf_obj_s *root) {
	struct ncnf_obj_s *obj;
	enum collections_e c;

	/*
	 * Create wireframe of the new object.
	 */
	obj = _ncnf_obj_new(mr, root->obj_class,
		root->type, root->value, root->config_line);
	if(obj == NULL)
		return NULL;

	switch(obj->obj_class) {
	case NOBJ_ROOT:
	case NOBJ_COMPLEX:
		for(c = 0; c < MAX_COLLECTIONS; c++) {
			collection_t *coll = &root->m_collection[c];
			int i;

			for(i = 0; i < coll->entries; i++) {
				struct ncnf_obj_s *clone;

				clone = _ncnf_obj_clone(mr,
						coll->entry[i].object);
				if(clone == NULL) {
					_ncnf_obj_destroy(obj);
					return NULL;
				}

				if(_ncnf_coll_insert(mr, &obj->m_collection[c],
						clone,
						/* Cloning does not have
						 * to do DUPCHECK */
						MERGE_NOFLAGS)
				) {
					_ncnf_obj_destroy(clone);
					_ncnf_obj_destroy(obj);
					return NULL;
				} else {
					clone->parent = obj;
				}
			}
		}
		break;
	case NOBJ_ATTRIBUTE:
		obj->m_attr_flags = root->m_attr_flags;
		break;
	case NOBJ_REFERENCE:
		obj->m_ref_type = bstr_ref(root->m_ref_type);
		obj->m_ref_value = bstr_ref(root->m_ref_value);
		obj->m_ref_flags = root->m_ref_flags;
		obj->m_direct_reference = root->m_direct_reference;
		break;
	case NOBJ_INSERTION:
	default:
		/* Do nothing */
		break;
	}

	/* Object copied. */
	return obj;
}


