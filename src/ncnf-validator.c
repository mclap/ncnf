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
#include "headers.h"

#include "ncnf.h"
#include "ncnf_app.h"

#undef	SUPPORT_NCQL
#ifdef	HAVE_LIBSTRFUNC
#define	SUPPORT_NCQL
#endif	/* HAVE_LIBSTRFUNC */

#ifdef	SUPPORT_NCQL
/*
 * NCQL is only supported when libstrfunc was around when building NCNF.
 */
#include "ncnf_ql.h"
#endif	/* SUPPORT_NCQL */

void usage(const char *);

int
main(int ac, char **av) {
	ncnf_obj *root = NULL;
	ncnf_obj *start_obj;
	char *start_path = NULL;/* -S controls that */
	int validate = 1;	/* -V disables that */
	int verbose = 0;	/* -v enables that */
	int indent = 4;		/* -i controls that */
	char *output_file = 0;	/* -o controls that */
	int profile_mode = 0;	/* -p enables that */
	int reload_times = 0;	/* -r controls that */
	int silent = 0;		/* -s enables that */
	char *flatten_type = 0;	/* -t controls that */
	ncnf_sf_svect *query_files = 0;	/* -Q controls that */
	int rld;
	int ch;

	while((ch = getopt(ac, av,
#ifdef	SUPPORT_NCQL
		"Q:"
#endif	/* SUPPORT_NCQL */
		"P:S:i:mo:pr:st:Vv")) != -1)
	switch(ch) {
	case 'Q':
		if(!query_files) query_files = ncnf_sf_sinit();
		ncnf_sf_sadd(query_files, optarg);
		break;
	case 'S':
	case 'P':
		if(start_path) {
			fprintf(stderr, "-S used twice\n");
			usage(av[0]);
		}
		start_path = optarg;
		break;
	case 'i':
		indent = atoi(optarg);
		if(indent > 8)
			indent = 8;
		else if(indent < 1)
			indent = 1;
		break;
	case 'o':
		if(output_file) {
			fprintf(stderr, "-o used twice\n");
			usage(av[0]);
		}
		output_file = optarg;
		break;
	case 'p':
		profile_mode = 1;
		break;
	case 'r':
		reload_times = atoi(optarg);
		if(reload_times < 0)
			reload_times = 0;
		break;
	case 's':
		silent = 1;
		break;
	case 't':
		flatten_type = optarg;
		/* Fall through */
	case 'V':
		validate = 0;
		break;
	case 'v':
		verbose = 1;
		break;
	default:
		usage(av[0]);
	}

	ac -= optind;
	av += optind;

	if(silent && output_file) {
		fprintf(stderr, "-s and -o cannot be used simultaneously\n");
		usage(av[-optind]);
	}

	if(ac <= 0 || ac > (reload_times + 1)) {
		if(ac > 0)
			fprintf(stderr,
			"ERROR: Number of -r reloads is insufficient "
			"to cover the supplied files\n");
		usage(av[-optind]);
	}

	for(rld = 0; rld <= reload_times; rld++) {
		/* Read the ncnf file, optionally disabling validation */
		ncnf_obj *new_root = ncnf_Read(av[rld % ac], NCNF_ST_FILENAME
			| (validate ? 0 : (NCNF_FL_NODYN | NCNF_FL_NOEMB)));
		if(new_root == NULL) {
			perror("Failed to read configuration file");
			return 1;
		}

		if(reload_times && root) {
			int ret;
			if(profile_mode)
				printf("Merging the files\n");
			ret = ncnf_diff(root, new_root);
			assert(ret == 0);
			if(profile_mode)
				printf("Destroying processed new root\n");
			ncnf_destroy(new_root);
		} else {
			root = new_root;
		}

#ifdef	SUPPORT_NCQL
		/* -Q in operation */
		if(query_files) {
			int i;
			ncnf_clear_query(root ? root : new_root);
			for(i = 0; i < query_files->count; i++) {
				char errbuf[256];
				size_t errlen = sizeof(errbuf);
				ncnf_query_t *nq;

				ncnf_obj *ncql = ncnf_Read(
					query_files->list[i],
					NCNF_ST_FILENAME
					/* Never use embedded validation */
					| NCNF_FL_NOEMB
					/* Relaxed namespace rules */
					| NCNF_FL_RELNS);
				if(!ncql) {
					fprintf(stderr,
					"-Q %s: Failed to read file: %s\n",
						query_files->list[i],
						strerror(errno));
					return 1;
				}

				nq = ncnf_compile_query(ncql, errbuf, &errlen);
				ncnf_destroy(ncql);
				if(!nq) {
					fprintf(stderr, "-Q %s: %s\n",
						query_files->list[i],
						errbuf);
					return 1;
				}
				ncnf_exec_query(root ? root : new_root, nq, 0);
				ncnf_delete_query(nq);
			}
		}
#endif	/* SUPPORT_NCQL */

		if(profile_mode) {
			int sleep_time = 10;
			if(rld < reload_times) {
				printf("Wait %ds before %d reload%s\n",
					sleep_time,
					reload_times - rld,
					(reload_times - rld == 1) ? "" : "s"
				);
			} else {
				sleep_time = 10;
				printf("Waiting a bit before exit...\n");
			}
			sleep(sleep_time);
			printf("Waiting finished.\n");
			if(rld == reload_times)
				exit(2);
		}
	}

	assert(root);

	if(start_path) {
		if(strchr(start_path, '@')) {
			start_obj = NCNF_APP_resolve_sysid(root, start_path);
		} else {
			start_obj = NCNF_APP_resolve_path(root, start_path);
		}
		if(start_obj == NULL) {
			fprintf(stderr, "Entity not found: %s: %s\n",
				start_path, strerror(errno));
			exit(EX_DATAERR);
		}
	} else {
		start_obj = root;
	}

	/*
	 * Print config nicely.
	 */
	if(!silent) {
		FILE *ofile = stdout;
		if(output_file) {
			ofile = fopen(output_file, "w");
			if(!ofile) {
				fprintf(stderr, "Cannot save %s: %s",
					output_file, strerror(errno));
				exit(EX_OSERR);
			}
		}
		ncnf_dump(ofile, start_obj, flatten_type,
			query_files ? 1 : 0, verbose, indent);
	}

	ncnf_destroy(root);

	fprintf(stderr, "%s: %s\n", *av, validate ? "Validated" : "Parsed");

	return 0;
}


void
usage(const char *av0) {
	fprintf(stderr,
	"Configuration file validator (c) 2002, 03, 04, 2005 Netli, Inc.\n"
	"Usage: %s [-impQrsStvV] <ncnf_config_file> ...\n"
	"Options:\n"
	"  -i <indent>      Use indentation spaces\n"
	"  -o <ofile.ncnf>  Specify output file instead of default stdout\n"
	"  -p               Profile mode (sleep() & exit())\n"
#ifdef	SUPPORT_NCQL
	"  -Q <file.ncql>   Perform NCQL query using specified query file\n"
#endif	/* SUPPORT_NCQL */
	"  -r <num>         Reload <num> times\n"
	"  -s               Suppress file contents print-out (cmp. -v)\n"
	"  -S <subtree>     Specify @sysid or /path of the subtree to dump\n"
	"  -t <type>        \"Flatten\" the type. Put \"-\" for all types\n"
	"  -v               Turn on verbose contents print-out mode (cmp. -s)\n"
	"  -V               Disable validation\n"
	, av0);
	exit(EX_USAGE);
}

