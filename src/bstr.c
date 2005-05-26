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
 * Implementation of the reference-counted, length-contained
 * basic ASCIIZ string. Modelled after well-known Microsoft
 * Windows "Basic String" type, bstr_t.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "bstr.h"

/*
 * Shadow structure behind the bstr_t pointer.
 * This structure will be properly aligned.
 */
typedef struct bstr_shadow_s {
	union {
		struct {
			int refs;	/* Reference counter */
			int len;	/* String length (always positive) */
		} life;
		struct {
			bstr_t next;
			int chain_size;
		} death;
	} bstr_shadow_union;
#define	b_refs	bstr_shadow_union.life.refs
#define	b_len	bstr_shadow_union.life.len
#define	b_next	bstr_shadow_union.death.next
#define	b_chain	bstr_shadow_union.death.chain_size
} bstr_shadow_t;

#define	SHADOW(bstr)	((bstr_shadow_t *)(((char *)bstr) \
			- sizeof(bstr_shadow_t)))
#define	ROUND_SIZE	(16)
#define	ROUND_BITS	(4)
#define	ROUND_MASK	((ROUND_SIZE)-1)
#define	ROUND(foo)	( ((foo) + (ROUND_MASK)) & (~(ROUND_MASK)) )

#define	BSTR_FREE_STORAGE_SIZE	256
#define	BSTR_MAX_CHAIN_SIZE	256

static bstr_t _bstr_free_storage[BSTR_FREE_STORAGE_SIZE];
static bstr_t _bstr_get(int len);
static int mem_required(int strlen_ex_null);

/*
 * calc the mem required of a bstr
 * @param
 * @int str_len_wo_null: string's length excluding the null terminator
 */
static inline int
mem_required(int strlen_ex_null)
{
	return ROUND(sizeof(bstr_shadow_t) + strlen_ex_null + 1);
}

/*
 * Create a new basic string.
 * Initialize the reference counter to ONE.
 */
bstr_t
str2bstr(const char *optStr, int optLen) {
	bstr_t bs;

	if(!optStr && optLen < 0) {
		errno = EINVAL;
		return (bstr_t)0;
	}

	if(optLen < 0)
		optLen = strlen(optStr);

	bs = _bstr_get(optLen);
	if(bs == NULL) {
		return NULL;
	}

	SHADOW(bs)->b_refs = 1;
	SHADOW(bs)->b_len = optLen;
	if(optStr) memcpy(bs, optStr, optLen);
	bs[optLen] = '\0';

	return bs;
}

/*
 * Get a pointer to string, incrementing it's reference counter.
 */
bstr_t
bstr_ref(bstr_t src) {
	if(src == NULL) {
		errno = EINVAL;
		return NULL;
	}

	assert(SHADOW(src)->b_refs >= 0);
	SHADOW(src)->b_refs++;

	return src;
}

/*
 * Make a private copy of the given string.
 * Initialize reference counter to one.
 */
bstr_t
bstr_copy(bstr_t src) {

	if(src == NULL) {
		errno = EINVAL;
		return NULL;
	}

	assert(SHADOW(src)->b_refs > 0);

	return str2bstr(src, bstr_len(src));
}


/*
 * Decrement reference counter. If it reaches zero, move this storage
 * into special memory area, or free it, if inappropriate.
 * Depending on the flag, it may also memset area to 0 before freenig.
 */
static inline void
bstr_free_impl(bstr_t bs, int clear_me) {
	int len;
	int mem_size;

	if(bs == NULL) {
		errno = EINVAL;
		return;
	}

	if(--(SHADOW(bs)->b_refs) > 0) {
		/*
		 * Don't allow double bstr_free'ing.
		 * Note that b_next and b_refs is the same memory location.
		 */
		assert( (((int)(SHADOW(bs)->b_next) >> 24) == 0) );
		return;
	}

	if(clear_me && SHADOW(bs)->b_len)
		memset(bs, 0, SHADOW(bs)->b_len);

	mem_size = mem_required(SHADOW(bs)->b_len);
 	len = mem_size >> ROUND_BITS;

	/*
	 * We don't post-keep strings with length more than
	 * (BSTR_FREE_STORAGE_SIZE << ROUND_BITS)
	 */
	if(len >= BSTR_FREE_STORAGE_SIZE) {
		free( SHADOW(bs) );
		return;
	}

	if(_bstr_free_storage[len]) {
		SHADOW(bs)->b_chain
			= SHADOW(_bstr_free_storage[len])->b_chain + 1;
		if(SHADOW(bs)->b_chain > BSTR_MAX_CHAIN_SIZE) {
			free( SHADOW(bs) );
			/* Don't create too long chain of freed strings. */
			return;
		}
		SHADOW(bs)->b_next = _bstr_free_storage[len];
	} else {
		SHADOW(bs)->b_next = 0;
		SHADOW(bs)->b_chain = 1;
	}

	_bstr_free_storage[len] = bs;
}

void
bstr_free_zero(bstr_t bs) {
    bstr_free_impl(bs,1);
}

void
bstr_free(bstr_t bs) {
    bstr_free_impl(bs,0);
}


/* Get the string length */
int
bstr_len(bstr_t bs) {
	return bs ? SHADOW(bs)->b_len : 0;
}

/* Get the reference count */
int
bstr_refs(bstr_t bs) {
	if(bs) {
		return SHADOW(bs)->b_refs;
	} else {
		return 0;
	}
}

/* Local service functions */

/* return a bstr_t. it will try to find in the
 * bstr pool.  If it missed, it malloc new memory
 *
 * @param
 * @int len: string length _EXCLUDING_ the null terminator
 */
static bstr_t
_bstr_get(int len) {
	bstr_t bs;
	int mem_size = mem_required(len);

 	len = mem_size >> ROUND_BITS;
	
	if(len < BSTR_FREE_STORAGE_SIZE && (bs = _bstr_free_storage[len])) {
		/* great! found in bstr pool */
		_bstr_free_storage[len] = SHADOW(bs)->b_next;
		return bs;
	} else {
		/* bstr pool missed.  malloc now */
		char *new_bs = (char*)malloc(mem_size);
		if(new_bs) {
			return (bstr_t)(new_bs + sizeof(bstr_shadow_t));
		} else {
			return NULL;
		}
	}
}

void
bstr_flush_cache() {
	bstr_t bs;
	int i;
	
	for(i = 0; i < BSTR_FREE_STORAGE_SIZE; i++) {
		int mem_size = i << ROUND_BITS;
		while((bs = _bstr_free_storage[i])) {
			_bstr_free_storage[i] = SHADOW(bs)->b_next;
			free(SHADOW(bs));
		}
	}
}

