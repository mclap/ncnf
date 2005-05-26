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

/*
 * Forward declarations
 */
static int _ncnf_vr_validate(struct vr_config *vc, struct ncnf_obj_s *obj);
static int _vr_check_entity(struct vr_config *vc, struct ncnf_obj_s *obj, struct vr_entity *e, int check_results);
static int _vr_check_rule(struct vr_config *vc, struct ncnf_obj_s *obj, struct vr_rule *rule);

static char *
__vr_obj_class2string(enum vr_obj_class vr_obj_class) {
	switch(vr_obj_class) {
	case VR_CLASS_ATTRIBUTE:
		return "attribute";
	case VR_CLASS_ENTITY:
		return "entity";
	case VR_CLASS_REFERENCE:
		return "reference";
	case VR_CLASS_ATTACHMENT:
		return "attachment";
	}

	return "UNKNOWN";
}

static collection_t *
__vr_collection_by_obj_class(struct ncnf_obj_s *obj, enum vr_obj_class vr_obj_class) {
	int idx;

	if(obj->obj_class != NOBJ_ROOT && obj->obj_class != NOBJ_COMPLEX)
		return NULL;

	switch(vr_obj_class) {
	case VR_CLASS_ATTRIBUTE:
		idx = COLLECTION_ATTRIBUTES;
		break;
	case VR_CLASS_ENTITY:
	case VR_CLASS_REFERENCE:
	case VR_CLASS_ATTACHMENT:
		idx = COLLECTION_OBJECTS;
		break;
	default:
		assert(0);
		return NULL;
	}

	return &obj->m_collection[idx];
}

void
ncnf_vr_destroy(struct vr_config *vc) {
	if(vc) {
		if(vc->entities)
			genhash_destroy(vc->entities);
		if(vc->types)
			genhash_destroy(vc->types);
		free(vc);
	}
}


int
ncnf_validate(struct ncnf_obj_s *obj, struct vr_config *vc) {

	if(obj == NULL || vc == NULL) {
		errno = EINVAL;
		return -1;
	}

	if(_ncnf_vr_validate(vc, obj)) {
		return -1;
	}

	return 0;
}


static int
_ncnf_vr_validate(struct vr_config *vc, struct ncnf_obj_s *obj) {
	struct vr_entity *e;
	int i;

	assert(vc && obj);

	/*
	 * Checking root object.
	 */
	if(obj->obj_class == NOBJ_ROOT) {

		e = _vr_get_entity(vc, "ROOT", NULL, 0);
		if(e == NULL) {
			/* Unknown entity */
			return 0;
		}
	
		if(_vr_check_entity(vc, obj, e, 1))
			return -1;
	} else if(obj->obj_class == NOBJ_COMPLEX) {

		e = _vr_get_entity(vc, obj->type, obj->value, 0);
		if(e == NULL) {
			/* Unknown entity */
			return 0;
		}
	} else {
		return 0;
	}

	/*
	 * Checking descendent objects.
	 */

	for(i = 0; i < obj->m_collection[COLLECTION_OBJECTS].entries; i++) {
		struct ncnf_obj_s *next_obj
			= obj->m_collection[COLLECTION_OBJECTS].entry[i].object;

		e = _vr_get_entity(vc, next_obj->type, next_obj->value, 0);
		if(e == NULL) {
			/* Dont touch unknown entity */
			continue;
		}

		if(_vr_check_entity(vc, next_obj, e, 1))
			return -1;

		/*
		 * Recursively go down the tree.
		 */
		if(_ncnf_vr_validate(vc, next_obj))
			return -1;

	}



	return 0;
}


static int
_vr_check_entity(struct vr_config *vc, struct ncnf_obj_s *obj, struct vr_entity *e, int check_results) {
	collection_t *coll;
	struct vr_rule *rule;
	int i;

	assert(vc && obj && e);

	if(e->already_here)
		return 0;
	else
		e->already_here = 1;

	for(rule = e->rules; rule; rule = rule->next) {
		if(_vr_check_rule(vc, obj, rule))
			break;
	}

	e->already_here = 0;

	if(rule)
		return -1;

	if(check_results == 0)
		return 0;

	if(e->rules == NULL)
		/* Allow everything by default */
		return 0;

	if(obj->obj_class != NOBJ_ROOT && obj->obj_class != NOBJ_COMPLEX)
		return 0;

	/*
	 * Checking if everything is marked as checked.
	 */

	coll = &obj->m_collection[COLLECTION_OBJECTS];
	for(i = 0; i < coll->entries; i++) {
		struct ncnf_obj_s *o;
		o = coll->entry[i].object;
		if(o->mark == 0) {
			_ncnf_debug_print(1, "Object `%s \"%s\"' at line %d used in `%s \"%s\"` at line %d is not mentioned in ruleset for entity `%s%s%s%s'",
				o->type, o->value,
				o->config_line,
				obj->type,
				obj->value,
				obj->config_line,
				e->type,
				e->name?" \"":"",
				e->name?e->name:"",
				e->name?"\"":""
			);
			return -1;
		}
	}

	coll = &obj->m_collection[COLLECTION_ATTRIBUTES];
	for(i = 0; i < coll->entries; i++) {
		struct ncnf_obj_s *o;
		o = coll->entry[i].object;
		if(o->mark == 0) {
			_ncnf_debug_print(1, "Attribute `%s \"%s\"' at line %d is not mentioned in ruleset for entity `%s%s%s%s'",
				o->type,
				o->value,
				o->config_line,
				e->type,
				e->name?" \"":"",
				e->name?e->name:"",
				e->name?"\"":""
			);
			return -1;
		}
	}

	return 0;
}

static int
_vr_check_rule(struct vr_config *vc, struct ncnf_obj_s *obj, struct vr_rule *rule) {
	collection_t *coll;
	int count = 0;
	int rule_name_star = 0;
	int i;

	assert(vc && obj && rule);

	coll = __vr_collection_by_obj_class(obj, rule->vr_obj_class);
	if(coll == NULL)
		return 0;

	rule_name_star = strcmp(rule->name, "*")?0:1;

	for(i = 0; i < coll->entries; i++) {
		struct ncnf_obj_s *found = coll->entry[i].object;
		struct vr_type *ty;
		char *value;

		if(!rule_name_star && strcmp(found->type, rule->name))
			continue;

		count++;

		if(rule->vr_obj_class == VR_CLASS_REFERENCE
		|| rule->vr_obj_class == VR_CLASS_ATTACHMENT) {
			if(found->obj_class != NOBJ_REFERENCE) {
				_ncnf_debug_print(1,
					"Reference requested, "
					"but object is not a reference "
					"at line %d",
					found->config_line
				);
				return -1;
			}

			if(rule->vr_obj_class == VR_CLASS_ATTACHMENT) {
				if((found->m_ref_flags & 1) == 0) {
					_ncnf_debug_print(1,
						"Attachment requested "
						"but plain reference found "
						"at line %d",
						found->config_line
					);
					return -1;
				}
			}
		}

		if(found->mark == 1) {
			/* Already checked */
			continue;
		}

		if((rule->multiple == 0) && (count > 1)) {
			_ncnf_debug_print(1, "Single %s %s required, multiple found at line %d",
				__vr_obj_class2string(rule->vr_obj_class),
				rule->name,
				found->config_line
			);
			return -1;
		}

		if(found->mark == 0)
			found->mark = 1;

		if(rule->type == NULL)
			goto skip_type_checks;
		ty = rule->type;

		value = found->value;
		if(value == NULL) value = "";

		if(ty->range_defined) {
			double val = atof(value);
			if(val < ty->range_start
			  || val > ty->range_end) {
				_ncnf_debug_print(1, "Value \"%s\" at line %d does not fit in defined range (%.3f - %.3f)",
					value,
					found->config_line,
					ty->range_start,
					ty->range_end);
				return -1;
			}
		}

		if(ty->regex) {
#ifdef	HAVE_LIBSTRFUNC
			char *results;
			results = sed_exec(ty->regex_compiled, value);
			if(results == NULL) {
				_ncnf_debug_print(1, "Value \"%s\" at line %d does not match regular expression \"%s\"",
					value,
					found->config_line,
					ty->regex);
				return -1;
			}
			if(ty->regex[0] == 's' || ty->regex[0] == 'y') {
				found->mark = 2;
				if(strcmp(value, results)) {
					bstr_t b;
					b = str2bstr(results, -1);
					if(b == NULL) {
						_ncnf_debug_print(1,
						"Memory allocation failed");
						return -1;
					}
					bstr_free(value);
					found->value = b;
				}
			}
#else	/* !HAVE_LIBSTRFUNC */
			assert(!ty->regex);
#endif	/* HAVE_LIBSTRFUNC */
		}

		if(ty->ip_required) {
			struct in_addr ip;
			if(inet_aton(value, &ip) != 1) {
				_ncnf_debug_print(1, "Value \"%s\" at line %d is not an IP address",
					value,
					found->config_line);
				return -1;
			}
		}

		if(ty->ip_mask_required) {
#ifdef	HAVE_LIBSTRFUNC
			unsigned int ip, mask;
			if(strchr(value, ' ')
				|| split_network(value, &ip, &mask)
			) {
				_ncnf_debug_print(1, "Value \"%s\" at line %d is not an IP address/Mask",
					value,
					found->config_line);
				return -1;
			}
#else
			assert(!ty->ip_mask_required);
#endif	/* HAVE_LIBSTRFUNC */
		}

		if(ty->ip_masklen_required) {
#ifdef	HAVE_LIBSTRFUNC
			char *p;
			unsigned int ip, mask;
			if((p = strchr(value, ' '))
				|| !(p = strchr(value, '/'))
				|| strlen(p) > 3
				|| split_network(value, &ip, &mask)
			) {
				_ncnf_debug_print(1, "Value \"%s\" at line %d is not an IP address/Masklen",
					value,
					found->config_line);
				return -1;
			}
#else
			assert(!ty->ip_masklen_required);
#endif	/* HAVE_LIBSTRFUNC */
		}

		if(ty->ip_mask_required) {
			char *p;
			struct in_addr ip;

			if( (p = strchr(value, ':')) == NULL
				|| atoi(p+1) == 0
				|| (*p = '\0' && 0) /* Mask ':' */
				|| inet_aton(value, &ip) != 1
			) {
				if(p) *p = ':';	/* Unmask ':' */
				_ncnf_debug_print(1, "Value \"%s\" at line %d is not an ip:port",
					value,
					found->config_line);
				return -1;
			}
			if(p) *p = ':';	/* Unmask ':' */
		}

	skip_type_checks:

		if(rule->_entity_reference) {
			char *e_type, *e_name;
			struct vr_entity *e;

			e_type = alloca(strlen(found->value) + 1);
			strcpy(e_type, found->value);
			e_name = strchr(e_type, ':');
			if(e_name)
				*e_name++ = '\0';


			e = _vr_get_entity(vc, e_type, e_name, 0);
			if(e == NULL) {
				_ncnf_debug_print(1,
				"Reference to the unknown validation entity %s at line %d",
					e_type,
					found->config_line
				);
				return -1;
			}

			if(_vr_check_entity(vc, obj, e, 0)) {
				return -1;
			}

		}


	}

	if((rule->mandatory != 0) && (count == 0)) {
		_ncnf_debug_print(1,
			"Mandatory %s %s missing in entity `%s \"%s\"' at line %d",
			__vr_obj_class2string(rule->vr_obj_class),
			rule->name,
			obj->type?obj->type:"ROOT",
			obj->value?obj->value:"<unnamed>",
			obj->config_line
		);
		return -1;
	}



	return 0;
}


