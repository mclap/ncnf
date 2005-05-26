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
#include "ncnf_app.h"

#define	_INSIDE_POLICY_C
#include "ncnf_policy.h"

/*
 * Iterate trough defined policies.
 * For information on how to add a policy, see the ncnf_policy.h file.
 */
int
ncnf_policy(struct ncnf_obj_s *root) {
	char policy_disable_attr_name[64];
	policy_descriptor_t *pd;
	int i;
	int failures_found = 0;
	int errno_of_last_failure = 0;
	int policies = sizeof(policy_descriptors) / sizeof(pd);

	for(i = 0; i < policies; i++) {
		int line;

		pd = policy_descriptors[i];

		snprintf(policy_disable_attr_name,
			sizeof(policy_disable_attr_name),
			"_validator-policy-%d-disable", i + 1);

		if(ncnf_get_obj(root, policy_disable_attr_name, "yes",
				NCNF_FIRST_ATTRIBUTE)) {
			_ncnf_debug_print(0,
				"Validator policy %d disabled on request",
				i + 1);
			continue;
		}

		line = pd->policy_function(root);
		if(line == 0) {
			continue;
		} else if(line > 0) {
			_ncnf_debug_print(1,
				"Configuration policy \"%s\" failed at line %d",
				pd->policy_description, line);
			errno = EINVAL;
		} else if(line < 0) {
			_ncnf_debug_print(1,
				"Configuration policy \"%s\" failed",
				pd->policy_description);
		}

		failures_found = 1;
		errno_of_last_failure = errno;
	}

	if(failures_found) {
		errno = errno_of_last_failure;
		return -1;
	}

	return 0;
}
