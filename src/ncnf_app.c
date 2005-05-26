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
#include "ncnf_app.h"
#include "ncnf_app_int.h"
#include "ncnf_int.h"
#include "ncnf_find.h"

/*
 * Fetch the entity from the tree by the given sysid.
 */
ncnf_obj *
NCNF_APP_resolve_sysid(ncnf_obj *root, const char *sysid) {
	ncnf_sf_svect *sv;
	int token;
	ncnf_obj *cur;

	/* Don't take nothing */
	if(root == NULL || sysid == NULL || sysid[0] == '\0') {
		errno = EINVAL;
		return NULL;
	}

	/* We do want only the root object */
	if(ncnf_obj_type(root)) {
		errno = EINVAL;
		return NULL;
	}

	/* Split path by tokens */
	sv = ncnf_sf_split(sysid, "@", 0);
	if(sv == NULL)
		return NULL;

	/* Walk down the tree in search of the next token */
	for(cur = root, token = sv->count - 1; token >= 0; token--) {
		cur = ncnf_get_obj(cur,
			NULL, sv->list[token],
			NCNF_FIRST_OBJECT);
		if(cur == NULL)
			break;
	}

	ncnf_sf_sfree(sv);

	if(cur == NULL) {
		errno = ESRCH;
		return NULL;
	} else if(cur == root) {
		errno = EINVAL;	/* Invalid empty path specified */
		return NULL;
	}

	return cur;
}

/*
 * Fetch the entity from the tree by the given path.
 */
ncnf_obj *
NCNF_APP_resolve_path(ncnf_obj *root, const char *config_path) {
	ncnf_sf_svect *sv;
	unsigned int token;
	ncnf_obj *cur;

	/* Don't take nothing */
	if(root == NULL || config_path == NULL || config_path[0] == '\0') {
		errno = EINVAL;
		return NULL;
	}

	/* We do want only the root object */
	if(ncnf_obj_type(root)) {
		errno = EINVAL;
		return NULL;
	}

	/* Split path by tokens */
	sv = ncnf_sf_split(config_path, "/", 0);
	if(sv == NULL)
		return NULL;

	/* Walk down the tree in search of the next token */
	for(cur = root, token = 0; token < sv->count; token++) {
		cur = ncnf_get_obj(cur,
			NULL, sv->list[token],
			NCNF_FIRST_OBJECT);
		if(cur == NULL)
			break;
	}

	ncnf_sf_sfree(sv);

	if(cur == NULL) {
		errno = ESRCH;
		return NULL;
	} else if(cur == root) {
		errno = EINVAL;	/* Invalid empty path specified */
		return NULL;
	}

	return cur;
}


static void
_figure_out_ids(ncnf_obj *process, uid_t *uid, gid_t *gid) {
	int i;

	if(ncnf_get_attr_int(process, "uid", &i) == 0)
		*uid = i;
	else
		*uid = -1;

	if(ncnf_get_attr_int(process, "gid", &i) == 0)
		*gid = i;
	else
		*gid = -1;

}

/*
 * Do basic initialization of the process environment.
 */
int
NCNF_APP_initialize_process(ncnf_obj *process) {
	char *s;
	int ret = 0;
	uid_t new_uid = -1;
	gid_t new_gid = -1;
	uid_t saved_euid = -1;
	gid_t saved_egid = -1;
	int tmp;


	if(process == NULL
		|| ncnf_obj_type(process) == NULL
		|| strcmp(ncnf_obj_type(process), "process")) {
		errno = EINVAL;
		return -1;
	}


	/*
	 * Figure out the effective ID's.
	 */
	_figure_out_ids(process, &new_uid, &new_gid);

	/*
	 * Create temporary process permissions.
	 */
	if(new_gid != -1) {
		saved_egid = getegid();
		setegid(new_gid);
	}
	if(new_uid != -1) {
		saved_euid = geteuid();
		seteuid(new_uid);
	}

	s = ncnf_get_attr(process, "chroot");
	if(s && chroot(s)) {
		_ncnf_debug_print(0,
			"Chroot(\"%s\") failed: %s",
			s, strerror(errno)
		);
		ret = -1;
		goto finish;
	}

	s = ncnf_get_attr(process, "chdir");
	if(s && chdir(s)) {
		_ncnf_debug_print(0,
			"Chdir(\"%s\") failed: %s",
			s, strerror(errno)
		);
		ret = -1;
		goto finish;
	}

	/*
	 * Create and initialize the pid file.
	 */
	if(ncnf_lazy_notificator(process, "pidfile",
		__na_pidfile_notificator, NULL)) {
		/*
		 * If pidfile is not initialized,
		 * initialize logging anyway.
		 */
		/* EPERM? */
		ret = -1;
	}

	/*
	 * This parameter sets the name of a program used for
	 * asynchronous NCNF validation.
	 */
	(void)ncnf_lazy_notificator(process, "reload-ncnf-validator",
		__na_reload_ncnf_validator_notificator, NULL);
	(void)ncnf_lazy_notificator(process, "reload-ncnf-validator-ncql",
		__na_reload_ncnf_validator_ncql_notificator, NULL);

	/*
	 * Disable swapping and core dumping if configured so.
	 */
	if(ncnf_get_attr_int(process, "do-not-swap", &tmp)) {
		if(0) {
		_ncnf_debug_print(0,
			"do-not-swap is not given, skipping mlockall()");
		}
	} else if(tmp == 0) {
		_ncnf_debug_print(0,
			"do-not-swap is DISABLED, skipping mlockall()");
	} else if(
#ifdef	__linux__
		mlockall(MCL_CURRENT | MCL_FUTURE)
#else	/* __linux__ */
		1
#endif	/* __linux__ */
	) {
		_ncnf_debug_print(1,
			"Security measure failure: mlockall(): %s",
			strerror(errno));
		ret = -1;	/* Caller must exit() */
	} else {
		_ncnf_debug_print(0, "mlockall() performed");
	}

finish:

	/*
	 * Give away temporary permissions.
	 */

	if(saved_egid != -1)
		setegid(saved_egid);
	if(saved_euid != -1)
		seteuid(saved_euid);

	return ret;
}

bstr_t
NCNF_APP_construct_id(ncnf_obj *obj) {
	bstr_t b;

	b = str2bstr(NULL, 15);
	if(b) {
		int wrote = ncnf_construct_path(obj, "@", 1,
			ncnf_obj_name, b, bstr_len(b));
		if(wrote > bstr_len(b)) {
			bstr_free(b);
			b = str2bstr(NULL, wrote);
			if(b) {
				wrote = ncnf_construct_path(obj, "@", 1,
					ncnf_obj_name, b, bstr_len(b) + 1);
				assert(wrote <= bstr_len(b));
			}
		}
	}

	return b;
}

/*
 * Update pidfile when pid is being changed (after fork())
 */
int
NCNF_APP_pidfile_update(ncnf_obj *process) {
	return __na_pidfile_update(process, getpid());
}

/*
 * Update pidfile when process is finishing.
 */
int
NCNF_APP_pidfile_finishing(ncnf_obj *process) {
	return __na_pidfile_update(process, 0);
}

int
NCNF_APP_pidfile_write(int pfd, pid_t pid) {
	return __na_write_pid_file(pfd, pid);
}

void
NCNF_APP_pidfile_manual_handler(
	void (*onUnload)(int pfd, const char *filename)) {
	__na_pidfile_manual_handler(onUnload);
}


void (*NCNF_APP_pidfile_open_failed_callback)(char *filename, int is_firsttime)
	= __na_default_pidfile_open_failed_callback;


/*
 * Establish process permissions and environment.
 */
int
NCNF_APP_set_permissions(ncnf_obj *process, enum ncnf_app_perm_set set) {
	int was_all;

	if(process == NULL) {
		errno = EINVAL;
		return -1;
	}

	if(set == NAPS_ALL)
		was_all = 1;
	else
		was_all = 0;


	if(set & NAPS_SETGID) {
		int id_numeric = -1;
		int id_literal = -1;
		int i;
		char *s;

		if(ncnf_get_attr_int(process, "gid", &i) == 0)
			id_numeric = i;

		s = ncnf_get_attr(process, "group");
		if(s) {
			struct group *grp = getgrnam(s);
			if(grp)
				id_literal = grp->gr_gid;
		}

		if(id_numeric != -1 || id_literal != -1) {

			if(id_numeric != -1 && id_literal != -1) {
				if(id_numeric != id_literal)
					/* Numbers should be exactly the same, if set. */
					return -1;
			}

			i = (id_literal == -1) ? id_numeric : id_literal;

			if(setgid(i) == -1)
				return -1;
		}

		set &= ~NAPS_SETGID;
	}

	if(set & NAPS_SETUID) {
		int id_numeric = -1;
		int id_literal = -1;
		int i;
		char *s;

		if(ncnf_get_attr_int(process, "uid", &i) == 0)
			id_numeric = i;

		s = ncnf_get_attr(process, "user");
		if(s) {
			struct passwd *pw = getpwnam(s);
			if(pw)
				id_literal = pw->pw_uid;
		}

		if(id_numeric != -1 || id_literal != -1) {

			if(id_numeric != -1 && id_literal != -1) {
				if(id_numeric != id_literal)
					/* Numbers should be exactly the same, if set. */
					return -1;
			}

			i = (id_literal == -1) ? id_numeric : id_literal;

			if(setuid(i) == -1)
				return -1;
		}

		set &= ~NAPS_SETUID;
	}


	/*
	 * Check if all options were used.
	 */

	if(set && !was_all) {
		/* some options left */
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * Universal function to retrieve a list of configuration objects
 * at the specified configuration tree level.
 */
ncnf_obj *
NCNF_APP_find_objects(ncnf_obj *start_level,
	char *types_tree,
	int (*opt_filter)(ncnf_obj *, void *),
	void *opt_key) {

	if(start_level == NULL || types_tree == NULL) {
		errno = EINVAL;
		return NULL;
	}

	return (ncnf_obj *)_na_find_objects(
		(struct ncnf_obj_s *)start_level,
		types_tree,
		(int (*)(struct ncnf_obj_s *, void *))(opt_filter),
		opt_key);
}
