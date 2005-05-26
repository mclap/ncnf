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
#include "ncnf_int.h"

static void
_print_indent(FILE *f, int indent) {
	while(indent--)
		fprintf(f, " ");
}

/*
 * Process value and insert necessary escaping.
 */
static void
_display_value(FILE *f, const char *value) {
	int to_escape = 0;
	const char *v;

	for(v = value; *v; v++) {
		switch(*v) {
		default:
			continue;
		case '\\':
			if(v != value)
				continue;
		case '"':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
			to_escape = 1;
		}
		break;
	}

	if(!to_escape) {
		/* No escaping is necessary */
		fwrite(value, v - value, 1, f);
		return;
	}

	/* Print out the string, escaping certain characters */
	for(v = value; *v; v++) {
		switch(*v) {
		case '\v': fputs("\\v", f); break;
		case '\f': fputs("\\f", f); break;
		case '\n': fputs("\\n", f); break;
		case '\r': fputs("\\r", f); break;
		case '"': fputs("\\\"", f); break;
		case '\\': fputs("\\\\", f); break;
		default:
			if(v == value) {
				fputc('\\', f);
				if(v[0] != ' '
				&& v[0] != '\n'
				&& v[0] != '\r'
				&& v[0] != '\t'
				) fputc('\n', f);
			}
			fputc(*v, f);
		}
	}

}

void
_ncnf_obj_dump_recursive(FILE *f, struct ncnf_obj_s *obj,
		const char *flatten_type, int marked_only,
		int verbose, int indent, int indent_shift, int single_level,
		int *ret_rsize
) {
	int cc;
	int recursive_size = sizeof(*obj);

	assert(obj->obj_class != NOBJ_INVALID);

	if(marked_only && !obj->mark)		/* Skip unmarked entries */
		return;

	if(obj->obj_class != NOBJ_ROOT)
		_print_indent(f, indent);

	if(flatten_type) indent_shift = 0;

	if(!flatten_type)
	switch(obj->obj_class) {
	case NOBJ_COMPLEX:
		if(single_level) {
			fprintf(f, "%s %s { ... }", obj->type, obj->value);
		} else {
			fprintf(f, "%s \"%s\" {", obj->type, obj->value);
		}
		if(verbose)
			fprintf(f, "\t# line %d	<%d> <%p>",
				obj->config_line,
				obj->obj_class,
				obj
			);
		fputs("\n", f);
		break;
	case NOBJ_ATTRIBUTE:
		if(single_level) {
			fprintf(f, "%s\t", obj->type);
			_display_value(f, obj->value);
		} else {
			fprintf(f, "%s \"", obj->type);
			_display_value(f, obj->value);
			fprintf(f, "\";");
		}
		if(verbose)
			fprintf(f,
				"\t# line %d\t<%d>",
				obj->config_line,
				obj->obj_class
			);
		fputs("\n", f);
		break;
	case NOBJ_REFERENCE:
		if(single_level) {
			fprintf(f, "%s %s => %s %s { ... }",
				obj->type, obj->value,
				obj->m_ref_type, obj->m_ref_value
			);
		} else {
			fprintf(f, "%s %s \"%s\" = %s \"%s\";",
				(obj->m_ref_flags & 1) ? "attach" : "ref",
				obj->type, obj->value,
				obj->m_ref_type, obj->m_ref_value
			);
		}
		if(verbose)
			fprintf(f,
				"\t# line %d <%p>-><%p>",
				obj->config_line,
				obj,
				_ncnf_real_object(obj)
			);
		fputs("\n", f);
		break;
	default:
		/* Do nothing */
		break;
	}

	if(!single_level)
	switch(obj->obj_class) {
	case NOBJ_ROOT:
	case NOBJ_COMPLEX:

		/* Count the overhead for collections */
		for(cc = 0; cc < MAX_COLLECTIONS; cc++) {
			collection_t *coll = &obj->m_collection[cc];
			int i;

			/* Count the overhead */
			recursive_size += coll->size * sizeof(coll->entry[0]);

			for(i = 0; i < coll->entries; i++) {
				struct ncnf_obj_s *child=coll->entry[i].object;

				if(flatten_type
				&& *flatten_type != '-'
				&& *flatten_type != '*'
				&& strcmp(flatten_type, child->type)
				) continue;

				_ncnf_obj_dump_recursive(f, child, 0,
					marked_only,
					verbose,
					indent + (obj->type?indent_shift:0),
					indent_shift, flatten_type ? 1 : 0,
					&recursive_size);
			}

			/* Put some space between entries */
			if(obj->m_collection[cc].entries
			&& obj->m_collection[cc + 1].entries
			&& !flatten_type)
				fprintf(f, "\n");
		}

	default:
		/* Do nothing */
		break;
	}

	if(obj->obj_class == NOBJ_COMPLEX && !flatten_type && !single_level) {
		_print_indent(f, indent);
		fputs("}", f);

		if(verbose) {
			fprintf(f, " # %s \"%s\" RSIZE=%d",
				obj->type, obj->value,
				recursive_size
			);
		}

		fputs(indent?"\n":"\n\n", f);
	}

	if(ret_rsize) *ret_rsize += recursive_size;
}

