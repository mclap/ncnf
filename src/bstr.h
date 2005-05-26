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
#ifndef	__BSTR_H__
#define	__BSTR_H__

/*
 * Implementation of the reference-counted, length-contained
 * basic ASCIIZ string.
 *
 * Length and reference counters are stored in a memory area behind the
 * exported pointer value, and both are basic integers (int).
 * However, the implementation hides the exact placement of these values,
 * and exports safe functions to manipulate them.
 * (bstr_t) pointer is intended to be directly compatible with the
 * functions expecting the (char *) arguments.
 * 
 * All exported functions are immune to NULL's.
 *
 * $Id$
 */

typedef	char *	bstr_t;


/*
 * Allocate a new basic string and copy provided one into a newly created.
 * Optional string could be specified. In this case, a zero-terminated buffer
 * of length optLen will be allocated.
 * Optional length might be left as negative value (-1), in this case
 * strlen() would be used to count the length of the optStr, which is
 * presumed to be ASCIIZ.
 * 
 * Initializes the reference counter to ONE.
 */
bstr_t	str2bstr(const char *optStr, int optLen);

/*
 * Increment reference counter of the given basic string and return
 * the pointer to it (return pointer == pointer provided).
 * Returns NULL only if (src == NULL).
 */
bstr_t	bstr_ref(bstr_t src);

/*
 * Make a private copy of the given string.
 * Initializes the reference counter to ONE.
 */
bstr_t	bstr_copy(bstr_t src);

/*
 * Decrement reference counter and free the basic string,
 * when it becomes zero.
 */
void	bstr_free(bstr_t);

/*
 * Just like bstr_free, but also, if we free the string
 * we memset it to 0 before hand; use this if the
 * string contains sensitive data (crypto, etc).
 */
void	bstr_free_zero(bstr_t);

/*
 * Support functions
 */

/* Get the string length */
int	bstr_len(bstr_t);

/*
 * Get the reference counter value.
 */
int	bstr_refs(bstr_t);

/*
 * Flush the cache of freed memory.
 */
void bstr_flush_cache(void);

#endif	/* __BSTR_H__ */
