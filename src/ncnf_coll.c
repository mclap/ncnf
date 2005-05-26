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
 * Implementation of object Collection structure.
 */
#include "headers.h"
#include "ncnf_int.h"

/*
 * Adjust storage size of the collection.
 */
int
_ncnf_coll_adjust_size(void *mr, collection_t *coll, int new_count) {

	if(new_count > coll->entries) {

		/*
		 * Allocate and clear a few more entries.
		 */
		if(new_count > coll->size) {
			void *p;
			size_t new_size = mr
				? (coll->size + ((new_count + 3) & ~3))
				: new_count;

			p = realloc(coll->entry,
				new_size * sizeof(collection_entry));
			if(p == NULL)
				return -1;
			coll->entry = p;
			coll->size = new_size;
		}

		memset(&coll->entry[coll->entries],
			0,
			(new_count - coll->entries)
				* sizeof(collection_entry));

	} else {

		/*
		 * Destroy all inserted objects.
		 */

		for(; coll->entries > new_count;) {
			int num = --coll->entries;
			struct ncnf_obj_s *obj;

			obj = coll->entry[num].object;
			coll->entry[num].object = NULL;

			_ncnf_obj_destroy(obj);
		}

		if(new_count == 0) {
			if(coll->entry) {
				free(coll->entry);
				coll->entry = NULL;
				coll->size = 0;
			}
		}
	}

	return 0;
}


/*
 * Insert new object into collection.
 */
int
_ncnf_coll_insert(void *mr, collection_t *coll, struct ncnf_obj_s *obj, enum merge_flags merge_flags) {

	/* Check for duplicates */
	if(merge_flags & MERGE_DUPCHECK) {
		if(_ncnf_coll_get(coll,
			CG_TYPE_NOCASE | CG_NAME_NOCASE,
			(obj->obj_class == NOBJ_ATTRIBUTE
			  || obj->obj_class == NOBJ_LAZY_NOTIF)
			? obj->type
			: NULL,
				obj->value, NULL)) {
			errno = EEXIST;
			return -1;
		}
	}

	if(merge_flags & MERGE_PTRCHECK) {
		int ent;
		for(ent = 0; ent < coll->entries; ent++) {
			if(coll->entry[ent].object == obj) {
				errno = EEXIST;
				return -1;
			}
		}
	}

	if(_ncnf_coll_adjust_size(mr, coll, coll->entries + 1))
		/* ENOMEM */
		return -1;

	coll->entry[coll->entries++].object = obj;

	return 0;
}


/*
 * Add contents of the second collection to the first collection.
 */
int
_ncnf_coll_join(void *mr, collection_t *to, collection_t *from, struct ncnf_obj_s *parent, enum merge_flags merge_flags) {
	int to_idx, from_idx;

	/* Check for duplicates */
	if(merge_flags & MERGE_DUPCHECK) {
	    for(from_idx = 0; from_idx < from->entries; from_idx++) {
		struct ncnf_obj_s *obj = from->entry[from_idx].object;
		if(_ncnf_coll_get(to,
			CG_TYPE_NOCASE | CG_NAME_NOCASE,
			(obj->obj_class == NOBJ_ATTRIBUTE
			  || obj->obj_class == NOBJ_LAZY_NOTIF)
			? obj->type
			: NULL,
				obj->value, NULL)) {
			errno = EEXIST;
			return -1;
		}
	    }
	}

	if(_ncnf_coll_adjust_size(mr, to, to->entries + from->entries))
		/* ENOMEM */
		return -1;

	for(to_idx = to->entries, from_idx = 0;
		from_idx < from->entries;
			to_idx++, from_idx++) {

		to->entry[to_idx] = from->entry[from_idx];
		if(parent)
			to->entry[to_idx].object->parent = parent;
	}

	to->entries += from->entries;

	if(merge_flags & MERGE_EMPTYSRC)
		/* Empty the source collection */
		_ncnf_coll_clear(mr, from, 0);

	return 0;
}

/*
 * Clear specified collection.
 */
void
_ncnf_coll_clear(void *mr, collection_t *coll, int destroy) {

	/* [Martin 03/03/05]: bug 1917. memory leak.
	 * we must do this before _ncnf_coll_adjust_size
	 */
	if(destroy == 0)
		coll->entries = 0;

	_ncnf_coll_adjust_size(mr, coll, 0);

	coll->entries = 0;

}


/*
 * Search in given collection for object specified by type or value or both.
 */
struct ncnf_obj_s *
_ncnf_coll_get(collection_t *coll, enum cget_flags flags,
	const char *opt_type, const char *opt_name,
		void *iterator) {
	struct ncnf_obj_s *found = NULL;
	struct ncnf_obj_s *found_last = NULL;
	int (*type_compare)(const char *, const char *);
	int (*name_compare)(const char *, const char *);
	int ignore_class;
	int opt_type_len;
	int opt_name_len;
	int entries;
	int i;

	type_compare = (flags & CG_TYPE_NOCASE) ? strcasecmp : strcmp;
	name_compare = (flags & CG_NAME_NOCASE) ? strcasecmp : strcmp;

	ignore_class = (flags & CG_IGNORE_REFERENCES)
		? NOBJ_REFERENCE
		: -1;

	opt_type_len = opt_type ? strlen(opt_type) : 0;
	opt_name_len = opt_name ? strlen(opt_name) : 0;

	for(i = 0, entries = coll->entries; i < entries; i++) {
		struct ncnf_obj_s *cur = coll->entry[i].object;

		/*
		 * Filters.
		 */

		if(opt_type)
			if(bstr_len(cur->type) != opt_type_len
			|| type_compare(cur->type, opt_type))
				continue;
		if(opt_name)
			if(bstr_len(cur->value) != opt_name_len
			|| name_compare(cur->value, opt_name))
				continue;

		if(ignore_class == coll->entry[i].object->obj_class)
			continue;

		if(coll->entry[i].ignore_in_search)
			continue;

		/*
		 * Found something.
		 */

		if((flags & CG_MARK_UNSEARCHABLE))
			coll->entry[i].ignore_in_search = 1;

		if(iterator) {
			if(flags & CG_RETURN_POSITION) {
				*(int *)iterator = i;
				return cur;
			} else {
				struct ncnf_obj_s *iterator_obj
					= (struct ncnf_obj_s *)iterator;
				if(_ncnf_coll_insert(0,
					&iterator_obj->m_iterator_collection,
						cur, MERGE_NOFLAGS)) {
					return NULL;
				}
				found = iterator;
			}
		} else {
			if((flags & CG_RETURN_CHAIN)) {
				if(found) {
					found_last->chain_next = cur;
				} else {
					found = cur;
				}
				found_last = cur;
				found_last->chain_next = NULL;
				found_last->chain_cur = NULL;
			} else {
			    	return cur;
			}
		}
	}

	if(found == NULL)
		errno = ESRCH;

	return found;
}


/*
 * Remove all objects with mark equal to match_mark
 * from the collection.
 */
void
_ncnf_coll_remove_marked(collection_t *coll, int match_mark) {
	int shift = 0;
	int k;

	assert(coll && match_mark);

	for(k = 0; k < coll->entries; k++) {
		ncnf_obj *obj;

		if(shift)
			coll->entry[k] = coll->entry[k + shift];

		obj = coll->entry[k].object;

		if(obj->mark == match_mark) {
			/* Squeeze a little tighter */
			shift++;
			coll->entries--;
			coll->entry[k].object = NULL;
			_ncnf_obj_destroy(obj);
			k--;
		}
	}

}


int ncnf_coll_get_nentry(collection_t *coll)
{
	if(coll) {
		return coll->entries;
	} else {
		errno = EINVAL;
		return 0;
	}
}

struct ncnf_obj_s* ncnf_coll_get_obj_at(collection_t *coll, int idx)
{
	if(coll && idx < coll->entries && idx >= 0) {
		return coll->entry[idx].object;
	} else {
		errno = EINVAL;
		return NULL;
	}
}

#ifdef	MODULE_TEST

int
main(int ac, char **av) {
	static collection_t coll;
	struct ncnf_obj_s *obj;
	int ret;

	/*
	 * Test 1
	 */

	/* Mark */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE, NULL, NULL, 0);
	assert(obj);
	obj->mark = 1;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	_ncnf_coll_remove_marked(&coll, 1);

	assert(coll.entries == 0);

	/*
	 * Test 2
	 */

	/* Mark */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE,
		str2bstr("a", -1), str2bstr("marked", -1), 0);
	assert(obj);
	obj->mark = 1;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	/* Zero */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE,
		str2bstr("b", -1), str2bstr("unmarked", -1), 0);
	assert(obj);
	obj->mark = 0;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	/* Mark */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE,
		str2bstr("c", -1), str2bstr("marked", -1), 0);
	assert(obj);
	obj->mark = 1;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	_ncnf_coll_remove_marked(&coll, 1);

	assert(coll.entries == 1);
	assert(coll.entry[0].object->mark != 1);
	assert(!strcmp(coll.entry[0].object->type, "b"));

	/*
	 * Test 3
	 */

	/* Zero */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE,
		str2bstr("c", -1), str2bstr("unmarked", -1), 0);
	assert(obj);
	obj->mark = 0;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	assert(coll.entries == 2);

	/* Mark */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE,
		str2bstr("d", -1), str2bstr("marked", -1), 0);
	assert(obj);
	obj->mark = 1;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	/* Zero */
	obj = _ncnf_obj_new(0, NOBJ_ATTRIBUTE,
		str2bstr("g", -1), str2bstr("unmarked", -1), 0);
	assert(obj);
	obj->mark = 0;
	ret = _ncnf_coll_insert(0, &coll, obj, MERGE_DUPCHECK);
	assert(ret == 0);

	_ncnf_coll_remove_marked(&coll, 1);

	assert(coll.entries == 3);

	_ncnf_coll_adjust_size(0, &coll, 0);

	assert(coll.entries == 0);

	return 0;
}

#endif
