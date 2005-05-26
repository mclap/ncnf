typedef union {
	int tv_int;
	bstr_t tv_str;
	struct ncnf_obj_s *tv_obj;
} YYSTYPE;
#define	TOK_NAME	257
#define	TOK_STRING	258
#define	ERROR	259
#define	AT	260
#define	SEMICOLON	261
#define	INSERT	262
#define	INHERIT	263
#define	REF	264
#define	ATTACH	265


extern YYSTYPE ncnf_cr_lval;
