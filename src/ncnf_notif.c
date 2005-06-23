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


static int
_ncnf_notify_callback(struct ncnf_obj_s *obj, void *eventp) {
	enum ncnf_notify_event event = *(enum ncnf_notify_event *)eventp;

	if(obj->notify)
		obj->notify((ncnf_obj *)obj, event, obj->notify_key);

	return 0;
}


void
_ncnf_notify_everyone(struct ncnf_obj_s *obj, enum ncnf_notify_event event) {

	_ncnf_walk_tree(obj, _ncnf_notify_callback, &event);

}

/*
 * This mask is being used instead of NULL value, which is
 * inappropriate in certain cases.
 */
#define	MASK_ALL_OBJECTS	"#AlLObJeCtS#"


int
_ncnf_lazy_notificator(struct ncnf_obj_s *obj, const char *watchfor,
        int (*notify)(ncnf_obj *obj, enum ncnf_notify_event, void *key),
		void *key) {
	collection_t *coll;
	struct ncnf_obj_s *ln;
	int (*old_notify)(ncnf_obj *, enum ncnf_notify_event, void *);
	void *old_notify_key;
	int adding = 0;
	bstr_t wf;

	if(_NOBJ_CONTAINER(obj) == 0) {
		/* Inappropriate object */
		errno = EINVAL;
		return -1;
	}

	if(watchfor == NULL)
		/* Mask NULL */
		watchfor = MASK_ALL_OBJECTS;

	coll = &obj->m_collection[COLLECTION_LAZY_NOTIF];

	wf = str2bstr(watchfor, -1);
	if(!wf) return -1;
	ln = _ncnf_coll_get(coll, 0, wf, NULL, NULL);
	if(ln == NULL) {
		ln = _ncnf_obj_new(0, NOBJ_LAZY_NOTIF, wf, NULL, 0);
		bstr_free(wf);
		if(ln == NULL)
			return -1;
		adding = 1;
	} else {
		bstr_free(wf);
	}

	old_notify = ln->notify;
	old_notify_key = ln->notify_key;
	ln->notify = NULL;

	/* Report detach to the old function */
	if(old_notify) {
		if(old_notify((ncnf_obj *)obj,
			NCNF_NOTIF_DETACH, old_notify_key) == -1) {
			ln->notify = old_notify;
			ln->notify_key = old_notify_key;
			if(adding)
				_ncnf_obj_destroy(ln);
			errno = EPERM;
			return -1;
		}
	}

	ln->notify = notify;
	ln->notify_key = key;

	/* Report attach to the new one */
	if(ln->notify) {
		if(notify((ncnf_obj *)obj,
			NCNF_NOTIF_ATTACH, key) == -1) {
			obj->notify = NULL;
			obj->notify_key = NULL;
			if(adding)
				_ncnf_obj_destroy(ln);
			errno = EPERM;
			return -1;
		}
	}


	if(adding) {
		if(_ncnf_attach_obj(obj, ln, 0)) {
			/* ncnf_destroy() will notify callbacks */
			ncnf_destroy((ncnf_obj *)ln);
			return -1;
		}
	}

	_ncnf_check_lazy_filters(obj, -1);

	return 0;
}


void
_ncnf_check_lazy_filters(struct ncnf_obj_s *obj, int only_mark_value) {
	collection_t *coll;
	collection_t *ncoll;
	int i, j;

	if(_NOBJ_CONTAINER(obj) == 0) {
		assert(0);
		return;
	}

	coll = &obj->m_collection[COLLECTION_LAZY_NOTIF];
	for(i = 0; i < coll->entries; i++) {
		char *watchfor = NULL;
		struct ncnf_obj_s *o = coll->entry[i].object;

		/* Lazy notificator has no notificator function */
		if(o->notify == NULL)
			continue;

		if(strcmp(o->type, MASK_ALL_OBJECTS))
			watchfor = o->type;

		/* Check objects */

		ncoll = &obj->m_collection[COLLECTION_OBJECTS];
		for(j = 0; j < ncoll->entries; j++) {
			struct ncnf_obj_s *child
				= ncoll->entry[j].object;

			if(child->mark != only_mark_value
				&& only_mark_value != -1)
				continue;

			if(watchfor && strcmp(child->type, watchfor))
				continue;

			if(_ncnf_real_object(child)->notify == NULL)
				o->notify((ncnf_obj *)child, NCNF_OBJ_ADD,
					o->notify_key);
		}

		/* Check attributes */

		ncoll = &obj->m_collection[COLLECTION_ATTRIBUTES];
		for(j = 0; j < ncoll->entries; j++) {
			struct ncnf_obj_s *child
				= ncoll->entry[j].object;

			if(child->mark != only_mark_value
				&& only_mark_value != -1)
				continue;

			if(watchfor && strcmp(child->type, watchfor))
				continue;

			if(_ncnf_real_object(child)->notify == NULL)
				o->notify((ncnf_obj *)child, NCNF_OBJ_ADD,
					o->notify_key);
		}

	}

}


