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
 * Constructors and destructors.
 */
#ifndef	__NCNF_CONSTR_H__
#define	__NCNF_CONSTR_H__

#include "ncnf_int.h"

/*
 * Basic constructor for the configuration object.
 */
struct ncnf_obj_s *_ncnf_obj_new(void *ignore, enum obj_class,
	const bstr_t _type, const bstr_t _name, int _config_line);

/*
 * Universal destructor of any type of configuration object.
 */
void _ncnf_obj_destroy(struct ncnf_obj_s *);

/*
 * Insert an object into an object.
 */
int _ncnf_attach_obj(struct ncnf_obj_s *to, struct ncnf_obj_s *what, int relaxed_ns);

/*
 * Create a full copy of the given object.
 */
struct ncnf_obj_s *_ncnf_obj_clone(void *ignore, struct ncnf_obj_s *root);

#endif	/* __NCNF_CONSTR_H__ */
