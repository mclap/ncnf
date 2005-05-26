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
#ifndef	__NCNF_APP_INT_H__
#define	__NCNF_APP_INT_H__

/*
 * Update the files with the specified opt_update_pid, or
 * empty these files, if (!opt_update_pid).
 */
int __na_pidfile_update(ncnf_obj *process, pid_t opt_update_pid);

/*
 * Write opt_pid to the file, or truncate the file, if (!opt_pid).
 */
int __na_write_pid_file(int pf, pid_t opt_pid);
int __na_make_pid_file(char *pidfile);

/* Override automatic pidfile handling */
void __na_pidfile_manual_handler(
	void (*onUnload)(int pfd, const char *filename));

/*
 * Callbacks.
 */
int __na_pidfile_notificator(ncnf_obj *, enum ncnf_notify_event, void *key);
void __na_default_pidfile_open_failed_callback(char *fname, int is_firsttime);
int __na_reload_ncnf_validator_notificator(ncnf_obj *, enum ncnf_notify_event, void *);
int __na_reload_ncnf_validator_ncql_notificator(ncnf_obj *, enum ncnf_notify_event, void *);

/* Current value of "reload-ncnf-validator" setting. */
extern char *_param_reload_ncnf_validator;
extern int _param_reload_ncnf_validator_ncql;

#endif	/* __NCNF_APP_INT_H__ */
