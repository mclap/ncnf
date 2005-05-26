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

/* pidfile manual handler */
static void (*pf_handler)(int pfd, const char *filename);

void
__na_pidfile_manual_handler(
        void (*onUnload)(int pfd, const char *filename)) {
	assert(!pf_handler);
	pf_handler = onUnload;
}

static int
__na_trylock(int fd, int timeout) {
	int ret;
	int warned = 0;

	struct flock flk;

	flk.l_start = 0;	/* lock from start */
	flk.l_len = 0;	/* till the end */
	flk.l_type = F_WRLCK;
	flk.l_whence = 0;

	do {

		ret = fcntl(fd, F_SETLK, &flk);
		if(ret == 0)
			return 0;
		assert(ret == -1);
		if(errno != EAGAIN)
			return -1;

		if(((warned++) % 30) == 0) {
			_ncnf_debug_print(0,
			"Waiting %d for the previous process to exit...",
			timeout
			);
		}
		sleep(1);
	} while(--timeout);

	errno = ETIMEDOUT;
	return -1;
}

int
__na_pidfile_update(ncnf_obj *process, pid_t update_pid) {
        ncnf_obj *iter;
        ncnf_obj *pfo;  /* pidfile object */

        if(process == NULL || strcmp(ncnf_obj_type(process), "process")) {
                errno = EINVAL;
                return -1;
        }

        iter = ncnf_get_obj(process, "pidfile", NULL, NCNF_ITER_ATTRIBUTES);
        while((pfo = ncnf_iter_next(iter))) {
		struct ncnf_obj_s *pfo_int = pfo;
		int pfd;

		if(pfo_int->notify != __na_pidfile_notificator)
			/* Incorrectly initialized object, or not ours */
			continue;
		pfd = ((int)pfo_int->notify_key) - 1;
		if(pfd <= 0)
			/* Not bound to specific FD */
			continue;

		__na_write_pid_file(pfd, update_pid);
        }

        ncnf_destroy(iter);

        return 0;
}

int
__na_write_pid_file(int pf, pid_t opt_pid) {
	char _buf[32];
	int ret, len;
	struct flock flk;

	assert(pf != -1);

	/*
	 * Lock it again.
	 */
	flk.l_start = 0;	/* lock from start */
	flk.l_len = 0;		/* till the end */
	flk.l_type = F_WRLCK;
	flk.l_whence = 0;
	(void)fcntl(pf, F_SETLK, &flk);

	if(opt_pid) {
		len = snprintf(_buf, sizeof(_buf), "%lu\n",
			(unsigned long)opt_pid);
		assert(len < sizeof(_buf));
		if(len >= sizeof(_buf))
			return -1;

		if(lseek(pf, 0, SEEK_SET) != 0) {
			errno = EIO;
			return -1;
		}

		ftruncate(pf, 0);

		do {
			ret = write(pf, _buf, len);
		} while(ret == -1 && errno == EINTR);
		if(ret != len) {
			if(ret > 0) {
				/*
				 * Do not allow others to use partial file.
				 */
				ftruncate(pf, 0);
			}
			errno = EIO;
			return -1;
		}

	} else {
		ftruncate(pf, 0);
	}

	fsync(pf);

	return 0;
}

int
__na_make_pid_file(char *pidfile) {
	struct stat sb;
	struct flock flk;
	char buf[32] = { '\0' };
	int pf;
	int ret;
	int open_flags = O_RDWR | O_CREAT;

	if(!pidfile) {
		errno = EINVAL;
		return -1;
	}

	if(*pidfile == '\0') {
		errno = 0;
		return -1;
	}

	ret = stat(pidfile, &sb);
	if(ret == -1) {
		if(errno != ENOENT)
			return -1;

		open_flags |= O_EXCL;
	} else {
		if((sb.st_mode & S_IFMT) != S_IFREG) {
			_ncnf_debug_print(1, "%s: Inappropriate file type",
				pidfile);
			errno = EPERM;
			return -1;
		}
	}

	/*
	 * Open the PID file
	 */

	pf = open(pidfile, open_flags, 0644);
	if(pf == -1) {
		_ncnf_debug_print(1,
			"Can't open PID file %s: %s",
				pidfile, strerror(errno));
		return -1;
	}

	/*
	 * Set close-on-exec flag.
	 */

	ret = fcntl(pf, F_GETFD, 0);
	if(ret == -1) {
		close(pf);
		_ncnf_debug_print(1,
			"Can't get flags for %s: %s",
				pidfile, strerror(errno));
		return -1;
	}
	if(fcntl(pf, F_SETFD, ret | FD_CLOEXEC) == -1) {
		close(pf);
		_ncnf_debug_print(1,
			"Can't set close-on-exec flag for %s: %s",
				pidfile, strerror(errno));
		return -1;
	}

	/*
	 * Lock the file.
	 */

	flk.l_start = 0;	/* lock from start */
	flk.l_len = 0;	/* till the end */
	flk.l_type = F_WRLCK;
	flk.l_whence = 0;

	do {
		ret = fcntl(pf, F_SETLK, &flk);
	} while(ret == -1 && errno == EINTR);
	if(ret != -1) {
		if(__na_write_pid_file(pf, getpid())) {
			_ncnf_debug_print(1,
				"Can't write PID file %s", pidfile);
			return -1;
		}

		/* Don't close the file descriptor in order to save lock. */

		return pf;
	}

	if(errno != EACCES && errno != EAGAIN) {
		/*
		 * Unknown problem during locking.
		 */
		_ncnf_debug_print(1,
			"Can't lock PID file %s: %s",
				pidfile, strerror(errno));
		close(pf);
		errno = EPERM;
		return -1;
	}

	/*
	 * Try to read the contents of the file.
	 */
	do {
		ret = read(pf, buf, sizeof(buf));
	} while(ret == -1 && errno == EINTR);
	if(ret == -1 || (ret > 0 && buf[ret -1] != '\n')) {
		/*
		 * Something wrong here: error or partial contents.
		 */
		_ncnf_debug_print(1,
			"Can't start: another instance running");
	} else if(ret >= 0) {
		/*
		 * Somebody locked the file and wrote a getpid() in it.
		 */
		buf[ret - 1] = '\0';
		ret = fcntl(pf, F_GETLK, &flk);
		if(ret == -1) {
			_ncnf_debug_print(1,
			"Can't start: Problem getting pid file "
			"lock information, data=[%s]", buf);
		} else {
			long pif;	/* pid in file */
			pif = strtol(buf, NULL, 10);
			if(flk.l_type == F_UNLCK) {
				/* Suddenly, not locked anymore! */
				_ncnf_debug_print(1,
				"Can't start: another instance "
				"almost running (\"%s\")", buf);
			} else if(*buf == 0
				/* Tested new behavior and old one */
				|| strcmp(buf, "finishing") == 0) {

				/*
				 * Wait until the previous process finishes.
				 */
				ret = __na_trylock(pf, 5 * 60);
				if(ret == -1) {
					_ncnf_debug_print(1,
					"Can't start: another instance "
					"still running: %s",
					strerror(errno));
				} else if(__na_write_pid_file(pf, getpid())) {
					_ncnf_debug_print(1,
					"Can't write PID file %s",
					pidfile);
				} else {
					return pf;
				}
			} else {
				if(pif != flk.l_pid) {
					_ncnf_debug_print(1,
					"Can't start: another instance running,"
					" pid in file=%s, lock pid=%lu",
					buf, (unsigned long)flk.l_pid);
				} else {
					_ncnf_debug_print(1,
					"Can't start: another instance running,"
					" pid=%lu",
					(unsigned long)flk.l_pid);
				}
			}
		}
	}

	close(pf);
	errno = EPERM;
	return -1;
}


void
__na_default_pidfile_open_failed_callback(char *filename, int is_firsttime) {
	_ncnf_debug_print(is_firsttime?1:0, "Pidfile %s open failed: %s",
		filename,
		strerror(errno));
	if(is_firsttime)
		exit(EX_TEMPFAIL);
}


int
__na_pidfile_notificator(ncnf_obj *obj, enum ncnf_notify_event event,
		void *key) {
	static int firsttime = 1;
	int old_pidfd = ((int)key) - 1;
	int pf;
	char *filename;

	switch(event) {
	case NCNF_NOTIF_ATTACH:
		return 0;
	case NCNF_NOTIF_DETACH:
		if(old_pidfd != -1)
			__na_write_pid_file(old_pidfd, getpid());
		/* Refusing to detach myself */
		return -1;
	case NCNF_UDATA_DETACH:
	case NCNF_UDATA_ATTACH:
		return 0;
	case NCNF_OBJ_ADD:

		if(pf_handler) {
			/* Manual override requested. */
			break;
		}

		filename = ncnf_obj_name(obj);

		/*
		 * Append sysid if filename is a directory name.
		 */
		if(filename && filename[0]
			&& filename[strlen(filename) - 1] == '/') {
			bstr_t sysid = NCNF_APP_construct_id(ncnf_obj_parent(obj));
			if(sysid) {
				filename = alloca(
					strlen(ncnf_obj_name(obj))
					+ bstr_len(sysid)
					+ sizeof(".pid"));
				assert(filename);
				strcpy(filename, ncnf_obj_name(obj));
				strcat(filename, sysid);
				strcat(filename, ".pid");
				bstr_free(sysid);
			}
		}

		pf = __na_make_pid_file(filename);
		if(pf == -1 && errno != 0) {
			if(NCNF_APP_pidfile_open_failed_callback)
			NCNF_APP_pidfile_open_failed_callback(filename,
				firsttime);
			return -1;
		}
		ncnf_notificator_attach(obj,
			__na_pidfile_notificator,
			(void *)(pf + 1));
		break;
	case NCNF_OBJ_DESTROY:
		if(old_pidfd != -1) {
			if(pf_handler) {
				pf_handler(old_pidfd, ncnf_obj_name(obj));
			} else {
				__na_write_pid_file(old_pidfd, 0);
				close(old_pidfd);
			}
		}
		break;
	default:
		/* Do nothing */
		break;
	}

	if(strcmp(ncnf_obj_type(obj), "process") == 0)
		firsttime = 0;

	return 0;
}


/* Current value of "reload-ncnf-validator" setting */
char *_param_reload_ncnf_validator;
int _param_reload_ncnf_validator_ncql = 1;

/*
 * React on change in "reload-ncnf-validator" setting.
 */
int
__na_reload_ncnf_validator_notificator(ncnf_obj *obj, enum ncnf_notify_event event, void *key) {
	switch(event) {
	case NCNF_OBJ_ADD:
		ncnf_notificator_attach(obj,
			__na_reload_ncnf_validator_notificator,
			NULL);
		/* Fall through */
	case NCNF_OBJ_CHANGE:
		_param_reload_ncnf_validator = ncnf_obj_name(obj);
		break;
	case NCNF_OBJ_DESTROY:
		_param_reload_ncnf_validator = 0;
		break;
	default:
		break;
	}

	return 0;
}


/*
 * React on change in "reload-ncnf-validator-ncql" setting.
 */
int
__na_reload_ncnf_validator_ncql_notificator(ncnf_obj *obj, enum ncnf_notify_event event, void *key) {
	switch(event) {
	case NCNF_OBJ_ADD:
		ncnf_notificator_attach(obj,
			__na_reload_ncnf_validator_ncql_notificator,
			NULL);
		/* Fall through */
	case NCNF_OBJ_CHANGE:
		ncnf_get_attr_int(ncnf_obj_parent(obj),
			"reload-ncnf-validator-ncql",
			&_param_reload_ncnf_validator_ncql);
		break;
	case NCNF_OBJ_DESTROY:
		_param_reload_ncnf_validator_ncql = 1;
		break;
	default:
		break;
	}

	return 0;
}

