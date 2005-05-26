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
 * Support for NCNF query language.
 */
#ifndef	NCNF_QL_H
#define	NCNF_QL_H

#include <ncnf.h>

typedef struct ncnf_query_s ncnf_query_t;	/* Forward declaration */

/*
 * Compile the NCNF query specified using the NCNF syntax.
 * 
 * customer "*" {
 * 	attribute-value "/&$/";
 * 
 * 	_select "/type-name/i";
 * 	_select-children "none";        // {none|one|all}
 * 
 * 	domain "/ .* /" {
 * 		my-domain "/on/i";
 * 		blah1 "/ .* /";
 * 
 * 		_select-children "none";
 * 		_select "/ngrs-equiv/";
 * 	}
 * }
 * 
 */
ncnf_query_t *ncnf_compile_query(ncnf_obj *ncnf_query,
	char *errbuf, size_t *errlen);
void ncnf_delete_query(ncnf_query_t *);

/*
 * Unmark the tree before or after query.
 */
void ncnf_clear_query(ncnf_obj *root);

/*
 * Execute the compiled NCNF query,
 * marking (coloring) the NCNF objects satisfying the query.
 * 
 * Results from multiple ncnf_exec_query() functions are combined
 * unless ncnf_clear_query() is invoked.
 */
int ncnf_exec_query(ncnf_obj *root, ncnf_query_t *query, int debug);

#endif	/* NCNF_QL_H */
