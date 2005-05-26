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
%{

#include "headers.h"
#include "ncnf_int.h"
#include "ncnf_cr_y.h"

int __ncnf_cr_lineno = 1;

int ncnf_cr_lex(void);


char *s_buf;
int s_buf_len;
int s_buf_size;

static int _init_s_buf() {
	if(s_buf) free(s_buf);
	s_buf_size = 512;
	s_buf_len = 0;
	s_buf = malloc(s_buf_size);
	if(s_buf == NULL) {
		s_buf_size = 0;
		return -1;
	}
	s_buf[0] = '\0';
	return 0;
}

static int _s_buf_addchar(char ch) {
	if((s_buf_size - s_buf_len) < 2) {
		char *p;
		/*
		 * Reallocate
		 */
		p = realloc(s_buf, s_buf_size << 2);
		if(p == NULL)
			return -1;
		s_buf_size <<= 2;
		s_buf = p;
	}

	s_buf[s_buf_len++] = ch;
	s_buf[s_buf_len] = '\0';

	return 0;
}

static genhash_t *__token_pool;

void ncnf__flush_lex_token_pool(void);
void ncnf__flush_lex_token_pool() {
	genhash_destroy(__token_pool);
	__token_pool = 0;
}

#define	ADD_STR_POOL(b)	do {						\
		bstr_t nb;						\
		if(!b) return ERROR;					\
		if(!__token_pool) {					\
			__token_pool = genhash_new(			\
				cmpf_string, hashf_string,		\
				NULL, (void (*)(void *))bstr_free);	\
			if(!__token_pool) {				\
				bstr_free(b);				\
				return ERROR;				\
			}						\
		}							\
		nb = genhash_get(__token_pool, b);			\
		if(nb) {						\
			bstr_free(b);					\
			b = nb;						\
		} else if(genhash_add(__token_pool, b, b)) {		\
			bstr_free(b);					\
			return ERROR;					\
		}							\
	} while(0)
		

%}

%option never-interactive
%option noinput nounput
%option noyywrap stack
%option caseless 8bit

%pointer

%x comment
%x multiline

%%

<INITIAL,comment>"/*"		yy_push_state(comment);
<comment>{
	[^*/\n]+		/* Eat */
	"*/"	yy_pop_state();
	\n	__ncnf_cr_lineno++;
	.	/* Eat */
}

#[^\n]*[\n]?	{
		if(yytext[yyleng-1] == '\n')
			__ncnf_cr_lineno++;
	}

"//"[^\n]*[\n]?	{
		if(yytext[yyleng-1] == '\n')
			__ncnf_cr_lineno++;
	}


"{"	{ return '{'; }
"="	{ return '='; }
"}"	{ return '}'; }
";"	{ return SEMICOLON; }

ins	{ return INSERT; }
insert	{ return INSERT; }
inherit	{ return INHERIT; }
ref	{ return REF; }
reference	{ return REF; }
att	{ return ATTACH; }
attach	{ return ATTACH; }

[a-z0-9\._-]+	{
		bstr_t b = str2bstr(yytext, yyleng);
		ADD_STR_POOL(b);
		ncnf_cr_lval.tv_str = b;
		return TOK_NAME;
	}

\"[^"\n\v\f\r\\]*\"	{
		bstr_t b;
		yytext[yyleng - 1] = '\0';
		b = str2bstr(yytext+1, yyleng-2);
		ADD_STR_POOL(b);
		ncnf_cr_lval.tv_str = b;
		return TOK_STRING;
	}

\"[^"\n\v\f\r\\][^"\n\v\f\r]*\"	{
		bstr_t b;
		yytext[yyleng - 1] = '\0';
		b = str2bstr(yytext+1, yyleng-2);
		ADD_STR_POOL(b);
		ncnf_cr_lval.tv_str = b;
		return TOK_STRING;
	}


\"/\\	{
		/*
		 * Quote and backslash immediately after it.
		 * It's a special kind of string: it has relaxed
		 * rules and \n etc rewriting.
		 */
		if(_init_s_buf()) return ERROR;
		yy_push_state(multiline);
	}

<multiline>{

	[^\\\r\n\"]+	{
			if((s_buf_size - s_buf_len) > yyleng) {
				strncpy(s_buf + s_buf_len, yytext, yyleng);
				s_buf_len += yyleng;
				s_buf[s_buf_len] = '\0';
			} else {
				char *p;
				for(p = yytext; *p; p++) {
					if(_s_buf_addchar(*p)) {
						while(YY_START) yy_pop_state();
						yy_top_state();
						return ERROR;
					}
				}
			}
		}

	\\[\r]?\n	{ __ncnf_cr_lineno++; /* Nothing more: skip it */ }

	\\.	{
			char ch = yytext[1];
			switch(ch) {
			case 'n':
				ch = '\n';
				break;
			case 't':
				ch = '\t';
				break;
			}

			if(_s_buf_addchar(ch)) {
				while(YY_START) yy_pop_state();
				return ERROR;
			}
		}

	\"	{
			/*
			 * End of string.
			 */
			bstr_t b = str2bstr(s_buf, s_buf_len);
			ADD_STR_POOL(b);
			ncnf_cr_lval.tv_str = b;
			free(s_buf);
			s_buf = NULL;
			yy_pop_state();
			return TOK_STRING;
		}

	(.|\n)	{
			while(YY_START) yy_pop_state();
			return ERROR;
		}

  }

\"[\r]?\n[^\\"]*\n[\t ]*\"	{
		const int strip_last_crlf = 0;
		bstr_t b;
		char *p;
		int i;

		for(p = yytext; *p; p++)
			if(*p == '\n')
				__ncnf_cr_lineno++;

		p = strchr(yytext, '\n');
		assert(p);
		yyleng -= (p - yytext) + 1;
		yytext = ++p;		/* Skip after the first \n */

		/*
		 * Get to the last \n.
		 */
		for(i = yyleng; i >= 0; i--) {
			if(yytext[i] == '\n') {
				if(strip_last_crlf) {
					if(i > 0 && yytext[i - 1] == '\r')
						i--;
				} else {
					/* One symbol past. */
					i++;
				}
				yytext[i] = '\0';
				break;
			}
		}

		/*
		 *
		 */
		b = str2bstr(yytext, -1);
		ADD_STR_POOL(b);
		ncnf_cr_lval.tv_str = b;
		return TOK_STRING;
	}

<*>[[:space:]]	{
		if(*yytext == '\n')
			__ncnf_cr_lineno++;
	}

<*>.	{
		while(YY_START) yy_pop_state();
		return ERROR;
	}

<*><<EOF>>	{
		while(YY_START) yy_pop_state();
		yy_delete_buffer(yy_current_buffer);
		yy_current_buffer = 0;
		if(yy_start_stack) {
			yy_flex_free(yy_start_stack);
			yy_start_stack_ptr = 0;
			yy_start_stack_depth = 0;
			yy_start_stack = 0;
		}
		yyterminate();
	}

%%
