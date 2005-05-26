/*
 * This code was taken from the libstrfunc project, http://lionet.info
 * All rights reserved.
 */
/*-
 * Copyright (c) 1999, 2000, 2001 Lev Walkin <vlm@lionet.info>.
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

#ifndef NCNF_SF_LITE_H
#define NCNF_SF_LITE_H

	  /*************************/
	 /*** 1. String vectors ***/
	/*************************/

/*
 * A vector of known size lengths.
 */
typedef struct {
	char   **list;
	size_t	*lens;
	size_t	count;		/* Count of active elements */
	size_t	listlen;	/* Allocated size */
} ncnf_sf_svect;

ncnf_sf_svect	*ncnf_sf_sinit(void);		/* Create empty string vector */
void	ncnf_sf_sclear(ncnf_sf_svect *);	/* Clear elements of filled structure */
void	ncnf_sf_sfree(ncnf_sf_svect *);		/* Destroy the entire object vector */

/* Add element to the end of list */
int	ncnf_sf_sadd(ncnf_sf_svect *, const char *toadd);	/* with strdup(toadd) */
int	ncnf_sf_sadd2(ncnf_sf_svect *, const char *toadd, size_t len); /* With length. */

int	ncnf_sf_sdel(ncnf_sf_svect *, size_t num);	/* Delete specified element */

/* String splitting functions */
ncnf_sf_svect *ncnf_sf_split(const char *what, const char *delim, int flags); /* creates new (ncnf_sf_svect *) */
int ncnf_sf_splitf(ncnf_sf_svect *sl, const char *what, const char *delim, int strict);

/* Find element */
ssize_t ncnf_sf_sfind(ncnf_sf_svect *, const char *);	/* Case-sensitive */

char *ncnf_sf_sjoin(ncnf_sf_svect *s, const char *delimiter);

#endif	/* NCNF_SF_LITE_H */
