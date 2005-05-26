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
 * Walk the tree.
 */
#ifndef	__NCNF_WALK_H__
#define	__NCNF_WALK_H__

/*
 * Search the given object.
 */

enum _ncnf_get_flags {
  _NGF_NOFLAGS		= 0,
  _NGF_RECURSIVE	= 1,
  _NGF_IGNORE_REFS	= 2,
};

struct ncnf_obj_s *_ncnf_get_obj(struct ncnf_obj_s *obj,
	const char *opt_type, const char *opt_name,
	enum ncnf_get_style style,
	enum _ncnf_get_flags);


int _ncnf_walk_tree(struct ncnf_obj_s *obj,
	int (*callback)(struct ncnf_obj_s *, void *key),
	void *key
	);

/* Resolve reference */
struct ncnf_obj_s *_ncnf_real_object(struct ncnf_obj_s *obj);

/*
 * Iterators
 */
struct ncnf_obj_s *_ncnf_iter_next(struct ncnf_obj_s *iter);
void _ncnf_iter_rewind(struct ncnf_obj_s *iter);


/*
 * Get attributes
 */
char *_ncnf_get_attr(struct ncnf_obj_s *obj, const char *type);

#endif	/* __NCNF_WALK_H__ */
