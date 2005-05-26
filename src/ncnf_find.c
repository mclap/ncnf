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
#include "ncnf_find.h"

struct ncnf_obj_s *
_na_find_objects(struct ncnf_obj_s *start_level,
	char *types_tree,
	int (*opt_filter)(struct ncnf_obj_s *, void *),
	void *opt_key)
{
	ncnf_sf_svect *tt;	/* types tokens */
	struct ncnf_obj_s *result_iter = NULL;
	struct ncnf_obj_s *iter;

	assert(start_level);
	assert(types_tree);

	/*
	 * Initialize necessary structures.
	 */

	tt = ncnf_sf_split(types_tree, "/", 0);
	if(tt == NULL)
		/* ENOMEM */
		return NULL;
	if(tt->count == 0) {
		ncnf_sf_sfree(tt);
		errno = EINVAL;
		return NULL;
	}

	assert(tt->count);

	result_iter = _ncnf_obj_new(0, NOBJ_ITERATOR, NULL, NULL, 0);
	if(result_iter == NULL)
		/* ENOMEM */
		goto fail;

	/*
	 * Find all interesting elements at this level.
	 */
	iter = _ncnf_get_obj(start_level, tt->list[0], NULL,
		NCNF_ITER_OBJECTS, _NGF_NOFLAGS);
	if(iter == NULL) {
		if(errno != ESRCH)
			goto fail;
	} else {
		struct ncnf_obj_s *obj;
		int is_last_level;

		is_last_level = (tt->count == 1);

		while((obj = _ncnf_iter_next(iter))) {

			if(opt_filter) {
				int ret;
				int tmp_errno = errno;

				errno = -2;
				ret = opt_filter(obj, opt_key);
				if(ret < 0) {
					assert(errno != -2);
					if(errno == -2) {
						errno = EFAULT;
					}
					break;
				}
				errno = tmp_errno;
				if(ret > 0)
					continue;
			}

			if(is_last_level) {
				if(_ncnf_coll_insert(result_iter->mr,
					&result_iter->m_iterator_collection,
					obj, MERGE_NOFLAGS)
				)
					break;
			} else {
				struct ncnf_obj_s *ll_result;
				size_t combined_len = 0;
				char *new_tree;
				int cnt;
				char *p;

				/* Compute the length */
				for(cnt = 1; cnt < tt->count; cnt++)
					combined_len += 1 + tt->lens[cnt];

				p = new_tree = alloca(combined_len + 1);
				for(cnt = 1; cnt < tt->count; cnt++) {
					memcpy(p, tt->list[cnt], tt->lens[cnt]);
					p += tt->lens[cnt];
					*p = '/';
					if(cnt < (tt->count - 1))
						p++;
				}
				*p = '\0';

				ll_result = _na_find_objects(obj, new_tree,
					opt_filter, opt_key);
				if(ll_result == NULL) {
					if(errno == ESRCH)
						continue;
					break;
				}
				if(_ncnf_coll_join(result_iter->mr,
					&result_iter->m_iterator_collection,
					&ll_result->m_iterator_collection,
					NULL, MERGE_NOFLAGS))
					break;
				_ncnf_obj_destroy(ll_result);
			}
		}

		_ncnf_obj_destroy(iter);

		if(obj != NULL)
			goto fail;
	}

	ncnf_sf_sfree(tt);

	/* Remove envelope if there is no data */
	if(result_iter->m_iterator_collection.entries == 0) {
		_ncnf_obj_destroy(result_iter);
		errno = ESRCH;
		return NULL;
	}

	return result_iter;

fail:

	if(result_iter)
		_ncnf_obj_destroy(result_iter);

	ncnf_sf_sfree(tt);
	return NULL;
}

