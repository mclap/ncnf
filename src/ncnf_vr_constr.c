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

static void _vr_entity_free(void *);
static void _vr_rule_free(void *);

static int _vr_entity_cmpf(const void *ap, const void *bp);
static int _vr_entity_hashf(const void *ap);

static int
_vr_entity_cmpf(const void *ap, const void *bp) {
	const struct vr_entity *a = ap;
	const struct vr_entity *b = bp;

	if(strcmp(a->type, b->type))
		return -1;

	if(a->name && b->name) {
		if(strcmp(a->name, b->name) == 0)
			return 0;
		else
			return -1;
	}

	if(a->name || b->name)
		return -1;

	return 0;
}

static int
_vr_entity_hashf(const void *ap) {
	const struct vr_entity *a = ap;
	return hashf_string(a->type);
}

static void
_vr_entity_free(void *p) {
	struct vr_entity *e = (struct vr_entity *)p;
	struct vr_rule *rule;

	if(e->type) {
		free(e->type);
		e->type = NULL;
	}
	if(e->name) {
		free(e->name);
		e->name = NULL;
	}

	while(e->rules) {
		rule = e->rules;
		e->rules = rule->next;
		_vr_rule_free(rule);
	}

	free(e);
}

static void
_vr_rule_free(void *p) {
	struct vr_rule *r = (struct vr_rule *)p;
	if(r) {
		if(r->name) {
			free(r->name);
			r->name = NULL;
		}
		if(r->type && r->type->standalone) {
			_vr_destroy_type(r->type);
		}
		free(r);
	}
}

void
_vr_destroy_type(void *p) {
	struct vr_type *ty = (struct vr_type *)p;
	if(ty) {
		if(ty->name) {
			free(ty->name);
			ty->name = NULL;
		}
		if(ty->regex) {
			free(ty->regex);
			ty->regex = NULL;
		}
#ifdef	HAVE_LIBSTRFUNC
		sed_free(ty->regex_compiled);
#endif	/* HAVE_LIBSTRFUNC */
		free(ty);
	}
}

struct vr_entity *
_vr_get_entity(struct vr_config *vc, char *type, char *name, int create) {
	struct vr_entity static_entity;
	struct vr_entity *e;

	if(vc->entities) {
		static_entity.type = type;	
		static_entity.name = name;

		e = genhash_get(vc->entities, &static_entity);
		if(e) return e;

		if(name && create == 0) {
			static_entity.name = NULL;

			e = genhash_get(vc->entities, &static_entity);
			if(e) return e;
		}

		if(create == 0)
			return NULL;
		e = (struct vr_entity *)calloc(1, sizeof(struct vr_entity));
		if(e == NULL)
			return NULL;

		e->type = strdup(type);
		if(e->type == NULL) {
			_vr_entity_free(e);
			return NULL;
		}

		if(name) {
			e->name = strdup(name);
			if(e->name == NULL) {
				_vr_entity_free(e);
				return NULL;
			}
		}

		if(genhash_addunique(vc->entities, e, e)) {
			_vr_entity_free(e);
			return NULL;
		}
	} else {
		if(create == 0)
			return NULL;

		vc->entities = genhash_new(
			_vr_entity_cmpf,
			_vr_entity_hashf,
			NULL,
			_vr_entity_free);
		if(vc->entities)
			return _vr_get_entity(vc, type, name, create);
		else
			return NULL;
	}
	return e;
}

struct vr_type * _vr_new_type(struct vr_config *vc, char *name, char *type, char *value, int line);
struct vr_type *
_vr_new_type(struct vr_config *vc, char *name, char *type, char *value, int line) {
	struct vr_type *ty;

	ty = (struct vr_type *)calloc(1, sizeof(struct vr_type));
	if(ty == NULL)
		return NULL;

	ty->name = strdup(name);
	if(ty->name == NULL) {
		_vr_destroy_type(ty);
		return NULL;
	}


	if(strcmp(type, VR_STR_TYPE) == 0) {
		_vr_destroy_type(ty);
		ty = NULL;
		if(vc->types)
			ty = genhash_get(vc->types, name);
		if(ty == NULL) {
			_ncnf_debug_print(1, "Can't find type \"%s\" for rule at line %d", value, line);
			return NULL;
		} else {
			return ty;
		}
	} else if(strcmp(type, VR_STR_REGEX) == 0) {
#ifdef	HAVE_LIBSTRFUNC
		ty->regex = strdup(value);
		if(ty->regex == NULL) {
			_vr_destroy_type(ty);
			return NULL;
		}
		ty->regex_compiled = sed_compile(value);
		if(ty->regex_compiled == NULL) {
			_ncnf_debug_print(1,
			"Invalid regular expression \"%s\" at line %d",
				value, line);

			_vr_destroy_type(ty);
			return NULL;
		}
#else	/* !HAVE_LIBSTRFUNC */
		_ncnf_debug_print(1, "\"%s\" at line %d not supported: libstrfunc is not compiled in", type, line);
		_vr_destroy_type(ty);
		return NULL;
#endif	/* HAVE_LIBSTRFUNC */
	} else if(strcmp(type, VR_STR_RANGE) == 0) {
		char *p = strchr(value, ':');
		if(p == NULL) {
			_ncnf_debug_print(1, "Range should be specified as min:max at line %d", line);
			_vr_destroy_type(ty);
			return NULL;
		}
		*p++ = '\0';
		ty->range_defined = 1;
		ty->range_start = atof(value);
		ty->range_end = atof(p);
	} else if(strcmp(type, VR_STR_IP) == 0) {
		ty->ip_required = 1;
	} else if(strcmp(type, VR_STR_IP_MASK) == 0) {
#ifdef	HAVE_LIBSTRFUNC
		ty->ip_mask_required = 1;
#else
		_ncnf_debug_print(1, "\"%s\" at line %d not supported: libstrfunc is not compiled in", type, line);
		_vr_destroy_type(ty);
		return NULL;
#endif
	} else if(strcmp(type, VR_STR_IP_MASKLEN) == 0) {
#ifdef	HAVE_LIBSTRFUNC
		ty->ip_masklen_required = 1;
#else
		_ncnf_debug_print(1, "\"%s\" at line %d not supported: libstrfunc is not compiled in", type, line);
		_vr_destroy_type(ty);
		return NULL;
#endif
	} else if(strcmp(type, VR_STR_IP_PORT) == 0) {
		ty->ip_port_required = 1;
	} else {
		_ncnf_debug_print(1, "Unknown type: \"%s\" at line %d", type, line);
		_vr_destroy_type(ty);
		return NULL;
	}

	ty->standalone = 1;

	return ty;
}

struct vr_type *
_vr_add_type(struct vr_config *vc, char *name, char *type, char *value, int line) {
	struct vr_type *ty;

	if(vc->types == NULL) {
		vc->types = genhash_new(
			cmpf_string, hashf_string,
			NULL, _vr_destroy_type);
		if(vc->types == NULL)
			return NULL;
	}

	ty = _vr_new_type(vc, name ? name : value, type, value, line);
	if(ty == NULL)
		return NULL;

	if(name == NULL)
		return ty;

	if(genhash_addunique(vc->types, ty->name, ty)) {
		_vr_destroy_type(ty);
		return NULL;
	} else {
		ty->standalone = 0;
	}

	return ty;
}


int
_vr_add_rule(int line, struct vr_config *vc, struct vr_entity *e,
	char *mo, char *sm, char *ea, char *name, char *a, char *v) {
	struct vr_rule *r, *prev;

	r = (struct vr_rule *)calloc(1, sizeof(struct vr_rule));
	if(r == NULL) {
		_ncnf_debug_print(1, "Memory allocation error");
		return -1;
	}

	/* mandatory | optional */
	if(strcmp(mo, VR_STR_MANDATORY) == 0) {
		r->mandatory = 1;
	} else if(strcmp(mo, VR_STR_OPTIONAL) == 0) {
		r->mandatory = 0;
	} else {
		_ncnf_debug_print(1, "%s or %s token expected at line %d",
			VR_STR_MANDATORY, VR_STR_OPTIONAL, line);
		goto fail;
	}

	/* single | multiple */
	if(strcmp(sm, VR_STR_SINGLE) == 0) {
		r->multiple = 0;
	} else if(strcmp(sm, VR_STR_MULTIPLE) == 0) {
		r->multiple = 1;
	} else {
		_ncnf_debug_print(1, "%s or %s token expected at line %d",
			VR_STR_SINGLE, VR_STR_MULTIPLE, line);
		goto fail;
	}

	/* entity | attribute | ... */
	if(strcmp(ea, VR_STR_ATTRIBUTE) == 0) {
		r->vr_obj_class = VR_CLASS_ATTRIBUTE;
	} else if(strcmp(ea, VR_STR_ENTITY) == 0) {
		r->vr_obj_class = VR_CLASS_ENTITY;
	} else if(strcmp(ea, VR_STR_REFERENCE) == 0) {
		r->vr_obj_class = VR_CLASS_REFERENCE;
	} else if(strcmp(ea, VR_STR_ATTACH) == 0) {
		r->vr_obj_class = VR_CLASS_ATTACHMENT;
	} else {
		_ncnf_debug_print(1, "%s or %s token expected at line %d",
			VR_STR_ENTITY, VR_STR_ATTRIBUTE, line);
		goto fail;
	}

	r->name = strdup(name);
	if(r->name == NULL)
		goto fail;

	if(strcmp(r->name, VR_STR_VALIDATOR_ENTITY) == 0)
		r->_entity_reference = 1;

	if(a) {
		r->type = _vr_add_type(vc, NULL, a, v, line);
		if(r->type == NULL)
			goto fail;
	}

	prev = e->rules;
	while(prev && prev->next)
		prev = prev->next;

	if(prev)
		prev->next = r;
	else
		e->rules = r;

	return 0;
fail:
	_vr_rule_free(r);

	return -1;
}
