/*
 * Netli Configuration library.
 *
 * This API provides a set of additional complex functions
 * built on top of Basic API.
 */
#ifndef	NCNF_APP_H
#define	NCNF_APP_H

#include <sys/types.h>	/* for pid_t */

#include "bstr.h"	/* Basic reference counted strings */
#include "ncnf.h"

/*
 * Fetch the process configuration entity from the configuration tree
 * by the given sysid or path inside the configuration tree.
 * If sysid, then checks sysid for validity (length).
 * 
 * Examples:
 * 	NCNF_APP_resolve_sysid(ncnf_root, "process@box@ploc");
 * 	NCNF_APP_resolve_path(ncnf_root, "ploc/box/process");
 *
 */
ncnf_obj *NCNF_APP_resolve_sysid(ncnf_obj *root, const char *sysid);
ncnf_obj *NCNF_APP_resolve_path(ncnf_obj *root, const char *config_path);

/*
 * Construct an identifier (entity@entity@...@...) of a given object.
 */
bstr_t NCNF_APP_construct_id(ncnf_obj *obj);

/*
 * The function does some basic initializations of the process environment:
 * Netli logging, pid file, etc.
 * Stores the open pid file descriptor in the static internal structures.
 * Installs a callback to catch changes of necessary attributes.
 *
 * process pointer may be obtained by using something like that:
 * ncnf_obj *process = NCNF_APP_resolve_path(root, "ploc/box/process");
 * Return -1 if initialization failed by some reason.
 */
int NCNF_APP_initialize_process(ncnf_obj *process);


/*
 * Override this function if you want to get better control over pid file
 * open failures. Right now it does exit(EX_TEMPFAIL) if there are problems
 * opening the pid file for the first time. (Second time may occur after
 * ncnf_diff() if "pidfile" attribute changed).
 */
extern void (*NCNF_APP_pidfile_open_failed_callback)(char *filename,
	int is_firsttime);


/*
 * Update pid files with the new pid value (i.e., after fork(2)/daemon(3)).
 */
int NCNF_APP_pidfile_update(ncnf_obj *process);

/*
 * Update pid files with the notice about process being terminated.
 */
int NCNF_APP_pidfile_finishing(ncnf_obj *process);

/* Lower level pidfile update function. */
int NCNF_APP_pidfile_write(int pfd, pid_t pid);

/*
 * Sets a _global_ flag to disable further pidfile handling, and request
 * pidfile notifications redirection upon configuration unload.
 * Used by some processes (ladmin) to handle pidfile manually, as part of
 * memory conservation tricks.
 */
void NCNF_APP_pidfile_manual_handler(
	void (*onUnload)(int pfd, const char *filename));


/*
 * Set uid, gid, etc. according to the process
 * configuration.
 */
enum ncnf_app_perm_set {
	NAPS_ALL	= -1,
	NAPS_SETGID	= (1 << 2),
	NAPS_SETUID	= (1 << 3),
};
int NCNF_APP_set_permissions(ncnf_obj *process, enum ncnf_app_perm_set set);

/***********************************************************************
 * Universal function to retrieve a list of configuration objects
 * at the specified configuration tree level.
 * 
 * Parameters are:
 *
 * start_level:	object we're willing to start at (root or any other object);
 * types_tree:	path of types which should be traversed to find an object;
 * opt_filter:	user-defined function to filter a specified level;
 * opt_key:	user-defined opaque data to be passed into filter.
 * 
 * opt_filter() may return the following values:
 *  0:	proceed with this object (level)
 * -1:	hard error found, stop completely and return NULL (errno should be set).
 *any:	don't proceed with this object (level)
 * 
 * EXAMPLE:
 *
 * int
 * func(ncnf_obj *nloc) {
 * 	ncnf_obj *iter;
 * 	iter = NCNF_APP_find_objects(nloc, "ploc/box/process/si",
 * 		filter, NULL);
 *	if(iter == NULL) {
 *		if(errno == ESRCH)
 *			return 0;
 * 		return -1;
 *	}
 * 	// Do something with this iterator
 * 	ncnf_destroy(iter);
 * 	return 0;
 * }
 * int 
 * filter(ncnf_obj *obj, void *key) {
 * 	if(strcmp(ncnf_obj_type(obj), "process") == 0) {
 *		char *process_type = ncnf_get_attr(obj, "process-type");
 * 		if(strcmp(process_type, "proxy"))
 * 			// Skip not proxies
 * 			return 1;
 * 	}
 * 	return 0;
 * }
 * 
 *
 * RETURN VALUES:
 *
 * Upon successful invocation, function will return an iterator
 * of found objects. You should destroy this iterator eventually
 * with ncnf_destroy();
 * 
 * ERRORS (when returned pointer is NULL):
 * 
 * [ESRCH]:	No objects found.
 * [ENOMEM]:	Memory allocation failed.
 * [EINVAL]:	Mandatory parameter missing or inappropriate object class.
 * other error codes may be returned if opt_filter() returns -1 and sets
 * its own error code.
 */

ncnf_obj *NCNF_APP_find_objects(ncnf_obj *start_level,
	char *types_tree,
	int (*opt_filter)(ncnf_obj *current_obj, void *key),
	void *opt_key);

#endif	/* __NCNF_APP_H__ */
