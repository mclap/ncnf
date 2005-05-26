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
 * Hardwired policy checker
 */
#ifndef	__NCNF_POLICY_H__
#define	__NCNF_POLICY_H__

#include <ncnf_app.h>

/*
 * Check the tree against hardcoded policies.
 */
int ncnf_policy(struct ncnf_obj_s *root);

typedef int (ncnf_policy_function_f)(ncnf_obj *);

typedef struct policy_descriptor_s {
	ncnf_policy_function_f *policy_function;
	char *policy_description;
} policy_descriptor_t;


#ifdef	_INSIDE_POLICY_C

extern policy_descriptor_t _ncnf_policy_1_description;

policy_descriptor_t *policy_descriptors[] = {
	&_ncnf_policy_1_description
};

#endif	/* _INSIDE_POLICY_C */


/*
 * Define a new policy inside ncnf_policy_X.c file.
 */
#define	NCNF_POLICY(number, policy_description)				\
	static ncnf_policy_function_f policy##number;			\
	policy_descriptor_t _ncnf_policy_ ## number ## _description =	\
	{ policy##number, policy_description };				\
	static int policy##number(struct ncnf_obj_s *root)


/*
 * HOW TO ADD A POLICY
 *
 * 1. Add a ncnf_policy_X.c file to the Makefile.am, where X is the first
 * unused number mentioned earlier in the this (ncnf_policy.h file, see #3);
 * remember the X value.
 * 
 * 2. Use the following command to re-create Makefile from the Makefile.am
 *	(cd ../..; automake; ./config.status)
 * 
 * 3. Add
 * 	extern policy_descriptor_t _ncnf_policy_X_description
 *    and
 *	&_ncnf_policy_X_decription
 * lines into the corresponding places inside _INSIDE_POLICY_C block
 * of the ncnf_policy.h (file you're currently viewing).
 * 
 * 4. Create the ncnf_policy_X.c file using this template,
 * (or use the provided template file ncnf_policy_X.h, not forgetting
 * to copy it to ncnf_policy_<X+1>.h to create a new template).
 * 

=== ncnf_policy_X_template.c ===
#include "headers.h"
#include "ncnf_int.h
#include "ncnf_policy.h

// Some commentary (multiline) describing the policy in full.

NCNF_POLICY(X, "X. Short description of this policy") {
	return 0;
}
=== ncnf_policy_X_template.c ===

 * 
 * In case of failure, this function should return either -1 (general
 * failure with description in errno) or positive configuration line number.
 * The configuration root will be accessible as the `root` local variable.
 * This function is defined as
 * 	int policyX(struct ncnf_obj_s *root);
 * and may be used recursively.
 * 
 * In case of any questions, refer to existing ncnf_policy_X.c files.
 *
 */


#endif	/* __NCNF_POLICY_H__ */
