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
 * This file is mostly a compatibility layer between
 * external ncnf_obj and internal struct ncnf_obj_s representations.
 * Internal functions provide virtually no fool-proofing.
 */
#include "headers.h"
#include "ncnf_int.h"
#include "ncnf_cr.h"
#include "ncnf_vr.h"
#include "ncnf_policy.h"
#include "ncnf.h"
#include "ncnf_app_int.h"


/*
 * All this overridable debug crap.
 */
static void _ncnf_default_debug_print_function(int, const char *, va_list)
	__attribute__ ((format (printf, 2, 0) ));

static void
_ncnf_default_debug_print_function(int critical, const char *fmt, va_list ap) {
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

void (*ncnf_debug_print_function)(int is_critical, const char *fmt, va_list)
	= _ncnf_default_debug_print_function;

void
_ncnf_debug_print(int critical, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	if(ncnf_debug_print_function)
		ncnf_debug_print_function(critical, fmt, ap);
	va_end(ap);
}



/*
 * Callback to destroy inserts shadow structures when configuration
 * file is fully read.
 */
static int
_ncnf_remove_inserts_callback(struct ncnf_obj_s *obj, void *key) {
	(void)key;

	obj->mark = 0;

	if(_NOBJ_CONTAINER(obj))
		_ncnf_coll_clear(obj->mr,
			&obj->m_collection[COLLECTION_INSERTS], 1);

	return 0;
}

static struct asynchronous_validation_state {
	enum {
		AVS_NOSTATE,
		AVS_INPROGRESS,
		AVS_FAILED,
		AVS_SUCCEEDED
	} state;

	pid_t validator_pid;	/* Valid if(state == AVS_INPROGRESS). */

	struct sigaction old_sigchld;
} _asyncval;
static int _fire_async_validation(const char *validator, const char *fname);

ncnf_obj *
ncnf_read(const char *config_filename) {
	return ncnf_Read(config_filename, NCNF_ST_FILENAME);
}


ncnf_obj *
ncnf_Read(const char *data, enum ncnf_source_type stype, ...) {
	struct ncnf_obj_s *root = NULL;
	int no_dynamic_validation	= (stype & NCNF_FL_NODYN);
	int no_embedded_validation	= (stype & NCNF_FL_NOEMB);
	int async_validation		= (stype & NCNF_FL_ASYNCVAL);
	int relaxed_ns			= (stype & NCNF_FL_RELNS);
	int strip_with_ncql		= (stype & NCNF_FL_EXTNCQL);
	char *ncql_qfile = 0;
	char *ncql_proc = 0;
	char *ncql_conf = 0;
	va_list ap;
	int ret;

	/* Get rid of NCNF_FL stuff from the source type indicator */
	stype &= ~(NCNF_FL_NODYN | NCNF_FL_NOEMB
		| NCNF_FL_ASYNCVAL | NCNF_FL_RELNS | NCNF_FL_EXTNCQL);

	/* BGZ#1988 */
	if(strip_with_ncql) {
		va_start(ap, stype);
		ncql_qfile = va_arg(ap, char *);
		ncql_proc = va_arg(ap, char *);
		ncql_conf = va_arg(ap, char *);
		va_end(ap);
	}

	/*
	 * Fire asynchronous validation.
	 */
	if((stype == NCNF_ST_FILENAME)
	&& (async_validation
	|| (strip_with_ncql && _param_reload_ncnf_validator_ncql))) {
		/*
		 * Check if the previous validation has finished properly.
		 */
		switch(_asyncval.state) {
		case AVS_NOSTATE:
			if(_fire_async_validation(
				ncql_proc ? ncql_proc :
					_param_reload_ncnf_validator,
				data))
				break;	/* Proceed as it weren't enabled */
			/* Fall through */
		case AVS_INPROGRESS:
			errno = EAGAIN;	/* Try again later */
			return NULL;
		case AVS_FAILED:
			/* Validation failed! */
			_asyncval.state = AVS_NOSTATE;
			errno = EINVAL;
			return NULL;
		case AVS_SUCCEEDED:
			_asyncval.state = AVS_NOSTATE;
			/* Disable internal validation */
			no_dynamic_validation = NCNF_FL_NODYN;
			no_embedded_validation = NCNF_FL_NOEMB;
			break;
		}
		/* Fall through */
	}

	/*
	 * Attempt to build the configuration tree
	 * by reading configuration file.
	 */
	do {
		if(ncql_conf && _asyncval.state == AVS_SUCCEEDED) {
			/* Read in the processed file */
			ret = _ncnf_cr_read(ncql_conf, NCNF_ST_FILENAME,
					&root, relaxed_ns);
			if(ret == 0) {
				no_dynamic_validation = NCNF_FL_NODYN;
				no_embedded_validation = NCNF_FL_NOEMB;
				break;
			}
			_ncnf_debug_print(0,
				"Warning: %s cannot be read, "
				"falling back to %s",
				ncql_conf, data);
			/* Fall back into the full configuration file reading */
		}

		ret = _ncnf_cr_read(data, stype, &root, relaxed_ns);
		if(ret != 0)
			return NULL;
	} while(0);

	/*
	 * Scan down the tree resolving all references.
	 */
	if(_ncnf_cr_resolve(root, relaxed_ns) == -1) {
		_ncnf_obj_destroy(root);
		return NULL;
	}

	/*
	 * Remove temporary inserts structures.
	 */
	_ncnf_walk_tree((struct ncnf_obj_s *)root,
		_ncnf_remove_inserts_callback, NULL);

	/*
	 * Dynamic validation against .vr (validator rules) file.
	 */
	while(!no_dynamic_validation) {
		char *filename;
		struct vr_config *vc;

		filename = ncnf_get_attr((ncnf_obj *)root,
			"_validator-rules");
		if(filename == NULL) {
			break;
		} else if(*filename != '/'
			&& stype == NCNF_ST_FILENAME
			&& strchr(data, '/')) {
			char *newfname;
			char *p;

			/*
			 * Prepend configuration path to the relative
			 * validator-rules filename, so
			 * ../path/config.conf will become
			 * ../path/config.vr
			 */
			newfname = alloca(strlen(data)
				+ strlen(filename) + 1);
			strcpy(newfname, data);
			p = strrchr(newfname, '/');
			p++;
			strcpy(p, filename);
			filename = newfname;
		}

		vc = ncnf_vr_read(filename);
		if(vc == NULL) {
			if(errno == ENOENT) {
				_ncnf_debug_print(0,
			"Warning: File with validator rules %s not found: %s",
				filename, strerror(errno));
				break;
			}
			_ncnf_debug_print(1,
				"Can't parse validation rules in file %s",
				filename);
			ncnf_destroy((ncnf_obj *)root);
			return NULL;
		}

		ret = ncnf_validate(root, vc);
		ncnf_vr_destroy(vc);
		vc = NULL;
		if(ret != 0) {
			_ncnf_debug_print(1,
				"%s validation against %s failed",
				stype==NCNF_ST_TEXT?"NCNF data":data,
				filename);
			ncnf_destroy((ncnf_obj *)root);
			return NULL;
		}

		break;
	}

	/*
	 * Validation against embedded policies.
	 */
	if(!no_embedded_validation) {
		int yes;

		/*
		 * Check if embedded validator is not disabled.
		 */
		if(ncnf_get_attr_int(root, "_validator-embedded", &yes) == 0
			&& yes) {
			/*
			 * Only invoke embedded validator if
			 * there is a "_validator-embedded"
			 * attribute defined to a non-zero value.
			 */
			if(ncnf_policy(root)) {
				_ncnf_debug_print(1,
					"Failed to check the configuration "
					"against the hardcoded policy");
				ncnf_destroy((ncnf_obj *)root);
				return NULL;
			}
		}
	}

	return (ncnf_obj *)root;
}

ncnf_obj *
ncnf_obj_parent(ncnf_obj *objp) {
	struct ncnf_obj_s *obj = objp;

	if(obj == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if(obj->parent == NULL)
		errno = 0;

	return obj->parent;
}

void
ncnf_destroy(ncnf_obj *obj_p) {
	struct ncnf_obj_s *obj = (struct ncnf_obj_s *)obj_p;
	if(obj == NULL)
		return;

	_ncnf_notify_everyone(obj, NCNF_OBJ_DESTROY);
	_ncnf_obj_destroy(obj);
}

/*
 * Objects and iterators
 */

ncnf_obj *
ncnf_get_obj(ncnf_obj *root,
	const char *opt_type, const char *opt_name,
		enum ncnf_get_style style) {

	if(root == NULL) {
		errno = EINVAL;
		return NULL;
	}

	return (ncnf_obj *)_ncnf_get_obj((struct ncnf_obj_s *)root,
		opt_type, opt_name, style, _NGF_NOFLAGS);
}

ncnf_obj *
ncnf_obj_real(ncnf_obj *ref_obj_p) {
	struct ncnf_obj_s *ref_obj = ref_obj_p;

	if(ref_obj == NULL)
		return NULL;

	/*
	 * Resolve reference.
	 */
	ref_obj = _ncnf_real_object(ref_obj);

	return (ncnf_obj *)ref_obj;
}

void
ncnf_iter_rewind(ncnf_obj *iter) {
	if(iter == NULL)
		return;

	_ncnf_iter_rewind((struct ncnf_obj_s *)iter);
}

ncnf_obj *
ncnf_iter_next(ncnf_obj *iter) {

	if(iter == NULL) {
		errno = EINVAL;
		return NULL;
	}

	return (ncnf_obj *)_ncnf_iter_next(iter);
}

int
ncnf_walk_tree(ncnf_obj *objp, int (*callback)(ncnf_obj *, void *),
	void *key) {
	if(objp == NULL || callback == NULL) {
		errno = EINVAL;
		return -1;
	}

	return _ncnf_walk_tree((struct ncnf_obj_s *)objp,
		(int (*)(struct ncnf_obj_s *, void *))(callback),
		key);
}

int
ncnf_construct_path(ncnf_obj *obj, char *delim,
		int rev_order, char *(*name_func)(ncnf_obj *obj),
		char *buf, int size) {
	int wrote = 0;
	ncnf_obj *o;
	char *name;

	if(obj == NULL || delim == NULL || buf == NULL || size <= 0) {
		errno = EINVAL;
		return -1;
	}

	if(!name_func)
		name_func = ncnf_obj_name;

#define	PUSH(ch) do{if(size>1){*buf++=ch;size--;}wrote++;}while(0)
#define	PUSH2(ch) do{if(shift<size)buf[shift]=ch;shift++;}while(0)

	if(rev_order) {

		for(o = obj;
			o && (name = name_func(o));
				o = ncnf_obj_parent(o)) {
			if(o != obj) {
				char *dlm;
				for(dlm = delim; *dlm; dlm++)
					PUSH(*dlm);
			}
			for(; *name; name++)
				PUSH(*name);
		}

		*buf = '\0';	/* NUL-terminate buffer */

	} else {
		int shift = 0;
		int dlen = strlen(delim);
		int nlen;

		/*
		 * Compute the length of the future path string.
		 */
		for(o = obj;
			o && (name = name_func(o));
				o = ncnf_obj_parent(o)) {
			if(o != obj)
				shift += dlen;
			shift += strlen(name);
		}

		wrote = shift;
		if(shift < size)
			buf[shift] = '\0';

		/*
		 * Fill the string from the end using computed length.
		 * Althoug it is filled from the right to the left,
		 * separate tokens are filled from the left to the right,			 * that's why we double "shift -= xxx" stuff.
		 */
		for(o = obj;
			o && (name = name_func(o));
				o = ncnf_obj_parent(o)) {
			if(o != obj) {
				char *dlm;
				shift -= dlen;
				for(dlm = delim; *dlm; dlm++)
					PUSH2(*dlm);
				shift -= dlen;
			}
			nlen = strlen(name);
			shift -= nlen;
			for(; *name; name++)
				PUSH2(*name);
			shift -= nlen;
		}

		buf[size - 1] = '\0';	/* NUL-terminate buffer */
	}

	return wrote;
}


/*
 * Names
 */
char *
ncnf_obj_type(ncnf_obj *objp) {
	struct ncnf_obj_s *obj = objp;

	if(obj == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if(obj->type == NULL)
		errno = 0;	/* No error */

	return obj->type;
}

char *
ncnf_obj_name(ncnf_obj *objp) {
	struct ncnf_obj_s *obj = objp;

	if(obj == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if(obj->value == NULL)
		errno = 0;	/* No error */

	return obj->value;
}


/*
 * Attributes
 */

char *
ncnf_get_attr(ncnf_obj *objp, char *opt_type) {
	struct ncnf_obj_s *obj = objp;

	if(obj == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if(opt_type == NULL)
		return obj->type;

	return _ncnf_get_attr(obj, opt_type);
}

int
ncnf_get_attr_int(ncnf_obj *obj, char *type, int *r) {
	char *s;

	if(type == NULL || r == NULL) {
		errno = EINVAL;
		return -1;
	}

	s = ncnf_get_attr(obj, type);
	if(s == NULL)
		return -1;

	if((*s >= '0' && *s <= '9') || *s == '-') {
		*r = atoi(s);
	} else {
		if(strcmp(s, "on") == 0
		  || strcmp(s, "yes") == 0
		  || strcmp(s, "true") == 0
		) {
			*r = 1;
		} else if(strcmp(s, "off") == 0
			|| strcmp(s, "no") == 0
			|| strcmp(s, "false") == 0
		) {
			*r = 0;
		} else {
			return -1;
		}
	}

	return 0;
}

long
ncnf_get_attr_long(ncnf_obj *obj, char *type, long *r) {
	char *s;

	if(type == NULL || r == NULL) {
		errno = EINVAL;
		return -1;
	}

	s = ncnf_get_attr(obj, type);
	if(s == NULL)
		return -1;

	if((*s >= '0' && *s <= '9') || *s == '-') {
		*r = atol(s);
	}

	return 0;
}

int
ncnf_get_attr_double(ncnf_obj *obj, char *type, double *r) {
	char *s;

	if(type == NULL || r == NULL) {
		errno = EINVAL;
		return -1;
	}

	s = ncnf_get_attr(obj, type);
	if(s == NULL)
		return -1;

	*r = atof(s);

	return 0;
}

int
ncnf_get_attr_ip(ncnf_obj *obj, char *type, struct in_addr *r) {
	char *s;

	if(type == NULL || r == NULL) {
		errno = EINVAL;
		return -1;
	}

	s = ncnf_get_attr(obj, type);
	if(s == NULL)
		return -1;

	if(inet_aton(s, r) != 1)
		return -1;

	return 0;
}


int
ncnf_get_attr_ipport(ncnf_obj *obj, char *type, struct in_addr *rip, unsigned short *rhport) {
	char *s;
	char *port;
	int ret;

	if(type == NULL || rip == NULL || rhport == NULL) {
		errno = EINVAL;
		return -1;
	}

	s = ncnf_get_attr(obj, type);
	if(s == NULL)
		/* ESRCH */
		return -1;

	port = strchr(s, ':');
	*rhport = port ? atoi(port + 1) : 0;

	if(port) *port = '\0';	/* mask ':' */
	ret = inet_aton(s, rip);
	if(port) *port = ':';	/* unmask ':' */

	if(!ret) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * User data
 */

/* Attach userdata, detaching previous */
int
ncnf_udata_attach(ncnf_obj *objp, void *user_data) {
	struct ncnf_obj_s *obj = (struct ncnf_obj_s *)objp;
	void *old_data;

	if(obj == NULL) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Notify the notificator that the data gone, if there was some data.
 	 */

	if(obj->user_data && obj->notify) {
		if(obj->notify(objp,
			NCNF_UDATA_DETACH,
				obj->notify_key) == -1) {
			errno = EPERM;
			return -1;
		}
	}

	old_data = obj->user_data;
	obj->user_data = user_data;

	/*
	 * Notify the notificator that the new data is attached. 
 	 */

	if(user_data && obj->notify) {
		if(obj->notify(objp,
			NCNF_UDATA_ATTACH,
				obj->notify_key) == -1) {
			obj->user_data = old_data;
			errno = EPERM;
			return -1;
		}
	}

	return 0;
}

int
ncnf_notificator_attach(ncnf_obj *objp,
	int (*notify)(ncnf_obj *obj, enum ncnf_notify_event, void *key),
		void *key) {
	struct ncnf_obj_s *obj = (struct ncnf_obj_s *)objp;
	int (*old_notify)(ncnf_obj *, enum ncnf_notify_event, void *);
	void *old_notify_key;

	if(obj == NULL) {
		errno = EINVAL;
		return -1;
	}

	old_notify = obj->notify;
	old_notify_key = obj->notify_key;
	obj->notify = NULL;

	if(old_notify) {
		/* Report detach to the previous notificator */
		if(old_notify(objp, NCNF_NOTIF_DETACH, old_notify_key) == -1) {
			/* DETACH denied */
			obj->notify = old_notify;
			obj->notify_key = old_notify_key;
			errno = EPERM;
			return -1;
		}
	}

	obj->notify = notify;
	obj->notify_key = key;

	/* Report attach to the new notificator */
	if(obj->notify) {
		if(notify(objp, NCNF_NOTIF_ATTACH, key) == -1) {
			obj->notify = NULL;
			obj->notify_key = NULL;
			errno = EPERM;
			return -1;
		}
	}

	return 0;
}

int
ncnf_lazy_notificator(ncnf_obj *objp, char *watchfor,
	int (*notify)(ncnf_obj *obj, enum ncnf_notify_event, void *key),
		void *key) {
	if(objp == NULL) {
		errno = EINVAL;
		return -1;
	}

	return _ncnf_lazy_notificator((struct ncnf_obj_s *)objp,
		watchfor, notify, key);
}

	
/* Get the attached user data */
void *
ncnf_udata_get(const ncnf_obj *objp) {
	if(objp) {
		return ((const struct ncnf_obj_s *)objp)->user_data;
	} else {
		errno = EINVAL;
		return NULL;
	}
}
 
/*
 * Diff the trees.
 */
int
ncnf_diff(ncnf_obj *old_treep, ncnf_obj *new_treep) {
	struct ncnf_obj_s *old_tree = (struct ncnf_obj_s *)old_treep;
	struct ncnf_obj_s *new_tree = (struct ncnf_obj_s *)new_treep;

	if(old_tree == NULL || new_tree == NULL) {
		errno = EINVAL;
		return -1;
	}

	return _ncnf_diff(old_tree, new_tree);
}

/*
 * Dump the whole tree.
 */
void
ncnf_dump(FILE *f, ncnf_obj *obj, const char *flatten_type, int marked_only, int verbose, int indent) {
	int recursive_size = 0;
	if(obj == NULL)
		return;
	if(f == NULL)
		f = stdout;
	_ncnf_obj_dump_recursive(f, (struct  ncnf_obj_s *)obj,
		flatten_type, marked_only,
		verbose, 0, indent, 0, &recursive_size);
	if(verbose)
		fprintf(f, "# TOTAL RSIZE=%d\n", recursive_size);
}

int
ncnf_obj_marked(ncnf_obj *obj) {
	if(obj) {
		return obj->mark?1:0;
	} else {
		errno = EINVAL;
		return -1;
	}
}

int
ncnf_obj_line(ncnf_obj *obj) {
	if(obj) {
		return obj->config_line;
	} else {
		return 0;
	}
}

static void
_async_sigchld_callback(int sig) {
	int status;
	pid_t pid;

	assert(sig == SIGCHLD);
	assert(_asyncval.state == AVS_INPROGRESS);

	do {
		pid = waitpid(_asyncval.validator_pid, &status,
			WNOHANG | WUNTRACED);
	} while(pid == -1 && errno == EINTR);

	if(pid != _asyncval.validator_pid) return;

	/* Restore old handler */
	sigaction(sig, &_asyncval.old_sigchld, 0);

	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == 0) {
			_asyncval.state = AVS_SUCCEEDED;
		} else {
			_asyncval.state = AVS_FAILED;
		}
	} else {
		_asyncval.state = AVS_FAILED;
		if(WIFSTOPPED(status)) kill(pid, SIGKILL);	/* Terminate */
	}

	/*
	 * Maybe we have missed an important child,
	 * let the parent gather its dead relatives.
	 */
	(void)raise(SIGCHLD);

	/* Issue a "config reload" command again. */
	(void)raise(SIGHUP);
}

static int
_fire_async_validation(const char *validator_proc, const char *fname) {
	ncnf_sf_svect *sv;
	struct sigaction new_sigchld;
	sigset_t set, oset;
	size_t i;

	assert(_asyncval.state == AVS_NOSTATE);
	if(_asyncval.state != AVS_NOSTATE)	/* Just in case */
		return -1;

	sv = ncnf_sf_split(validator_proc, " \n\r\t", 0);
	if(!sv) return -1;
	if(sv->count == 0) {
		ncnf_sf_sfree(sv);
		return -1;
	}

	_asyncval.validator_pid = 0;
	_asyncval.state = AVS_INPROGRESS;

	/*
	 * Block SIGCHLD temporarily.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, &oset);

	/*
	 * Install our own child signal handler.
	 */
	memset(&new_sigchld, 0, sizeof(new_sigchld));
	new_sigchld.sa_handler = _async_sigchld_callback;
	new_sigchld.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &new_sigchld, &_asyncval.old_sigchld)) {
		_asyncval.state = AVS_NOSTATE;
		ncnf_sf_sfree(sv);
		return -1;
	}

	/*
	 * Fire the validator.
	 */
	_asyncval.validator_pid = fork();
	switch(_asyncval.validator_pid) {
	case -1:
		(void)sigaction(SIGCHLD, &_asyncval.old_sigchld, 0);
		_asyncval.state = AVS_NOSTATE;
		sigprocmask(SIG_SETMASK, &oset, 0);
		ncnf_sf_sfree(sv);
		return -1;
	case 0:	/* Child (validator) process */
		for(i = 0; i < sv->count; i++) {
			if(strcmp(sv->list[i], "$config_file") == 0) {
				sv->list[i] = strdup(fname);
				if(!sv->list[i]) _exit(127);
			}
		}
		execv(sv->list[0], sv->list);
		_exit(127);
		/* UNREACHABLE */
	default:	/* Parent */
		sigprocmask(SIG_SETMASK, &oset, 0);
		ncnf_sf_sfree(sv);
		return 0;
	}
}
