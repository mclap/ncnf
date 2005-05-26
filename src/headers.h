/*
 * Common system headers.
 */
#ifndef	__HEADERS_H__
#define	__HEADERS_H__

#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include <errno.h>
#include <assert.h>

#include "ncnf_sf_lite.h"
#ifdef	HAVE_LIBSTRFUNC
#include <strfunc.h>
#endif

#include "genhash.h"
#include "ncnf.h"

#endif	/* __HEADERS_H__ */
