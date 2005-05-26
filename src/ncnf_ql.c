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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "headers.h"

#include "asn_SET_OF.h"

#include "ncnf.h"
#include "ncnf_int.h"
#include "ncnf_ql.h"

#define	QERROR(fmt, args...)	do {				\
	if(nq) ncnf_delete_query(nq);				\
	if(errbuf && errlen) {					\
		ssize_t _ret;					\
		assert(*errlen > 0);				\
		_ret = snprintf(errbuf, *errlen,		\
			"%s(): " fmt, __func__, ##args);	\
		assert(_ret >= 0);				\
		*errlen = _ret;					\
	}							\
	return NULL;						\
} while(0)

/*
 * Required attribute with the value specification.
 */
typedef struct ncnf_attrreq_s {
	char *Name;
	char *Value;
	sed_t *value_expression;/* If not given, use literal value */
} ncnf_attrreq_t;

static void ncnf_attrreq_free(ncnf_attrreq_t *ar) {
	if(ar) {
		if(ar->Name) free(ar->Name);
		if(ar->Value) free(ar->Value);
		if(ar->value_expression) sed_free(ar->value_expression);
	}
}

struct ncnf_query_s {
	ncnf_attrreq_t object_filter;	/* Filter on object name */
	A_SET_OF(ncnf_attrreq_t) required_attributes;
	A_SET_OF(sed_t) _select;/* _select "/type-name/i" [{ ... }]; */
	enum {
		NQSC_DEFAULT,	/* Semantically same as NQSC_NONE */
		NQSC_NONE,	/* No child objects are selected */
		NQSC_SINGLE,	/* Select the single level below the current */
		NQSC_ALL,	/* Select all underlying levels */
	} _select_children;
	A_SET_OF(struct ncnf_query_s) level_deeper;
};

ncnf_query_t *
ncnf_compile_query(ncnf_obj *qroot, char *errbuf, size_t *errlen) {
	ncnf_query_t *nq = NULL;
	ncnf_obj *iter, *attr, *obj;

	if(!qroot) QERROR("missing query specification, qroot=%p", qroot);

	nq = calloc(1, sizeof(*nq));
	if(nq) {
		nq->required_attributes.free = ncnf_attrreq_free;
		nq->_select.free = sed_free;
		nq->level_deeper.free = ncnf_delete_query;
	} else {
		QERROR("%s", strerror(errno));
	}

	if(ncnf_obj_type(qroot)) {
		char *type = ncnf_obj_type(qroot);
		char *value = ncnf_obj_name(qroot);

		if(strcmp(value, "*") == 0) value = "/.*/";

		nq->object_filter.Name = strdup(type);
		nq->object_filter.Value = strdup(value);
		if(!nq->object_filter.Name
		|| !nq->object_filter.Value)
			QERROR("%s", strerror(errno));

		if(*value == '/') {
			sed_t *se = sed_compile(value);
			if(!se) QERROR("Cannot compile \"%s\" "
				"at line %d: %s",
				value, ncnf_obj_line(qroot),
				strerror(errno));
			nq->object_filter.value_expression = se;
		}
	} else {
		/* This is the root, don't bother setting filters */
	}

	/*
	 * Gather all attribute value requirements.
	 */
	iter = ncnf_get_obj(qroot, NULL, NULL, NCNF_CHAIN_ATTRIBUTES);
	while((attr = ncnf_iter_next(iter))) {
		char *type = ncnf_obj_type(attr);
		char *value = ncnf_obj_name(attr);
		if(*type == '_') {
			if(strcmp(type, "_select") == 0) {
				sed_t *se;
				if(*value != '/') {
					errno = EINVAL;
					QERROR("%s \"%s\" "
					"regular expression expected "
					"at line %d",
					type, value, ncnf_obj_line(attr));
				}
				se = sed_compile(value);
				if(!se) QERROR("Cannot compile \"%s\" "
					"at line %d: %s",
					value, ncnf_obj_line(attr),
					strerror(errno));
				if(ASN_SET_ADD(&nq->_select, se)) {
					sed_free(se);
					QERROR("%s", strerror(errno));
				}
				continue;
			} else if(strcmp(type, "_select-children") == 0) {
				if(nq->_select_children) {
					errno = EINVAL;
					QERROR("_select-children is getting "
					"redefined at line %d",
						ncnf_obj_line(attr));
				}
				if(!strcmp(value, "none")) {
					nq->_select_children = NQSC_NONE;
				} else if(!strcmp(value, "one")) {
					nq->_select_children = NQSC_SINGLE;
				} else if(!strcmp(value, "all")) {
					nq->_select_children = NQSC_ALL;
				} else {
					errno = EINVAL;
					QERROR("`%s \"%s\"` at line %d: "
					"{none|one|all} expected",
					type, value, ncnf_obj_line(attr));
				}
				continue;
			}
			errno = EINVAL;
			QERROR("Unrecognized `%s \"%s\"` at line %d",
				type, value, ncnf_obj_line(attr));
		} else {
			ncnf_attrreq_t *ar;

			if(strcmp(value, "*") == 0) value = "/.*/";

			ar = calloc(1, sizeof(*ar));
			if(ASN_SET_ADD(&nq->required_attributes, ar)) {
				if(ar) free(ar);
				QERROR("%s", strerror(errno));
			}

			ar->Name = strdup(type);
			ar->Value = strdup(value);
			if(!ar->Name || !ar->Value)
				QERROR("%s", strerror(errno));

			if(*value == '/') {
				sed_t *se = sed_compile(value);
				if(!se) QERROR("Cannot compile \"%s\" "
					"at line %d: %s",
					value, ncnf_obj_line(attr),
					strerror(errno));
				ar->value_expression = se;
			} else {
				ar->value_expression = NULL;
			}
		}
	}

	/*
	 * Dig one level deeper.
	 */
	iter = ncnf_get_obj(qroot, NULL, NULL, NCNF_CHAIN_OBJECTS);
	while((obj = ncnf_iter_next(iter))) {
		ncnf_query_t *newnq = ncnf_compile_query(obj, errbuf, errlen);
		if(ASN_SET_ADD(&nq->level_deeper, newnq)) {
			if(newnq) ncnf_delete_query(newnq);
			ncnf_delete_query(nq);
			return NULL;	/* Reporting has already been done */
		}
	}

	return nq;
}

void
ncnf_delete_query(ncnf_query_t *nq) {
	if(nq) {
		if(nq->object_filter.Name) free(nq->object_filter.Name);
		if(nq->object_filter.Value) free(nq->object_filter.Value);
		if(nq->object_filter.value_expression)
			sed_free(nq->object_filter.value_expression);
		asn_set_empty(&nq->required_attributes);
		asn_set_empty(&nq->_select);
		asn_set_empty(&nq->level_deeper);
	}
}

static int set_mark_func(ncnf_obj *obj, void *markv)
	{ obj->mark = (int)markv; return 0; }

static void Mark(ncnf_obj *obj, int recurseDown) {
	if(!obj) return;

	if(!obj->mark) {
		obj->mark = 1;
		/* Mark the path to the root in the tree */
		Mark(ncnf_obj_parent(obj), 0);
		/* Mark the reference's path to the root */
		if(ncnf_obj_real(obj) != obj)
			Mark(ncnf_obj_real(obj), 0);
	}

	if(recurseDown && obj->mark != 2) {
		ncnf_obj *o;
		ncnf_obj *iter;

		if(ncnf_obj_real(obj) != obj)
			return;
		obj->mark = 2;

		iter = ncnf_get_obj(obj, NULL, NULL,
			NCNF_CHAIN_ATTRIBUTES);
		while((o = ncnf_iter_next(iter))) o->mark = 1;
		iter = ncnf_get_obj(obj, NULL, NULL,
			NCNF_ITER_OBJECTS);
		assert(iter || errno == ESRCH);
		while((o = ncnf_iter_next(iter))) Mark(o, recurseDown);
		ncnf_destroy(iter);
	}
}

void
ncnf_clear_query(ncnf_obj *qroot) {
	if(!qroot) {
		ncnf_walk_tree(qroot, set_mark_func, 0);
	}
}

#undef	DEBUG
#define	DEBUG(fmt, args...) do {		\
	if(!debug) break;			\
	fprintf(stderr, fmt "\n", ##args);	\
} while(0)

int
ncnf_exec_query(ncnf_obj *qroot, ncnf_query_t *nq, int debug) {
	ncnf_obj *iter, *obj;
	int i;

	if(!qroot || !nq) {
		errno = EINVAL;
		return -1;
	}

	DEBUG("Entering %s \"%s\"",
		ncnf_obj_type(qroot), ncnf_obj_name(qroot));

	/*
	 * Check the object filter on entry.
	 */
	if(nq->object_filter.Name) {
		char *type = ncnf_obj_type(qroot);
		char *value = ncnf_obj_name(qroot);

		DEBUG("Filtering against %s %s",
			nq->object_filter.Name, nq->object_filter.Value);

		if(strcmp(nq->object_filter.Name, type))
			/* Name filter does not match */
			return 0;

		if(nq->object_filter.value_expression) {
			if(!sed_exec(nq->object_filter.value_expression, value))
				/* Expression is not satisfied */
				return 0;
		} else {
			if(strcmp(nq->object_filter.Value, value))
				/* Value filter does not match */
				return 0;
		}
	} else {
		/* This is supposed to be a root object. */
	}

	DEBUG("Enter confirmed");


	/*
	 * Check that no required_attributes result in a mismatch.
	 */
	for(i = 0; i < nq->required_attributes.count; i++) {
		ncnf_attrreq_t *ar = nq->required_attributes.array[i];
		DEBUG("Against %s \"%s\"", ar->Name, ar->Value);
		if(ar->value_expression) {
			ncnf_obj *attr;
			iter = ncnf_get_obj(qroot, NULL, NULL,
				NCNF_CHAIN_ATTRIBUTES);
			while((attr = ncnf_iter_next(iter))) {
				char *value = ncnf_obj_name(attr);
				if(sed_exec(ar->value_expression, value))
					break;
			}
			if(!attr) {
				/* This attribute shall be present */
				return 0;
			}
		} else if(*ar->Value) {
			ncnf_obj *attr = ncnf_get_obj(qroot,
				ar->Name, ar->Value,
					NCNF_CHAIN_ATTRIBUTES);
			if(!attr) {
				/* This attribute shall be present */
				return 0;
			}
		} else {
			ncnf_obj *attr = ncnf_get_obj(qroot,
				ar->Name, NULL, NCNF_FIRST_ATTRIBUTE);
			if(attr) {
				/* This attribute should not be present */
				return 0;
			}
		}
	}

	/*
	 * Mark the attributes described by _select.
	 * NCNF entities will be selected separately.
	 */
	iter = ncnf_get_obj(qroot, NULL, NULL, NCNF_ITER_ATTRIBUTES);
	while((obj = ncnf_iter_next(iter))) {
		switch(nq->_select_children) {
		case NQSC_ALL:
		case NQSC_SINGLE:
			Mark(obj, 0);
			continue;
		default:
			break;
		}
		for(i = 0; i < nq->_select.count; i++)
			if(sed_exec(nq->_select.array[i], ncnf_obj_type(obj)))
				Mark(obj, 0);
	}

	/*
	 * Process the rest of the nesting levels.
	 */
	iter = ncnf_get_obj(qroot, NULL, NULL, NCNF_CHAIN_OBJECTS);
	while((obj = ncnf_iter_next(iter))) {
		/*
		 * Execute the _select statements.
		 */
		switch(nq->_select_children) {
		case NQSC_ALL:
		case NQSC_SINGLE:
			if(ncnf_obj_real(obj) == obj) {
				ncnf_obj *attr;
				ncnf_obj *chain;
				chain = ncnf_get_obj(obj, NULL, NULL,
					NCNF_CHAIN_ATTRIBUTES);
				DEBUG("Marking %s \"%s\"",
					ncnf_obj_type(obj), ncnf_obj_name(obj));
				/* Mark this single level, or all levels */
				Mark(obj, nq->_select_children == NQSC_ALL);
				/* Select this level's attributes */
				while((attr = ncnf_iter_next(chain)))
					Mark(attr, 0);
			} else {
				Mark(obj, 0);
			}
			break;
		default:
			DEBUG("Marking selected in %s \"%s\" against %s \"%s\"",
				ncnf_obj_type(obj),
				ncnf_obj_name(obj),
				nq->object_filter.Name,
				nq->object_filter.Value);
			for(i = 0; i < nq->_select.count; i++)
				if(sed_exec(nq->_select.array[i], ncnf_obj_type(obj)))
					Mark(obj, 0);
		}

		/*
		 * Process with deeper object levels.
		 */
		for(i = 0; i < nq->level_deeper.count; i++)
			if(ncnf_exec_query(obj, nq->level_deeper.array[i],
					debug))
				return -1;
	}

	return 0;
}

