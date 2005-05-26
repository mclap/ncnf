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

int ncnf_cr_parse(void *_param);

extern int __ncnf_cr_lineno;
void ncnf_cr_restart( FILE * );
void *ncnf_cr__scan_string( const char *str );
void ncnf_cr__delete_buffer( void *buffer_state );

/*
 * Low-level function to expand insertions and assignments.
 */
static int _ncnf_cr_expand_insert(struct ncnf_obj_s *obj, struct ncnf_obj_s *insert, int relaxed_ns);
static int __ncnf_cr_resolve_assignment(struct ncnf_obj_s *obj, int (*func)(struct ncnf_obj_s *ref, int invocation), int recursion_depth);
static int __ncnf_cr_ra_callback(struct ncnf_obj_s *obj, void *key);


#define	MAX_RECURSION_DEPTH	128

/* Check for insertion loops */
static int _ncnf_cr_check_insertion_loops(struct ncnf_obj_s *, void *refs[], int refs_count);

/*
 * Read the configuration file and create the objects tree.
 */
int
_ncnf_cr_read(const char *cfdata, enum ncnf_source_type stype, struct ncnf_obj_s **root, int relaxed_ns) {
	FILE *fp;
	int ret;
	void *bstate = NULL;
	void *parse_param[2];

	if(cfdata == NULL || root == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch(stype) {
	case NCNF_ST_TEXT:
		fp = NULL;
		break;
	case NCNF_ST_FILENAME:
		fp = fopen(cfdata, "r");
		if(fp) {
			struct stat sb;

			if(fstat(fileno(fp), &sb) == -1) {
				/* fstat() failed */
				fclose(fp);
				return -1;
			}
	
			if((sb.st_mode & S_IFMT) != S_IFREG) {
				fclose(fp);
				errno = EIO;
				return -1;
			}
		} else {
			return -1;
		}

		break;
	default:
		assert(!"ncnf_cr_read: invalid stype");
		errno = EINVAL;
		return -1;
	}

	__ncnf_cr_lineno = 1;

	/*
	 * Prepare input source for LEX.
	 */
	if(fp) {
		ncnf_cr_restart(fp);
	} else {
		bstate = ncnf_cr__scan_string(cfdata);
	}

	*root = NULL;
	parse_param[0] = (void *)root;
	parse_param[1] = (void *)relaxed_ns;
	ret = ncnf_cr_parse((void *)parse_param);

	/*
	 * Destroy input source.
	 */
	if(fp) fclose(fp);

	if(ret) {
		if(*root)
			perror("ncnf root defined after failure!");
		return 1;
	}

	assert(*root);

	return 0;
}



/*
 * If we have an insertion, copy the contents of the referred object
 * into the referring object.
 */
static int
_ncnf_cr_expand_insert(struct ncnf_obj_s *obj, struct ncnf_obj_s *insert, int relaxed_ns) {
	struct ncnf_obj_s *dst;
	enum collections_e c;
	collection_t *coll;
	int i;

	/*
	 * Search for the object to be inserted
	 * from the current object and up to the root level.
	 */
	dst = _ncnf_get_obj(obj,
		insert->type, insert->value,
		NCNF_FIRST_OBJECT,
		_NGF_RECURSIVE | _NGF_IGNORE_REFS);
	if(dst == NULL) {
		/*
		 * Cannot happen: checked by loops checking code.
		 */
		_ncnf_debug_print(1,
			"Cannot resolve insertion: `%s \"%s\"' at line %d",
			insert->type,
			insert->value,
			insert->config_line
		);
		return -1;
	}

	/*
	 * We mark objects which were resolved.
	 * Resolving means the absence of the unresolved references
	 * to the foreign structures: insertions, inheritance, etc.
	 */
	if(dst->mark == 0) {
		/*
		 * Resolve object which we are trying to insert.
		 * Don't resolve if it is marked as resolved.
		 */
		dst->mark = 1;
		if(_ncnf_cr_resolve(dst, relaxed_ns)) {
			return -1;
		}
	}

	/* We need to copy everything from dst into obj */
	for(c = COLLECTION_ATTRIBUTES; c <= COLLECTION_OBJECTS; c++) {
		coll = &dst->m_collection[c];

		for(i = 0; i < coll->entries; i++) {
			struct ncnf_obj_s *clone;

			if(insert->m_insert_flags & 1) {
				/*
				 * Don't override local values.
				 */
				if(_ncnf_coll_get(&obj->m_collection[c],
					0, coll->entry[i].object->type,
					NULL, 0))
					continue;
			}

			clone = _ncnf_obj_clone(obj->mr, coll->entry[i].object);
			if(clone == NULL) {
				_ncnf_debug_print(1, "Can't clone object: %s",
					strerror(errno));
				return -1;
			}
			if(_ncnf_coll_insert(obj->mr, &obj->m_collection[c],
				clone, relaxed_ns
					?  MERGE_NOFLAGS : MERGE_DUPCHECK)) {
				if(errno == EEXIST) {
					_ncnf_debug_print(1,
						"Cannot insert object `%s \"%s\"' at line %d into entity `%s \"%s\"' at line %d: similar entry already there",
						clone->type, clone->value,
						clone->config_line,
						obj->type, obj->value,
						obj->config_line);
				}
				_ncnf_obj_destroy(clone);
				return -1;
			} else {
				if(insert->m_insert_flags & 1) {
					/*
					 * Mark newly added foreign values.
					 */
					obj->m_collection[c].entry
					[ obj->m_collection[c].entries - 1 ]
					.ignore_in_search = 1;
				}
				clone->parent = obj;
			}
		}

		if(insert->m_insert_flags & 1) {
			/*
			 * Unmark foreign values.
			 */
			for(i = 0; i < obj->m_collection[c].entries; i++)
			  obj->m_collection[c].entry[i].ignore_in_search = 0;
		}
	}


	return 0;
}


/*
 * Check for insertion loops.
 */

static int
_ncnf_cr_check_insertion_loops(struct ncnf_obj_s *root, void *refs_list[], int refs_count) {
	collection_t *coll;
	void *allocated = NULL;	/* Memory to inherit in recursion */
	int ret = -1;
	int i;

	if(refs_list == NULL) {
		/*
		 * Allocate stack for checking loops.
		 */
		assert(root->obj_class == NOBJ_ROOT);

		allocated = malloc(sizeof(void *) * MAX_RECURSION_DEPTH);
		if(allocated == NULL)
			return -1;

		refs_list = (void **)allocated;
		refs_count = 0;
	}

	/*
	 * Check whether this object was mentioned in the path already.
	 */
	for(i = 0; i < refs_count; i++) {
		if(refs_list[i] == root) {
			int j;
			_ncnf_debug_print(1, "Object `%s \"%s\"' at line %d indirectly referred to itself",
				root->type,
				root->value,
				root->config_line);
			_ncnf_debug_print(0, "Path:");
			for(j = 0; j <= i; j++) {
				_ncnf_debug_print(0,
					"%s [%s \"%s\"]@line=%d",
					j?",":"",
					((struct ncnf_obj_s *)refs_list[j])->type,
					((struct ncnf_obj_s *)refs_list[j])->value,
					((struct ncnf_obj_s *)refs_list[j])->config_line
				);
			}
			errno = ELOOP;
			goto finish;
		}
	}


	if(root->m_collection[COLLECTION_INSERTS].entries) {
		refs_list[refs_count++] = root;
		if(refs_count >= MAX_RECURSION_DEPTH) {
			errno = ETOOMANYREFS;
			goto finish;
		}
	}

	/*
	 * Check inserts.
	 */
	coll = &root->m_collection[COLLECTION_INSERTS];
	for(i = 0; i < coll->entries; i++) {
		struct ncnf_obj_s *obj, *ref;
		ref = coll->entry[i].object;
		obj = _ncnf_get_obj(root,
			ref->type, ref->value,
			NCNF_FIRST_OBJECT,
			_NGF_RECURSIVE | _NGF_IGNORE_REFS);
		if(obj == NULL) {
			_ncnf_debug_print(1, "Could not find object for insertion `insert %s \"%s\"' at line %d",
				ref->type,
				ref->value,
				ref->config_line);
			goto finish;
		}
		if(_ncnf_cr_check_insertion_loops(obj, refs_list, refs_count))
			goto finish;
	}


	/*
	 * Recursively go down the tree.
	 */
	coll = &root->m_collection[COLLECTION_OBJECTS];
	for(i = 0; i < coll->entries; i++) {
		struct ncnf_obj_s *obj;
		obj = coll->entry[i].object;

		switch(obj->obj_class) {
		case NOBJ_COMPLEX:
			if(_ncnf_cr_check_insertion_loops(obj,
					refs_list, refs_count))
				goto finish;
			break;
		case NOBJ_REFERENCE:
			break;
		default:
			assert(0);
		}

	}


	ret = 0;
finish:

	if(allocated) {
		assert(root->obj_class == NOBJ_ROOT);
		free(allocated);
	}

	return ret;
}



/*
 * Resolve indirect references and expand insertions.
 */

int
_ncnf_cr_resolve(struct ncnf_obj_s *obj, int relaxed_ns) {
	collection_t *coll;
	collection_t coll_s;
	int e;
	int i;

	if(!_NOBJ_CONTAINER(obj))
		return 0;

	if(obj->obj_class == NOBJ_ROOT) {
		/*
		 * Check for insertion loops.
		 */
		if(_ncnf_cr_check_insertion_loops(obj, NULL, 0))
			return -1;
	}

	/*
	 * Expanding inserts
	 */

	/*
	 * Expanding inserts 1.
	 * Inserts at the current level.
	 */
	coll_s = obj->m_collection[COLLECTION_INSERTS];
	memset(&obj->m_collection[COLLECTION_INSERTS], 0,
		sizeof(collection_t));

	for(i = 0, e = coll_s.entries; i < e; i++) {
		if(_ncnf_cr_expand_insert(obj,
				coll_s.entry[i].object, relaxed_ns))
			break;
	}

	_ncnf_coll_clear(obj->mr, &coll_s, 1);

	if(i < e)
		/* expand_insert failed. */
		return -1;

	/*
	 * Expanding inserts 2.
	 * Resolve inner objects.
	 */
	coll = &obj->m_collection[COLLECTION_OBJECTS];
	for(i = 0; i < coll->entries; i++) {
		if(coll->entry[i].object->obj_class != NOBJ_COMPLEX)
			continue;
		if(_ncnf_cr_resolve(coll->entry[i].object, relaxed_ns))
			return -1;
	}


	if(obj->obj_class == NOBJ_ROOT) {
		/*
		 * Resolve references, late binding.
		 */
		if(_ncnf_cr_resolve_references(obj, NULL))
			return -1;
	}

	return 0;
}



int
_ncnf_cr_resolve_references(struct ncnf_obj_s *root,
	int (*func)(struct ncnf_obj_s *ref, int invocation)) {
	return _ncnf_walk_tree(root, __ncnf_cr_ra_callback, func);
}



static int
__ncnf_cr_ra_callback(struct ncnf_obj_s *obj, void *key) {
	int (*func)(struct ncnf_obj_s *ref, int invocation) = key;

	return __ncnf_cr_resolve_assignment(obj, func, 0);
}


static int
__ncnf_cr_resolve_assignment(struct ncnf_obj_s *obj, int (*func)(struct ncnf_obj_s *ref, int invocation), int recursion_depth) {

	if(++recursion_depth > MAX_RECURSION_DEPTH) {
		/* Return with EPERM, it will be caught by invocator */
		errno = EPERM;
		return -1;
	}

	if(obj->obj_class == NOBJ_REFERENCE) {

		if(func && func(obj, 0))
			return 0;
	
		/*
		 * Prepare to invocation of cr_resolve_reference...
		 */
		if(obj->m_new_ref_type) {
			void *p;
			p = obj->m_ref_type;
			obj->m_ref_type = obj->m_new_ref_type;
			obj->m_new_ref_type = p;
			p = obj->m_ref_value;
			obj->m_ref_value = obj->m_new_ref_value;
			obj->m_new_ref_value = p;
		}
	
		/* Actual resolving */
		obj->m_direct_reference = _ncnf_get_obj(obj->parent,
			obj->m_ref_type, obj->m_ref_value,
			NCNF_FIRST_OBJECT,
			_NGF_RECURSIVE | _NGF_IGNORE_REFS);
	
		if(obj->m_direct_reference == NULL) {
			_ncnf_debug_print(1, "Cannot find right-hand object in reference `ref %s \"%s\" = %s \"%s\"' at line %d",
				obj->type,
				obj->value,
				obj->m_ref_type,
				obj->m_ref_value,
				obj->config_line
			);
			return -1;
		} else {
			if(obj->m_new_ref_type) {
				bstr_free(obj->m_new_ref_type);
				bstr_free(obj->m_new_ref_value);
				obj->m_new_ref_type = NULL;
				obj->m_new_ref_value = NULL;
			}
		}

		if(func)
			return func(obj, 1);
	} else if(obj->obj_class == NOBJ_ATTRIBUTE
			&& (obj->m_attr_flags & 1)) {
		struct ncnf_obj_s *resolved_attr;

		resolved_attr = _ncnf_get_obj(obj->parent, obj->value, NULL,
			NCNF_FIRST_ATTRIBUTE,
			_NGF_RECURSIVE | _NGF_IGNORE_REFS);
		if(resolved_attr == NULL) {
			_ncnf_debug_print(1,
				"Cannot find the right-hand attribute `%s' mentioned in assignment `%s = %s' at line %d (%s)",
				obj->value,
				obj->type,
				obj->value,
				obj->config_line,
				obj->parent->type
			);
			return -1;
		}

		if(resolved_attr == obj) {
			_ncnf_debug_print(1,
				"Attribute `%s = %s' at line %d "
				"resolves to itself",
				obj->type, obj->value, obj->config_line
			);
			errno = EINVAL;
			return -1;
		}

		/*
		 * If we're resolved to unresolved attribute,
		 * do recursion.
		 */
		if(resolved_attr->m_attr_flags & 1) {
			int ret = __ncnf_cr_resolve_assignment(resolved_attr,
				func, recursion_depth);
			if(ret == -1) {
				if(errno == EPERM) {
					_ncnf_debug_print(1,
					"Attribute `%s = %s' at line %d "
					"too deep recursion to expand",
					obj->type, obj->value, obj->config_line
					);
				}
				return -1;
			}
			assert((resolved_attr->m_attr_flags & 1) == 0);
		}

		bstr_free(obj->value);
		obj->value = bstr_ref(resolved_attr->value);
		obj->m_attr_flags &= ~1;
	}

	return 0;
}


