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
#include "ncnf_cr.h"

/*
 * Constants for marking changed objects.
 */
enum difftype {
	DT_UNMODIFIED	= 0,
	DT_ADDED	= 1,
	DT_CHANGED	= 2,
	DT_DELETED	= 3,
};

/*
 * Function prototypes.
 */

static int
_ncnf_diff_level(struct ncnf_obj_s *oobj, struct ncnf_obj_s *nobj);

static int
_ncnf_check_difference(struct ncnf_obj_s *oobj, struct ncnf_obj_s *nobj,
	enum collections_e);

static int __ncnf_diff_resolve_references_callback(struct ncnf_obj_s *, int);
static int __ncnf_diff_invoke_lazy_notificators(struct ncnf_obj_s *, void *);
static int __ncnf_diff_invoke_notificators(struct ncnf_obj_s *, void *);

static int __ncnf_diff_remove_deleted(struct ncnf_obj_s *, void *key);

static int __ncnf_diff_undo_callback(struct ncnf_obj_s *, void *key);

static int __ncnf_diff_set_mark_func(struct ncnf_obj_s *, void *markv);

static int __ncnf_diff_cleanup_leaf(struct ncnf_obj_s *, void *key);


/*
 * Functions.
 */


int
_ncnf_diff(struct ncnf_obj_s *old_tree, struct ncnf_obj_s *new_tree) {
	int ret;

	if(old_tree->obj_class != NOBJ_ROOT
	  || new_tree->obj_class != NOBJ_ROOT) {
		errno = EINVAL;
		return -1;
	}

	/* Clear all marks - they will be required */
	if(old_tree->obj_class == NOBJ_ROOT) {
		_ncnf_walk_tree(old_tree,
			__ncnf_diff_cleanup_leaf, NULL);
		_ncnf_walk_tree(new_tree,
			__ncnf_diff_cleanup_leaf, NULL);
	}

	ret = _ncnf_diff_level(old_tree, new_tree);

	if(ret == 0) {
		/*
		 * After diffing, we should resolve
		 * all our references again.
		 */
		ret = _ncnf_cr_resolve_references(old_tree,
			__ncnf_diff_resolve_references_callback);
		assert(ret == 0);

		/* Invoke notificators */
		_ncnf_walk_tree(old_tree,
			__ncnf_diff_invoke_notificators, NULL);

		/* Invoke lazy notificators */
		_ncnf_walk_tree(old_tree,
			__ncnf_diff_invoke_lazy_notificators, NULL);

		/* Remove deleted entities */
		_ncnf_walk_tree(old_tree,
			__ncnf_diff_remove_deleted, NULL);

		/* Cleanup old tree */
		_ncnf_walk_tree(old_tree,
			__ncnf_diff_cleanup_leaf, NULL);
	} else {
		/* Undo additions and clear marks */
		_ncnf_walk_tree(old_tree,
			__ncnf_diff_undo_callback, NULL);
	}

	return ret;
}



/*
 * This procedure checks difference in entibutes, objects and
 * references.
 * For deleted and changed items in the old tree it marks 'em
 * as DT_DELETED and DT_CHANGED. For added items from the
 * new tree it clone it, mark as DR_ADDED and added it to the
 * old tree.
 */
static int
_ncnf_diff_level(struct ncnf_obj_s *oobj, struct ncnf_obj_s *nobj) {
	int ret;

	/* Check difference in the entity attributes. */
	ret = _ncnf_check_difference(oobj, nobj, COLLECTION_ATTRIBUTES);
	if(ret == -1) return -1;

	/* Check difference in the object entitities. */
	ret = _ncnf_check_difference(oobj, nobj, COLLECTION_OBJECTS);
	if(ret == -1) return -1;

	return 0;
}



/*
 * Internal functions.
 */


static int
_ncnf_check_difference(struct ncnf_obj_s *oobj, struct ncnf_obj_s *nobj,
	enum collections_e c) {
	collection_t *coll;
	collection_t *ncoll;
	int i;
	int stopped_at;

	assert(_NOBJ_CONTAINER(oobj) && _NOBJ_CONTAINER(nobj));

	coll = &oobj->m_collection[c];
	ncoll = &nobj->m_collection[c];
	for(i = 0; i < coll->entries; i++) {
		struct ncnf_obj_s *ent;
		struct ncnf_obj_s *nent;

		ent  = coll->entry[i].object;
		nent = _ncnf_coll_get(ncoll,
			CG_RETURN_POSITION,
			ent->type, ent->value,
			(void *)&stopped_at);

		if(nent) {
		    if(
			(ent->obj_class != nent->obj_class)
		    ) {
			/*
			 * Object is of another type,
			 * mean the previous was deleted.
			 * Here we relay on our anti-duplicate
			 * technique.
			 */
			ent->mark = DT_DELETED;
			oobj->mark = DT_CHANGED;	/* Parent object */

			/* Propagate DT_DELETED down the tree */
			ncnf_walk_tree(ent, __ncnf_diff_set_mark_func,
				(void *)DT_DELETED);
		    } else {

			if(ent->obj_class == NOBJ_COMPLEX) {

			    /* Walk down the tree */
			    if(_ncnf_diff_level(ent, nent))
				return -1;

			    /* Propagate changes back */
			    if(ent->mark == DT_CHANGED)
				oobj->mark = DT_CHANGED;

			} else if(ent->obj_class == NOBJ_REFERENCE) {

			    if(strcmp(ent->m_ref_value, nent->m_ref_value)
				|| strcmp(ent->m_ref_type, nent->m_ref_type)
			    ) {
				/*
				 * References are not absolutely equal.
				 * Maybe, something is changed.
				 */
				ent->mark = DT_CHANGED;
				oobj->mark = DT_CHANGED;

				ent->m_new_ref_type = bstr_ref(nent->m_ref_type);
				ent->m_new_ref_value = bstr_ref(nent->m_ref_value);
			    }

			    /*
			     * We will not be able to do an UNDO operation,
			     * but we should be able to at least
			     * modify the flags of a reference.
			     */
			    ent->m_ref_flags = nent->m_ref_flags;
			}

			/* Object retained */
			ncoll->entry[stopped_at].ignore_in_search = 1;
		    }
		} else {
		    ent->mark = DT_DELETED;
		    oobj->mark = DT_CHANGED;

		    /* Propagate DT_DELETED down the tree */
		    ncnf_walk_tree(ent, __ncnf_diff_set_mark_func,
			(void *)DT_DELETED);
		}

	}

	/* Add all unmarked properties */
	for(i = 0; i < ncoll->entries; i++) {
		struct ncnf_obj_s *nent;

		if(ncoll->entry[i].ignore_in_search) {
			ncoll->entry[i].ignore_in_search = 0;
			continue;
		}

		nent = _ncnf_obj_clone(oobj->mr, ncoll->entry[i].object);
		if(nent == NULL) {
			/* ENOMEM? */
			return -1;
		}

		if(_ncnf_coll_insert(oobj->mr, coll, nent, MERGE_NOFLAGS)) {
			/*
			 * Memory allocation failure (ENOMEM).
			 */
			_ncnf_obj_destroy(nent);
			return -1;
		} else {
			nent->parent = oobj;
		}

		nent->mark = DT_ADDED;
		oobj->mark = DT_CHANGED;
	}


	/* Mark all deleted properties to ignore them */
	for(i = 0; i < coll->entries; i++) {
		if(coll->entry[i].object->mark == DT_DELETED)
			coll->entry[i].ignore_in_search = 1;
	}


	return 0;
}


/*
 * Even more internal functions.
 */


/*
 * In any case, this function should succeed, because
 * it does not allocate memory and logically should find everything.
 */
static int
__ncnf_diff_resolve_references_callback(struct ncnf_obj_s *ref, int inv) {

	if(inv == 0) {
		if(ref->mark == DT_DELETED)
			return -1;
		return 0;
	}

	/*
	 * Our reference is of type "attach",
	 * so check if referenced object is changed, than
	 * propagate changes up the tree.
	 */
	if((ref->m_ref_flags & 1)) {
		if(ref->m_direct_reference->mark != DT_UNMODIFIED) {
			/*
			 * Referenced object is changed,
			 * so changed the reference itself.
			 * Propagate changed up the tree.
			 */
			ref->mark = DT_CHANGED;

			while(ref->parent
			  && ref->parent->mark == DT_UNMODIFIED) {
				ref->parent->mark = DT_CHANGED;
				ref = ref->parent;
			}
		}

	}

	return 0;
}

static int
__ncnf_diff_invoke_notificators(struct ncnf_obj_s *obj, void *key) {

	(void)key;

	/*
	 * General notifications.
	 */

	if(obj->notify) {
		switch(obj->mark) {
		case DT_CHANGED:
			obj->notify((ncnf_obj *)obj, NCNF_OBJ_CHANGE,
				obj->notify_key);
			break;
		case DT_DELETED:
			obj->notify((ncnf_obj *)obj, NCNF_OBJ_DESTROY,
				obj->notify_key);
			break;
		case DT_ADDED:
			/*
			 * We can't notify something that already
			 * has an event notificator on it.
			 */
			assert(0);
			break;
		}
	}


	return 0;
}

static int
__ncnf_diff_invoke_lazy_notificators(struct ncnf_obj_s *obj, void *key) {
	(void)key;

	if(obj->mark == DT_CHANGED &&
			_NOBJ_CONTAINER(obj))
		_ncnf_check_lazy_filters(obj, DT_ADDED);

	return 0;
}

/*
 * Squeeze configuration to get rid of deleted values.
 */

static int
__ncnf_diff_remove_deleted(struct ncnf_obj_s *obj, void *key) {
	collection_t *coll;

	if(_NOBJ_CONTAINER(obj)) {

		coll = &obj->m_collection[COLLECTION_OBJECTS];
		_ncnf_coll_remove_marked(coll, DT_DELETED);
	
		coll = &obj->m_collection[COLLECTION_ATTRIBUTES];
		_ncnf_coll_remove_marked(coll, DT_DELETED);
	}

	return 0;
}


static int
__ncnf_diff_undo_callback(struct ncnf_obj_s *obj, void *key) {
	collection_t *coll;
	struct ncnf_obj_s *o;
	int i;

	(void)key;

	if(_NOBJ_CONTAINER(obj)) {

		coll = &obj->m_collection[COLLECTION_ATTRIBUTES];
		for(i = 0; i < coll->entries; i++) {
			if(coll->entry[i].object->mark == DT_ADDED) {
				_ncnf_coll_adjust_size(obj->mr, coll, i);
				coll->entries = i;
				break;
			}
		}
	
		coll = &obj->m_collection[COLLECTION_OBJECTS];
		for(i = 0; i < coll->entries; i++) {
			o = coll->entry[i].object;
	
			/* Shrink collection to eliminate all added entries */
			if(o->mark == DT_ADDED) {
				_ncnf_coll_adjust_size(obj->mr, coll, i);
				coll->entries = i;
				break;
			}
	
			if(o->obj_class == NOBJ_COMPLEX)
				/* Go down the tree */
				__ncnf_diff_undo_callback(o, key);
		}

	}


	obj->mark = 0;
	return 0;
}

static int
__ncnf_diff_set_mark_func(struct ncnf_obj_s *obj, void *markv) {
	obj->mark = (int)markv;
	return 0;
}



static int
__ncnf_diff_cleanup_leaf(struct ncnf_obj_s *obj, void *key) {

	(void)key;

	/* Clear mark */
	obj->mark = DT_UNMODIFIED;

	/* Clear side reference data. */
	if(obj->obj_class == NOBJ_REFERENCE) {
		bstr_free(obj->m_new_ref_type);
		bstr_free(obj->m_new_ref_value);
		obj->m_new_ref_type = NULL;
		obj->m_new_ref_value = NULL;
	}
	return 0;
}

