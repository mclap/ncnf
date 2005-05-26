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
#ifndef	__NCNF_CR_H__
#define	__NCNF_CR_H__

#include "ncnf.h"

/*
 * Read the configuration file.
 * Returns 0 if all OK, -1 if file open error or 1 if parse error.
 */

int _ncnf_cr_read(const char *cfname, enum ncnf_source_type,
	struct ncnf_obj_s **root, int relaxed_namespace);


/*
 * Resolve indirect references and expand insertions.
 * Returns -1 if some errors or 0 if all OK.
 */
int _ncnf_cr_resolve(struct ncnf_obj_s *root, int relaxed_namespace);


/*
 * Low-level function to resolve one reference.
 * Used in ncnf_diff().
 * func used to optionally invoke user-defined function
 * when reference is found and processed.
 */
int _ncnf_cr_resolve_references(struct ncnf_obj_s *root,
	int (*func)(struct ncnf_obj_s *ref, int invocation));

#endif /* __NCNF_CR_H__ */
