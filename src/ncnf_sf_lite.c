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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

#include "ncnf_sf_lite.h"

#define	sf_malloc	malloc
#define	sf_calloc	calloc
#define	sf_realloc	realloc

/*
 * Create or initialize empty string list.
 */
ncnf_sf_svect *
ncnf_sf_sinit() {
	ncnf_sf_svect *sl;

	sl=(ncnf_sf_svect *)sf_calloc(1, sizeof(ncnf_sf_svect));
	if(!sl)
		return NULL;

	sl->listlen = 4;

	/* Place for data chunks */
	sl->list = (char **)sf_malloc(sizeof(char *) * sl->listlen);
	if(!sl->list) {
		free(sl);
		return NULL;
	}

	/* Place for data lengths */
	sl->lens = (size_t *)sf_malloc(sizeof(size_t) * sl->listlen);
	if(!sl->lens) {
		free(sl->list);
		free(sl);
		return NULL;
	}

	sl->list[0] = NULL;
	sl->lens[0] = 0;

	return sl;
};

/* frees any strings inside the ncnf_sf_svect object */
void
ncnf_sf_sclear(ncnf_sf_svect *sl) {

	if(sl == NULL)
		return;

	if(sl->list) {
		while(sl->count--) {
			if(sl->list[sl->count])
				free(sl->list[sl->count]);
		}
		*(sl->list) = NULL;
		free(sl->list);
		sl->list = 0;
	}

	if(sl->lens) {
		free(sl->lens);
	}

	sl->lens = 0;
	sl->count = 0;
	sl->listlen = 0;
}

void
ncnf_sf_sfree(ncnf_sf_svect *sl) {
	if(!sl)
		return;

	if(sl->list) {
		if(sl->count > 0)
			while(sl->count--) {
				void *p = sl->list[sl->count];
				if(p) free(p);
			}
		free(sl->list);
	};

	if(sl->lens)
		free(sl->lens);

	free(sl);
};

/* Basic adding function */
int
_sf_add_internal(ncnf_sf_svect *s, void *msg, size_t len) {

	if(s->count + 1 >= s->listlen) {
		void *z;
		size_t newlistlen;

		if((newlistlen = s->listlen << 2) == 0)
			newlistlen = 4;

		z = sf_realloc(s->list, sizeof(char *) * newlistlen);
		if(z)
			s->list = (char **)z;
		else
			return -1;

		z = sf_realloc(s->lens, sizeof(size_t) * newlistlen);
		if(z)
			s->lens = (size_t *)z;
		else
			return -1;
		s->listlen = newlistlen;
	}

	s->list[s->count] = msg;
	s->lens[s->count] = len;

	s->count++;
	s->list[s->count] = NULL;
	s->lens[s->count] = 0;

	return 0;
}


/* Add a string to the end of the ncnf_sf_svect structure */
int
ncnf_sf_sadd2(ncnf_sf_svect *s, const char *msg, size_t len) {
	char *tmp;

	if(!s || !msg)
		return -1;

	tmp = (char *)sf_malloc(len+1);
	if(!tmp)
		return -1;
	memcpy(tmp, msg, len);
	tmp[len] = 0;

	if(_sf_add_internal(s, tmp, len) == -1) {
		free(tmp);
		return -1;
	}

	return 0;
};

int
ncnf_sf_sadd(ncnf_sf_svect *s, const char *msg) {
	if(!s || !msg)
		return -1;
	return ncnf_sf_sadd2(s, msg, strlen(msg));
}


/* delete i'th string from the list */
int
ncnf_sf_sdel(ncnf_sf_svect *s, size_t n) {
	if(!s)
		return -1;

	if(n >= s->count)
		return s->count;

	free(s->list[n]);

	for(s->count--; n <= s->count; n++) {
		s->list[n] = s->list[n+1];
		s->lens[n] = s->lens[n+1];
	};

	return s->count;
}

ssize_t
ncnf_sf_sfind(ncnf_sf_svect *sl, const char *what) {
	size_t n;
	size_t wlen;
	char cw;

	if(!sl || !sl->count || !what)
		return -1;

	wlen=strlen(what);
	cw = *what;

	for(n=0; n < sl->count; n++) {

		if(sl->lens[n] != wlen)
			continue;

		if(wlen) {
			if(
				(sl->list[n][0] == cw)
				&& !strcmp(sl->list[n], what)
			) return n;
		} else {
			return n;
		};

	};

	return -1;
}

/****************/
/* SPLIT STRING */
/****************/

/* split string and add its contents to the ncnf_sf_svect structure */
/* Split: full version */
int
ncnf_sf_splitf(ncnf_sf_svect *s, const char *msg, const char *dlm, int flags) {
	const char *p = msg;
	const char *w = NULL;
	int tokens = 0;
	int dlen;
	char cp, cd;

	if(!s || !p) {
		errno = EINVAL;
		return -1;
	}

	if(!dlm) {
		if(flags & 4)
			flags &= ~4;

		if(flags)
			/* Default to parse /etc/password-like strings */
			dlm = ":";
		else
			/* Default to split on common whitespaces */
			dlm = " \t\n\r";
	}
	cd = *dlm;
	dlen = strlen(dlm);

	if(flags & 2) {

		for(; (cp = *p); p++) {
			if((cd == cp) && (strncmp(p, dlm, dlen) == 0)) {
				if(w) {
					if(ncnf_sf_sadd2(s, w, p-w) == -1) {
						while(tokens--)	/* cleanup */
							ncnf_sf_sdel(s, s->count - 1);
						return -1;
					}
					w = NULL;
					tokens++;
				} else if(flags & 1) {
					if(ncnf_sf_sadd2(s, "", 0) == -1) {
						while(tokens--)	/* cleanup */
							ncnf_sf_sdel(s, s->count - 1);
						return -1;
					};
					tokens++;
				}
				p += dlen - 1;
			} else {
				if(!w)
					w = p;
			}
		}

	} else {

		for(; (cp = *p); p++) {
			if((cd == cp) || memchr(dlm, cp, dlen)) {
				if(w) {
					if(ncnf_sf_sadd2(s, w, p-w) == -1) {
						while(tokens--)
							ncnf_sf_sdel(s, s->count -1);
						return -1;
					};
					w = NULL;
					tokens++;
				} else if(flags & 1) {
					if(ncnf_sf_sadd2(s, "", 0) == -1) {
						while(tokens--)
							ncnf_sf_sdel(s, s->count -1);
						return -1;
					}
					tokens++;
				}
			} else {
				if(!w)
					w = p;
			}
		}

	}

	if(w) {
		ncnf_sf_sadd2(s, w, p-w);
		tokens++;
	}

	return tokens;
}

ncnf_sf_svect *
ncnf_sf_split(const char *msg, const char *delim, int flags) {
	ncnf_sf_svect *sl;

	sl = ncnf_sf_sinit();
	if(!sl)
		return NULL;

	if(ncnf_sf_splitf(sl, msg, delim, flags) == -1) {
		ncnf_sf_sfree(sl);
		return NULL;
	}

	return sl;
}

static char *_sf_sjoin_buf = NULL;

char *
ncnf_sf_sjoin(ncnf_sf_svect *s, const char *delimiter) {
	char *buf;
	int flen;
	int k, need;
	char *p;

	if(s == NULL || s->count == 0) {
		if(_sf_sjoin_buf) free(_sf_sjoin_buf);
		return (_sf_sjoin_buf = strdup(""));
	}

	if(delimiter) {
		flen = strlen(delimiter);
	} else {
		delimiter = "";
		flen = 0;
	}

	for(need = 0, k = 0; k < s->count; k++)
		need += ((ssize_t)s->lens[k] >= 0
				? s->lens[k] : strlen(s->list[k]))
			+ (k?flen:0);

	buf = (char *)sf_malloc(need + 1);
	if(buf == NULL)
		/* ENOMEM? */
		return NULL;

	p=buf;

	for(k = 0; k < s->count; k++) {
		if(k) {
			memcpy(p, delimiter, flen);
			p += flen;
		}
		if((need = s->lens[k]) < 0)
			need = strlen(s->list[k]);
		memcpy(p, s->list[k], need);
		p += need;
	}
	*p = 0;

	if(_sf_sjoin_buf) free(_sf_sjoin_buf);
	return (_sf_sjoin_buf=buf);
}
