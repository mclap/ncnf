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
#include "ncnf_vr.h"

#define	BUFSIZE	4096

struct vr_config *
ncnf_vr_read(const char *filename) {
	struct stat sb;
	struct vr_config *vc;
	FILE *f;
	char buf[VR_TOKENS_MAX][BUFSIZE];
	char *p;
	int line = 0;
	struct vr_entity *default_entity = NULL;

	if(filename == NULL) {
		errno = EINVAL;
		return NULL;
	}

	vc = (struct vr_config *)calloc(1, sizeof(struct vr_config));
	if(vc == NULL)
		return NULL;

	f = fopen(filename, "r");
	if(f == NULL)
		goto fail;

	if(fstat(fileno(f), &sb) != 0 || (sb.st_mode & S_IFMT) != S_IFREG) {
		errno = EIO;
		goto fail;
	}

	default_entity = _vr_get_entity(vc, "ROOT", NULL, 1);
	if(default_entity == NULL) {
		_ncnf_debug_print(1, "Entity allocation error");
		goto fail;
	}
	
	while((p = fgets(buf[VR_GLOBAL], BUFSIZE, f))) {
		int len = strlen(p);
		int ret;
		line++;

		if(len == 0) continue;
		if(p[len - 1] != '\n' && p[len - 1] != '\r') {
			_ncnf_debug_print(1, "Unterminated line %d in %s\n",
				line, filename);
			goto fail;
		}

		if((p = strchr(p, '#')))
			*p = '\0';

		ret = sscanf(buf[VR_GLOBAL], "%s %s %s %s %s %s",
			buf[1], buf[2], buf[3],
			buf[4], buf[5], buf[6]);
		switch(ret) {
		case -1:
			/* Empty string */
			break;
		case 0:
			/* Empty string */
			continue;
		case 2:
			if(strcmp(buf[VR_ENTITY], VR_STR_ENTITY)) {
				_ncnf_debug_print(1, "%s expected at line %d\n",
					VR_STR_ENTITY, line);
				goto fail;
			} else {
				default_entity = _vr_get_entity(vc,
					buf[VR_ENTITY_TYPE],
					NULL,
					1);
				if(default_entity == NULL) {
					_ncnf_debug_print(1,
						"Entity allocation error");
					goto fail;
				}
			}
			break;
		case 6:
		case 4:

			if(strcmp(buf[VR_TYPE], VR_STR_TYPE)) {
			  if(_vr_add_rule(line, vc, default_entity,
				buf[VR_RULE_MO],
				buf[VR_RULE_SM],
				buf[VR_RULE_EA],
				buf[VR_RULE_NAME],
				(ret==6)?buf[VR_RULE_RANGE]:NULL,
				(ret==6)?buf[VR_RULE_RANGE_VALUE]:NULL
			  )) {
				/* _vr_add_rule will report errors */
				goto fail;
			  } else {
				break;
			  }
			}

			/* Fall through */
		case 3:
			if(strcmp(buf[VR_TYPE], VR_STR_TYPE) == 0) {
				if(_vr_add_type(vc, buf[VR_TYPE_NAME],
					buf[VR_TYPE_TYPE],
					buf[VR_TYPE_VALUE], line) == NULL) {
					switch(errno) {
					case EEXIST:
						_ncnf_debug_print(1, "Type %s already defined\n", buf[VR_TYPE_NAME]);
						goto fail;
					default:
						_ncnf_debug_print(1, "System error while processing line %d: %s\n", line, strerror(errno));
						goto fail;
					}
				}
				break;
			} else if(ret == 3 && strcmp(buf[VR_TYPE],
				VR_STR_ENTITY) == 0) {
				default_entity = _vr_get_entity(vc,
					buf[VR_ENTITY_TYPE],
					buf[VR_ENTITY_NAME],
					1);
				if(default_entity == NULL) {
					_ncnf_debug_print(1,
						"Entity allocation error");
					goto fail;
				}
				break;
			}

			goto fail;
		default:
			_ncnf_debug_print(1, "Parse error at line %d in %s",
				line, filename);
			goto fail;
		}

	}

	fclose(f);

	return vc;
fail:
	if(f)
		fclose(f);
	ncnf_vr_destroy(vc);
	return NULL;
}
