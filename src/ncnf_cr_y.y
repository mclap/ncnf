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

#undef	NDEBUG
#include "headers.h"
#include "ncnf_int.h"
#include "ncnf.h"

#define	YYPARSE_PARAM	paramp

int yylex(void);
int yyerror(char *s);

extern int __ncnf_cr_lineno;

#define	RELAXED_NS		((int)*((void **)YYPARSE_PARAM+1))
#define	ALLOC_NOBJ(_class)	_ncnf_obj_new(0, _class, NULL, NULL, __ncnf_cr_lineno)


/*
 * Quick wrapper around object attachment.
 */
#define	ATTACH_OBJ(dst,src) do {				\
		if(_ncnf_attach_obj(dst, src, RELAXED_NS)) {	\
			int lineno = src->config_line;		\
			_ncnf_obj_destroy(src);			\
			_ncnf_obj_destroy(dst);			\
			dst = NULL;				\
			if(errno == EEXIST) {			\
			__ncnf_cr_lineno = lineno;		\
			yyerror("Similarly named entity already defined"); \
			}					\
			YYABORT;				\
		}						\
	} while(0)

%}

/*
 * Token value definition
 */
%union {
	int tv_int;
	bstr_t tv_str;
	struct ncnf_obj_s *tv_obj;
}

/*
 * Token types returned by scanner
 */
%token	<tv_str>	TOK_NAME
%token	<tv_str>	TOK_STRING

/*
 * Reserved words
 */
%token		ERROR
%token		AT
%token		SEMICOLON
%token		INSERT INHERIT REF ATTACH


%type	<tv_obj>	sequence
%type	<tv_obj>	block
%type	<tv_obj>	object
%type	<tv_obj>	attribute
%type	<tv_obj>	insertion
%type	<tv_obj>	reference
%type	<tv_obj>	entity
%type	<tv_int>	reftype

%%
cfg_file: {
		**(void ***)YYPARSE_PARAM = ALLOC_NOBJ(NOBJ_ROOT);
		if(**(void ***)YYPARSE_PARAM == NULL)
			YYABORT;
	}
	| sequence {
		struct ncnf_obj_s *param_value = NULL;

		if($1 == NULL) {
			errno = EINVAL;
			YYABORT;
		}

		if($1->obj_class == NOBJ_ROOT) {
			param_value = $1;
		} else {
			param_value = ALLOC_NOBJ(NOBJ_ROOT);
			if(param_value == NULL) {
				_ncnf_obj_destroy($1);
				YYABORT;
			} else {
				ATTACH_OBJ(param_value, $1);
			}
		}

		/* Propagate root up to the caller */
		**(void ***)YYPARSE_PARAM = param_value;
	}
	;

ps:
	| SEMICOLON;

sequence: block ps {
		if($1 == NULL) {
			$$ = NULL;
			yyclearin;
			yyerrok;
			break;
		}

		$$ = ALLOC_NOBJ(NOBJ_ROOT);
		if($$ == NULL) {
			_ncnf_obj_destroy($1);
			$1 = NULL;
			$$ = NULL;
			YYABORT;
		} else {
			ATTACH_OBJ($$, $1);
		}
	}
	| sequence block ps {

		$$ = NULL;
		if($1 == NULL) {
			if($2) _ncnf_obj_destroy($2);
			break;
		} else if($2 == NULL) {
			if($1) _ncnf_obj_destroy($1);
			break;
		}

		if($1->obj_class == NOBJ_ROOT) {
			$$ = $1;

			ATTACH_OBJ($$, $2);
		} else {
			$$ = ALLOC_NOBJ(NOBJ_ROOT);
			if($$ == NULL) {
				_ncnf_obj_destroy($1);
				_ncnf_obj_destroy($2);
				$$ = NULL;
				YYABORT;
			}

			/* Join two blocks under root */

			/* Insert a nested object 1 */
			if(_ncnf_attach_obj($$, $1, RELAXED_NS)) {
				_ncnf_obj_destroy($1);
				_ncnf_obj_destroy($2);	/* Don't forget */
				_ncnf_obj_destroy($$);
				$$ = NULL;
				if(errno == EEXIST)
					yyerror("Similarly named entity "
						"already defined");
				YYABORT;
			} else {
				/* Insert object 2 */
				ATTACH_OBJ($$, $2);
			}
		}
	}
	;

block:
	object { $$ = $1; }
	| attribute SEMICOLON { $$ = $1; }
	| insertion SEMICOLON { $$ = $1; }
	| reference SEMICOLON { $$ = $1; }
	;

object:
	entity '{' '}' {
		$$ = $1;
		if($$) {
			$$->obj_class = NOBJ_COMPLEX;
		}
	}
	| entity '{' sequence '}' {
		int c;

		$$ = NULL;
		if($1 == NULL) {
			if($3) _ncnf_obj_destroy($3);
			break;
		} else if($3 == NULL) {
			if($1) _ncnf_obj_destroy($1);
			break;
		}

		assert($3->obj_class == NOBJ_ROOT);

		$$ = $1;
		$$->obj_class = NOBJ_COMPLEX;
		
		for(c = 0; c < MAX_COLLECTIONS; c++) {
			collection_t *coll_to = &($$)->m_collection[c];
			collection_t *coll_from = &($3)->m_collection[c];
			if(_ncnf_coll_join(($$)->mr,
				coll_to, coll_from, $$,
				(RELAXED_NS ? MERGE_NOFLAGS : MERGE_DUPCHECK)
				| MERGE_EMPTYSRC))
				break;
		}

		/* Dispose the NCNF_ROOT wrapper around $3 */
		_ncnf_obj_destroy($3);

		if(c < MAX_COLLECTIONS) {
			_ncnf_obj_destroy($$);
			$$ = NULL;
			YYABORT;
		}
	}
	;

attribute:
	entity {
		$$ = $1;
		if($$) {
			$$->obj_class = NOBJ_ATTRIBUTE;
		}
	}
	| TOK_NAME '=' TOK_NAME {
		$$ = ALLOC_NOBJ(NOBJ_ATTRIBUTE);
		if($$) {
			$$->type = bstr_ref($1);
			$$->value = bstr_ref($3);
			$$->m_attr_flags |= 1;
		} else {
			YYABORT;
		}
	}
	;

insertion:
	INSERT entity {
		$$ = $2;
		if($$) {
			$$->obj_class = NOBJ_INSERTION;
		}
	}
	| INHERIT entity {
		$$ = $2;
		if($$) {
			$$->obj_class = NOBJ_INSERTION;
			$$->m_insert_flags |= 1;
		}
	}
	;

reference:
	reftype TOK_NAME TOK_STRING '=' TOK_NAME TOK_STRING {
		$$ = ALLOC_NOBJ(NOBJ_REFERENCE);
		if($$) {
			$$->type = bstr_ref($2);
			$$->value = bstr_ref($3);
			$$->m_ref_type = bstr_ref($5);
			$$->m_ref_value = bstr_ref($6);
			$$->m_ref_flags = $1;
		} else {
			YYABORT;
		}
	}
	| reftype TOK_NAME '=' TOK_NAME TOK_STRING {
		$$ = ALLOC_NOBJ(NOBJ_REFERENCE);
		if($$) {
			$$->type = bstr_ref($2);
			$$->value = bstr_ref($5);
			$$->m_ref_type = bstr_ref($4);
			$$->m_ref_value = bstr_ref($5);
			$$->m_ref_flags = $1;
		} else {
			YYABORT;
		}
	}
	| reftype '=' TOK_NAME TOK_STRING {
                $$ = ALLOC_NOBJ(NOBJ_REFERENCE);
                if($$) {
                        $$->type = bstr_ref($3);
                        $$->value = bstr_ref($4);
                        $$->m_ref_type = bstr_ref($3);
                        $$->m_ref_value = bstr_ref($4);
                        $$->m_ref_flags = $1;
                } else {
			YYABORT;
                }
	}
	| reftype TOK_NAME TOK_STRING '=' TOK_STRING {
                $$ = ALLOC_NOBJ(NOBJ_REFERENCE);
                if($$) {
                        $$->type = bstr_ref($2);
                        $$->value = bstr_ref($3);
                        $$->m_ref_type = bstr_ref($2);
                        $$->m_ref_value = bstr_ref($5);
                        $$->m_ref_flags = $1;
                } else {
			YYABORT;
                }
	}
	;

reftype:
	REF {
		$$ = 0;
	}
	| ATTACH {
		$$ = 1;
	}
	;

entity:
	TOK_NAME TOK_STRING {
		$$ = ALLOC_NOBJ(-2);
		if($$) {
			$$->type = bstr_ref($1);
			$$->value = bstr_ref($2);
		} else {
			YYABORT;
		}
	}
	;


%%

int
yyerror(char *s) {
	if(s == NULL) {
		switch(errno) {
		case EEXIST:
			s = "Entity already defined this context";
			break;
		default:
			s = strerror(errno);
		}
	}

	_ncnf_debug_print(1,
		"Config parse error near line %d: %s",
		__ncnf_cr_lineno, s);
	return -1;
};

