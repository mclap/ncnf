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
 * Internal declarations.
 */
#ifndef	__NCNF_INT_H__
#define	__NCNF_INT_H__

#include <bstr.h>

#include "ncnf_coll.h"
#include "ncnf_notif.h"
#include "ncnf_walk.h"
#include "ncnf_diff.h"

enum obj_class {
	NOBJ_INVALID	= 0,	/* INVALID */
	NOBJ_ROOT	= 1,	/* Root */
	NOBJ_COMPLEX	= 2,	/* Complex object */
	NOBJ_ATTRIBUTE	= 3,	/* Plain attribute */
	NOBJ_INSERTION	= 4,	/* Content insertion */
	NOBJ_REFERENCE	= 5,	/* Indirect object reference */
	NOBJ_ITERATOR	= 6,	/* Iterator */
	NOBJ_LAZY_NOTIF	= 7,	/* Lazy notification functions holder */
};
#define	_NOBJ_CONTAINER(obj)	((obj)->obj_class <= NOBJ_COMPLEX)

enum collections_e {
	COLLECTION_ATTRIBUTES	= 0,
	COLLECTION_OBJECTS	= 1,
	COLLECTION_INSERTS	= 2,
	COLLECTION_LAZY_NOTIF	= 3,
	MAX_COLLECTIONS		= 4,
};

struct ncnf_obj_s {
	/*
	 * Common header
	 */
	enum obj_class obj_class;

	bstr_t type;
	bstr_t value;

	struct ncnf_obj_s *parent;
	int config_line;

	/* For building run-time chains */
	struct ncnf_obj_s *chain_next;
	struct ncnf_obj_s *chain_cur;

	/*
	 * User callbacks and data
	 */
	int  (*notify)(ncnf_obj *, enum ncnf_notify_event, void *notify_key);
	void *notify_key;
	void *user_data;

	/****************************
	* Class-specific properties *
	****************************/

	union {
		struct {
			/*
			 * Properties for NOBJ_COMPLEX and NOBJ_ROOT
			 */
			collection_t collection[MAX_COLLECTIONS];
		} property_CONTAINER;
		struct {
			int attr_flags;	/* &1 = not resolved */
		} property_ATTRIBUTE;
		struct {
			/*
			 * Properties for NOBJ_ITERATOR
			 */
			collection_t iterator_collection;
			int iterator_position;
		} property_ITERATOR;
		struct {
			/*
			 * Properties for NOBJ_REFERENCE
			 */
			bstr_t ref_type;
			bstr_t ref_value;
			int ref_flags;	/* &1 = Dependent */

			bstr_t new_ref_type;
			bstr_t new_ref_value;

			/*
			 * Configuration diffing of NOBJ_REFERENCE
			 * requires special handling to preserve links
			 */
			struct ncnf_obj_s *direct_reference;
		} property_REFERENCE;
		struct {
			/*
			 * Properties for NOBJ_INSERTION
			 */
			int insert_flags;	/* &1 = inheritance */
		} property_INSERTION;
	} un;
#define	m_collection	un.property_CONTAINER.collection
#define	m_attr_flags	un.property_ATTRIBUTE.attr_flags
#define	m_iterator_collection	un.property_ITERATOR.iterator_collection
#define	m_iterator_position	un.property_ITERATOR.iterator_position
#define	m_ref_type	un.property_REFERENCE.ref_type
#define	m_ref_value	un.property_REFERENCE.ref_value
#define	m_ref_flags	un.property_REFERENCE.ref_flags
#define	m_new_ref_type	un.property_REFERENCE.new_ref_type
#define	m_new_ref_value	un.property_REFERENCE.new_ref_value
#define	m_direct_reference	un.property_REFERENCE.direct_reference
#define	m_insert_flags	un.property_INSERTION.insert_flags

	/*
	 * Auxiliary temporary variables.
	 */

	int mark;	/* Sometimes we need to mark object somehow */

	int uses;	/* Someone relies on this. Used by ncnf-strip */

	void *mr;	/* Allocated in this memory region (optional) */
};

#include "ncnf_constr.h"


/* Recursively dump the object tree */
void _ncnf_obj_dump_recursive(FILE *f, struct ncnf_obj_s *obj, const char *flatten_type, int marked_only, int verbose, int indent, int indent_shift, int single_level, int *recursive_size);

void _ncnf_debug_print(int, const char *, ...)
	__attribute__ ((format (printf, 2, 3) ));


#endif	/* __NCNF_INT_H__ */
