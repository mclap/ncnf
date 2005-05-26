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
#include "headers.h"
#include "ncnf_int.h"

struct ncnf_obj_s *
_ncnf_real_object(struct ncnf_obj_s *obj) {
	if(obj->obj_class == NOBJ_REFERENCE) {
		assert(obj->m_direct_reference);
		return obj->m_direct_reference;
	}
	return obj;
}

struct ncnf_obj_s *
_ncnf_get_obj(struct ncnf_obj_s *obj,
	const char *opt_type, const char *opt_name,
		enum ncnf_get_style style, enum _ncnf_get_flags flags) {

	struct ncnf_obj_s *iterator = NULL;
	struct ncnf_obj_s *found;
	enum cget_flags cget_flags;
	collection_t *coll = NULL;

retry:
	switch(obj->obj_class) {
	case NOBJ_ROOT:
	case NOBJ_COMPLEX:
		break;
	case NOBJ_REFERENCE:
		obj = _ncnf_real_object(obj);
		goto retry;
	case NOBJ_INVALID:
		assert(obj->obj_class != NOBJ_INVALID);
	default:
		errno = EINVAL;
		return NULL;
	}

	/* _ncnf_coll_get() flags */
	cget_flags = (flags & _NGF_IGNORE_REFS) ? CG_IGNORE_REFERENCES : 0;

	/*
	 * Initialize iterator, if necessary.
	 */
	switch(style) {
	case NCNF_FIRST_OBJECT:
		coll = &obj->m_collection[COLLECTION_OBJECTS];
		break;

	case NCNF_FIRST_ATTRIBUTE:
		coll = &obj->m_collection[COLLECTION_ATTRIBUTES];
		break;

	case NCNF_ITER_OBJECTS:
		coll = &obj->m_collection[COLLECTION_OBJECTS];
		goto get_iterator;

	case NCNF_ITER_ATTRIBUTES:
		coll = &obj->m_collection[COLLECTION_ATTRIBUTES];

	  get_iterator:
		/*
		 * Prepare iterator body for adding found items later.
		 */
		iterator = _ncnf_obj_new(0, NOBJ_ITERATOR, NULL, NULL, 0);
		if(iterator == NULL)
			return NULL;
		break;

	case NCNF_CHAIN_OBJECTS:

		coll = &obj->m_collection[COLLECTION_OBJECTS];
		cget_flags |= CG_RETURN_CHAIN;
		break;

	case NCNF_CHAIN_ATTRIBUTES:

		coll = &obj->m_collection[COLLECTION_ATTRIBUTES];
		cget_flags |= CG_RETURN_CHAIN;
		break;

	default:
		errno = EINVAL;
		return NULL;
	}

	/* Search in straight descendents */
	found = _ncnf_coll_get(coll, cget_flags,
		opt_type, opt_name, iterator);
	if(found)
		return found;


	/*
	 * If no objects placed into iterator,
	 * destroy iterator.
	 */
	if(iterator)
		_ncnf_obj_destroy(iterator);

	/*
	 * Search one level higher, if requested.
	 */
	if((flags & _NGF_RECURSIVE) && obj->parent) {
		found = _ncnf_get_obj(obj->parent, opt_type,
			opt_name, style, flags);
		if(found)
			return found;
	}

	errno = ESRCH;
	return NULL;
}


char *
_ncnf_get_attr(struct ncnf_obj_s *obj, const char *type) {
	struct ncnf_obj_s *found;

	if(obj->obj_class == NOBJ_ATTRIBUTE)
		return obj->value;

	found = _ncnf_get_obj(obj, type, NULL,
		NCNF_FIRST_ATTRIBUTE, _NGF_NOFLAGS);
	if(found)
		return found->value;

	return NULL;
}



int
_ncnf_walk_tree(struct ncnf_obj_s *obj, int (*callback)(struct ncnf_obj_s *obj, void *key), void *key) {
	enum collections_e c;
	int r;

	switch(obj->obj_class) {
	case NOBJ_ROOT:
	case NOBJ_COMPLEX:

		r = callback(obj, key);
		if(r) return r;

		for(c = 0; c < MAX_COLLECTIONS; c++) {
		  collection_t *coll = &obj->m_collection[c];
		  int i;

		  for(i = 0; i < coll->entries; i++) {
		    r = _ncnf_walk_tree(coll->entry[i].object,
				callback, key);
		    if(r) return r;
		  }
		}
		break;
	case NOBJ_ATTRIBUTE:
	case NOBJ_REFERENCE:
		r = callback(obj, key);
		if(r) return r;

		break;
	case NOBJ_INSERTION:
	case NOBJ_ITERATOR:
	case NOBJ_LAZY_NOTIF:

		/* Do nothing */
		break;
	case NOBJ_INVALID:
		assert(obj->obj_class != NOBJ_INVALID);
	}

	return 0;
}


void
_ncnf_iter_rewind(struct ncnf_obj_s *iter) {
	if(iter->obj_class == NOBJ_ITERATOR) {
		iter->m_iterator_position = 0;
	} else {
		iter->chain_cur = NULL;
	}
}

struct ncnf_obj_s *
_ncnf_iter_next(struct ncnf_obj_s *iter) {
	struct ncnf_obj_s *obj;
	static void *nothing_is_here = "";

	if(iter->obj_class == NOBJ_ITERATOR) {
		/*
		 * This is the iterator - the specially allocated
		 * entity having the pointers to the selected objects.
		 */

		if(iter->m_iterator_position <
			iter->m_iterator_collection.entries) {

			obj = iter->m_iterator_collection
				.entry[iter->m_iterator_position++].object;

		} else {
			obj = NULL;
		}

	} else {
		/*
		 * This is the first object of the chain.
		 * The first object contains the pointer to the
		 * next object, and so on. The pointer is shifted
		 * forward and the previous value is returned.
		 */

		if(iter->chain_cur != nothing_is_here) {

			if(iter->chain_cur) {
				obj = iter->chain_cur;
			} else {
				/* Rewind to the start of the chain */
				obj = iter;
			}

			/*
			 * Shift the pointer.
			 */
			iter->chain_cur = obj->chain_next;
			if(iter->chain_cur == NULL
			  || iter->chain_cur == obj) {
				iter->chain_cur = nothing_is_here;
			}

		} else {
			obj = NULL;
		}

	}

	if(obj == NULL)
		errno = ESRCH;

	return obj;
}



