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
 * Validation engine.
 */
#ifndef	__NCNF_VR_H__
#define	__NCNF_VR_H__

#include "headers.h"

enum _vr_tokens {
	VR_GLOBAL	= 0,
	VR_ENTITY	= 1,
	VR_ENTITY_TYPE	= 2,
	VR_ENTITY_NAME	= 3,
	VR_TYPE		= 1,
	VR_TYPE_NAME	= 2,
	VR_TYPE_TYPE	= 3,
	VR_TYPE_VALUE	= 4,
	VR_RULE_MO	= 1,
	VR_RULE_SM	= 2,
	VR_RULE_EA	= 3,
	VR_RULE_NAME	= 4,
	VR_RULE_RANGE	= 5,
	VR_RULE_RANGE_VALUE	= 6,
	VR_RULE_REGEX	= 5,
	VR_RULE_REGEX_VALUE	= 6,
	VR_RULE_TYPE	= 5,
	VR_RULE_TYPE_NAME	= 6,
	VR_TOKENS_MAX	= 7
};

#define	VR_STR_ENTITY	"entity"
#define	VR_STR_TYPE	"type"
#define	VR_STR_REGEX	"regex"
#define	VR_STR_SINGLE	"single"
#define	VR_STR_MULTIPLE	"multiple"
#define	VR_STR_MANDATORY	"mandatory"
#define	VR_STR_OPTIONAL	"optional"
#define	VR_STR_ATTRIBUTE	"attribute"
#define	VR_STR_REFERENCE	"reference"
#define	VR_STR_ATTACH	"attach"
#define	VR_STR_RANGE	"range"
#define	VR_STR_IP	"ip"
#define	VR_STR_IP_MASK	"ip_mask"
#define	VR_STR_IP_MASKLEN	"ip_masklen"
#define	VR_STR_IP_PORT	"ip_port"
#define	VR_STR_VALIDATOR_ENTITY	"_validator-entity"

enum vr_obj_class {
	VR_CLASS_ATTRIBUTE	= 0,
	VR_CLASS_ENTITY		= 1,
	VR_CLASS_REFERENCE	= 2,
	VR_CLASS_ATTACHMENT	= 3,
};

/*
 * Type, i.e.: range, regex, and others.
 */
struct vr_type {
	char *name;
	int standalone;	/* Not in the hash */

	char *regex;
#ifdef	HAVE_LIBSTRFUNC
	sed_t *regex_compiled;
#endif	/* HAVE_LIBSTRFUNC */

	int range_defined;
	double range_start;
	double range_end;

	int ip_required;
	int ip_mask_required;
	int ip_masklen_required;
	int ip_port_required;
};

/*
 * Basic rule line:
 */
struct vr_rule {
	int mandatory;
	int multiple;
	enum vr_obj_class vr_obj_class;

	char *name;	/* Destination object name */

	int _entity_reference;

	struct vr_type *type;

	struct vr_rule *next;
};

/*
 * Entity containing ruleset:
 */
struct vr_entity {
	char *type;
	char *name;
	int already_here;
	struct vr_rule *rules;
};

/*
 * The whole configuration.
 */
struct vr_config {
	genhash_t *types;
	genhash_t *entities;
};

struct vr_config *ncnf_vr_read(const char *filename);
int ncnf_validate(struct ncnf_obj_s *, struct vr_config *);
void ncnf_vr_destroy(struct vr_config *);

struct vr_entity *_vr_get_entity(struct vr_config *vc, char *name, char *type, int create);
struct vr_type *_vr_add_type(struct vr_config *vc, char *name,
	char *type, char *value, int line);
void _vr_destroy_type(void *vr);
int _vr_add_rule(int line, struct vr_config *vc, struct vr_entity *e,
	char *mo, char *sm, char *ea, char *name, char *a, char *v);

#endif /* __NCNF_VR_H__ */
