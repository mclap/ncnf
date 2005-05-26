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
#include "ncnf_policy.h"

/*

Policy 1.

"Every entity's full path name (starting at bottom level
and working up, with @ as a separator) must be unique from any other
entity (not just any other entity of the same type). Uniqueness testing
should be case insensitive and ignore all punctuation. Even though ID's
are oficially case sensitive and includes all characters, validating
against the stricter rules minimises the danger of confusion".

This basically means that all entity names at the current level should
be different case-and-punctuation-insensitive.

also

"Individual entity names must pass the following regular
expression [a-zA-Z0-9.-]*".

And be sure that domain names (values of a "domain" entities
and certain other) are the special case.

 */

NCNF_POLICY(1, "1. Entity length, uniqueness and character set") {
	struct ncnf_obj_s *iter;
	struct ncnf_obj_s *obj;
	ncnf_sf_svect *sv;
	int buf_size = -1;
	char *buf = NULL;
	int ret = -1;

	/* THIS IS JUST AN EXAMPLE! SUBSTITUTE IT WITH YOUR OWN POLICY! */
	return 0;

	sv = ncnf_sf_sinit();
	if(sv == NULL)
		return -1;

	iter = _ncnf_get_obj(root, NULL, NULL,
		NCNF_CHAIN_OBJECTS, _NGF_NOFLAGS);
	if(iter == NULL) {
		ncnf_sf_sfree(sv);
		return 0;
	}

	while((obj = _ncnf_iter_next(iter))) {
		int len;
		char *p;
		char *c;

		assert(obj->obj_class != NOBJ_INVALID);

		/* Should we fail, the line will be our exit code */
		ret = obj->config_line;

		/*
		 * Checking the size of a name.
		 */
		len = strlen(obj->value);

		if(buf_size <= len) {
			buf_size = len + 1;
			buf = alloca(buf_size);
		}

		for(p = buf, c = obj->value; *c; c++) {

			if(
				(*c >= 'A' && *c <= 'Z')
				||
				(*c >= 'a' && *c <= 'z')
				||
				(*c >= '0' && *c <= '9')
				||
				(*c == '.' || *c == '-')
			) {
				/* Ignore punctuation */
				if(*c != '.' && *c != '-') {
					if(*c >= 'A' && *c <= 'Z')
						*p++ = (*c) + 0x20;
					else
						*p++ = *c;
				}
			} else {
				_ncnf_debug_print(1,
					"Wrong name \"%s\": "
					"invalid character set",
					obj->value
				);
				goto fail;
			}
		}
		*p = '\0';

		/* Check presense */
		if(ncnf_sf_sfind(sv, buf) != -1) {
			_ncnf_debug_print(1,
				"Wrong name \"%s\" (\"%s\"): %s "
				"is not unique",
				obj->value,
				buf, ncnf_sf_sjoin(sv, ", ")
			);
			goto fail;
		}

		/* Add if not already present */
		if(ncnf_sf_sadd(sv, buf) == -1) {
			ret = -1;
			goto fail;
		}
	}

	ncnf_sf_sfree(sv);

	/*
	 * Do the same thing down the tree.
	 */
	if(root->obj_class == NOBJ_ROOT || root->obj_class == NOBJ_COMPLEX) {
		collection_t *coll;
		int i;

		coll = &root->m_collection[COLLECTION_OBJECTS];
		for(i = 0; i < coll->entries; i++) {
			obj = coll->entry[i].object;
			assert(obj->obj_class != NOBJ_INVALID);
			if(obj->obj_class != NOBJ_REFERENCE) {
				ret = policy1(obj);
				if(ret) return ret;
			}
		}
	}


	return 0;

fail:

	if(ret == 0) {
		errno = EINVAL;
		ret = -1;
	}

	ncnf_sf_sfree(sv);
	return ret;
}


