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
 * Collection structure.
 */
#ifndef	__NCNF_COLL_H__
#define	__NCNF_COLL_H__

/* [Martin 12/16/04]: ncnf_int.h also includes us...
 *                    forward declaration whenever possible 
 *                    to avoid this and also less pressure on make dependency
 *                    check.
 */
/* #include "ncnf_int.h" */
struct ncnf_obj_s;

typedef struct collection_entry_s {
	struct ncnf_obj_s *object;
	int ignore_in_search;
} collection_entry;

typedef struct collection_s {
	collection_entry *entry;
	unsigned int entries;	/* Number of meaningful entries */
	unsigned int size;	/* Number of allocated entries */
} collection_t;

/*
 * How to insert or merge values?
 */
enum merge_flags {
	MERGE_NOFLAGS	= 0,
	MERGE_DUPCHECK	= 1,	/* Check for duplicates */
	MERGE_PTRCHECK	= 2,	/* Check for pointer duplicates */
	MERGE_EMPTYSRC	= 4,	/* Empty source bin (_coll_join()) */
};

/*
 * Insert new object into collection.
 */
int _ncnf_coll_insert(void *ignore, collection_t *, struct ncnf_obj_s *obj, enum merge_flags);

/*
 * Add contents of the second collection into the first one,
 * optionally assigning parent.
 * Returns -1 if any error.
 */
int _ncnf_coll_join(void *ignore, collection_t *to, collection_t *from, struct ncnf_obj_s *opt_parent, enum merge_flags);

/*
 * Search in given collection for object specified by type or value or both.
 */

enum cget_flags {
  CG_IGNORE_REFERENCES	= (1 << 0),	/* Skip references */
  CG_MARK_UNSEARCHABLE	= (1 << 1),	/* Mark found as unsearchable */
  CG_RETURN_POSITION	= (1 << 2),	/* Return stop position */
  CG_RETURN_CHAIN	= (1 << 3),	/* Chain the results */
  CG_TYPE_NOCASE	= (1 << 4),	/* Case insensitive type comparison */
  CG_NAME_NOCASE	= (1 << 5),	/* Case insensitive name comparison */
};

/*
 * Iterator may be an struct ncnf_obj_s *iterator
 */
struct ncnf_obj_s *_ncnf_coll_get(collection_t *coll,
	enum cget_flags,
	const char *opt_type, const char *opt_name,
	void *opt_iterator);


/* Adjust _storage size_ */
int _ncnf_coll_adjust_size(void *ignore, collection_t *coll, int new_count);

/* Remove marked elements */
void _ncnf_coll_remove_marked(collection_t *coll, int match_mark);

/*
 * Empty the collection.
 */
void _ncnf_coll_clear(void *ignore, collection_t *coll, int destroy);

/*
 * return the number of entry (coll->count)
 */
int ncnf_coll_get_nentry(collection_t *coll);

/*
 * return the ncnf_obj_s at index (coll->entry[index].object)
 */
struct ncnf_obj_s* ncnf_coll_get_obj_at(collection_t *coll, int idx);

#endif	/* __NCNF_COLL_H__ */
