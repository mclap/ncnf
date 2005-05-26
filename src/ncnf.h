/*
 * The Netli Configuration library basic API.
 * This API contains the set of relatively low-level primitives.
 * Check out the ncnf_app.h to get a complement set of the higher level ones.
 */
#ifndef	NCNF_H
#define	NCNF_H

#include <stdio.h> /* define FILE */

typedef	struct ncnf_obj_s ncnf_obj;

/*
 * Parse specified configuration file.
 * 
 * When successful, this function returns the pointer to the
 * root of the configuration objects tree.
 * Upon error, the function will return NULL and set the errno.
 * 
 * The caller of the function is the owner of this objects tree
 * and should destroy it with ncnf_destroy() eventually.
 */
ncnf_obj *ncnf_read(const char *_config_filename);

/*
 * Universal configuration reading interface.
 * 
 * When success, this function returns the pointer to the
 * root of the configuration objects tree.
 * Upon error, the function will return NULL and set the errno.
 * 
 * The caller of the function is the owner of this objects tree
 * and should destroy eventually the object tree with ncnf_destroy().
 */
enum ncnf_source_type {
	NCNF_ST_FILENAME = 0,	/* Filename is passed */
	NCNF_ST_TEXT     = 1,	/* Text file is passed */
	// BGZ#1976: NCNF_ST_FD = 2, /* FD number in decimal ASCIIZ format */
	/* Flags could be applied also */
	NCNF_FL_NODYN    = 32,	/* Disable dynamic (.vr) validation */
	NCNF_FL_NOEMB    = 64,	/* Disable embedded policies validation */
	NCNF_FL_ASYNCVAL = 128,	/* Enable asynchronous validation */
	NCNF_FL_RELNS    = 256, /* Relaxed namespace (no duplicate checking) */
	NCNF_FL_EXTNCQL  = 512, /* BGZ#1988: -Q, -E and -G arguments */
};
ncnf_obj *ncnf_Read(const char *source, enum ncnf_source_type, ...);


/*
 * Number of styles used to fetch an object or object chain.
 */
enum ncnf_get_style {
  NCNF_FIRST_OBJECT	= 0,	/* Fetch the first matched object */
  NCNF_FIRST_ATTRIBUTE	= 1,	/* Fetch the first matched attribute */
  /* Request to return iterator, reentrant, newly allocated object. */
  NCNF_ITER_OBJECTS	= 2,	/* Get iterator for the group of objects */
  NCNF_ITER_ATTRIBUTES	= 3,	/* Get iterator for the group of attributes */
  /* Request to return simple chain, not reentrant and generally unsafe. */
  NCNF_CHAIN_OBJECTS	= 4,	/* Get the chain of objects */
  NCNF_CHAIN_ATTRIBUTES	= 5,	/* Get the chain of attributes */
};

/*
 * Search inside the given object, according to ncnf_get_style.
 * Optional opt_type and opt_name may be omitted.
 * 
 * Return NULL (errno = ESRCH, etc.) if not found.
 * 
 * If NCNF_ITER_* specified in ncnf_get_style, the returned
 * iterator is the special object and should eventually be
 * disposed by ncnf_destroy().
 *
 * If NCNF_CHAIN_* specified in ncnf_get_style, the returned
 * chain is formed from raw list of found objects, and should
 * considered as unsafe as the next ncnf_get_obj will probably
 * break this chain. Not reentrant.
 * The chain must NOT be deleted after use.
 */
ncnf_obj *ncnf_get_obj(ncnf_obj *obj,
	const char *opt_type, const char *opt_name,
	enum ncnf_get_style);

/*
 * Return object type or name (value) for most objects.
 * Return NULL if it is the configuration root or an iterator.
 */
char *ncnf_obj_type(ncnf_obj *obj);
char *ncnf_obj_name(ncnf_obj *obj);

/*
 * Obtain the object's parent object.
 * Return NULL if no parent or obj == NULL.
 */
ncnf_obj *ncnf_obj_parent(ncnf_obj *obj);

/*
 * Resolve the real object out of the reference.
 * If it is not a reference, return the same object.
 */
ncnf_obj *ncnf_obj_real(ncnf_obj *ref);


/***********************
* Iterators and Chains *
***********************/

/*
 * Functions to deal with iterators and chains returned by
 * ncnf_get_obj() with NCNF_ITER_* and NCNF_CHAIN_* styles.
 */

/* Get the next object in the iterator or chain. NULL if nothing is left. */
ncnf_obj *ncnf_iter_next(ncnf_obj *iter_or_chain);

/* Rewind the iterator or chain position back to the start */
void ncnf_iter_rewind(ncnf_obj *iter_or_chain);


/*************
* Attributes *
*************/

/*
 * Search inside the given object for the specified attribute.
 */

/* Return attribute value or NULL (errno = ESRCH/whatever) if not found. */
char *ncnf_get_attr(ncnf_obj *obj, char *opt_type);

/*
 * Other function return 0 if found and placed in *r,
 * or -1 (errno = ESRCH/whatever) if attribute not found
 * or conversion error (see atoi(3)/atof(3), and inet_aton(3) functions 
 * for specific error codes).
 * On error, return value (*r) is undefined.
 * NOTE: The ncnf_get_attr_int() function is specifically enhanced to process
 * boolean values, like "on", "off", "yes", "no", "true", "false".
 */
int ncnf_get_attr_int(ncnf_obj *obj, char *type, int *r);
long ncnf_get_attr_long(ncnf_obj *obj, char *type, long *r);
int ncnf_get_attr_double(ncnf_obj *obj, char *type, double *r);
struct in_addr;	/* Forward declaration */
int ncnf_get_attr_ip(ncnf_obj *obj, char *type, struct in_addr *r);
int ncnf_get_attr_ipport(ncnf_obj *obj, char *type,
	struct in_addr *rip, unsigned short *rhport /* Host order */);


/****************************************
* Functions related with tree traversal *
****************************************/


/*
 * Walk through the tree and invoke callback for every found object.
 * If callback returns with non-zero value, walking stops and
 * this value becomes the return value of ncnf_walk_tree().
 */
int ncnf_walk_tree(ncnf_obj *tree,
	int (*callback)(ncnf_obj *, void *key),
	void *key);


/*
 * Low-level function to construct a path to the given entity inside
 * configuration file.
 * (*name_func): any function returning the arbitrary string identifier
 * of the object could be used. Default is ncnf_obj_name().
 * The function returns -1 in case of missing arguments (errno = EINVAL),
 * otherwise it return the number of characters written to the buffer
 * (not including the trailing '\0') or would have been written to the
 * buffer if enough space had been available.
 */
int ncnf_construct_path(ncnf_obj *obj,
	char *delim,
	int rev_order,
	char *(*name_func)(ncnf_obj *obj),
	char *buf, int size);


/******************************
* User data and notifications *
******************************/

enum ncnf_notify_event {
  NCNF_UDATA_ATTACH	= 0,	/* Invoked after ncnf_udata_attach() */
  NCNF_UDATA_DETACH	= 1,	/* Invoked in ncnf_udata_detach(NULL) */
  NCNF_NOTIF_ATTACH	= 2,	/* Invoked after ncnf_callback_attach() */
  NCNF_NOTIF_DETACH	= 3,	/* Invoked in ncnf_callback_detach(NULL) */
  NCNF_OBJ_ADD          = 4,	/* Invoked when object is added (lazy mode) */
  NCNF_OBJ_CHANGE	= 5,	/* Invoked when object is changed */
  NCNF_OBJ_DESTROY	= 6,	/* Invoked when object gets destroyed */
};

/*
 * Attach notification callback to the specified object.
 * 
 * If notify is NULL, old function is detached and will be invoked
 * will be invoked with NCNF_NOTIF_DETACH event. Otherwise, old function
 * (if any) will be detached and new function attached, and two callbacks
 * invoked with NCNF_NOTIF_DETACH and NCNF_NOTIF_ATTACH events.
 * 
 * Function will fail with -1/EINVAL when obj is NULL.
 * Function will fail with -1/EPERM if notificator function returns
 * -1 on NCNF_NOTIF_ATTACH or if the old one returns -1 on NCNF_NOTIF_DETACH.
 */
int ncnf_notificator_attach(ncnf_obj *obj,
	int (*notify)(ncnf_obj *obj, enum ncnf_notify_event, void *),
	void *key);

/*
 * Lazy notification is for objects types which are not yet here,
 * but expected to arrive (maybe, at the time of ncnf_diff()).
 * 
 * Function will fail with -1/EINVAL when obj is NULL.
 * Function will fail with -1/EPERM if notificator function returns
 * -1 on NCNF_NOTIF_ATTACH or if the old one returns -1 on NCNF_NOTIF_DETACH.
 */
int ncnf_lazy_notificator(ncnf_obj *parent, char *type_expecting,
	int (*notify)(ncnf_obj *obj, enum ncnf_notify_event, void *),
	void *key);

/*
 * Attach new user data.
 * If user_data is NULL, data is detached and notification function
 * will be invoked with NCNF_UDATA_DETACH event. Otherwise, old data
 * (if any) will be detached and new data attached, and two callbacks
 * invoked with NCNF_UDATA_DETACH and NCNF_UDATA_ATTACH events.
 * 
 * The user data is completely opaque for the library, and only pointer
 * matters. The caller should arrange appropriate notificators to
 * be sure the data is not lost during configuration tree merges
 * (ncnf_diff()) or deletions (ncnf_destroy()).
 * 
 * Function will fail with -1/EINVAL when obj is NULL.
 * Function will fail with -1/EPERM if notificator function returns
 * -1 on NCNF_UDATA_DETACH (for old data) or NCNF_UDATA_ATTACH.
 */
int ncnf_udata_attach(ncnf_obj *obj, void *new_user_data);

/* Get the attached user data */
void *ncnf_udata_get(const ncnf_obj *obj);


/*********
 * Diffs *
 ********/

/*
 * Make the old configuration tree look like the reference tree.
 * 0 if OK, -1 if something wrong.
 */
int ncnf_diff(ncnf_obj *old_root, ncnf_obj *reference_root);

/***********
* Disposal *
***********/

/*
 * Destroy the whole object tree.
 */
void ncnf_destroy(ncnf_obj *obj);

/*********
 * Debug *
 ********/

/*
 * Return the line number at which this object has occured.
 */
int ncnf_obj_line(ncnf_obj *obj);

/*
 * This function invokes to print debug messages.
 * Default is just to print them to stderr.
 */
#include <stdarg.h>
extern void (*ncnf_debug_print_function)(int is_critical,
	const char *fmt, va_list);

/* Recursively dump the tree */
void ncnf_dump(FILE *, ncnf_obj *obj, const char *flatten_type,
	int marked_only, int verbose, int indent);

/*
 * -1/EINVAL
 * 0: Object is not marked
 * 1: Object is marked
 */
int ncnf_obj_marked(ncnf_obj *obj);

#endif	/* NCNF_H */
