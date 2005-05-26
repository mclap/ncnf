#undef	NDEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ncnf_app.h"

int notify(ncnf_obj *obj, enum ncnf_notify_event event, void *key);

int
notify(ncnf_obj *obj, enum ncnf_notify_event event, void *key) {
	char *s;

	switch(event) {
	case NCNF_UDATA_ATTACH:
		s = "Udata ATTACH";
		break;
	case NCNF_UDATA_DETACH:
		s = "Udata DETACH";
		break;
	case NCNF_NOTIF_ATTACH:
		s = "Notificator ATTACH";
		break;
	case NCNF_NOTIF_DETACH:
		s = "Notificator DETACH";
		break;
	case NCNF_OBJ_ADD:
		s = "ADD";
		break;
	case NCNF_OBJ_CHANGE:
		s = "CHANGE";
		break;
	case NCNF_OBJ_DESTROY:
		s = "DESTROY";
		break;
	default:
		s = "UNKNOWN";
	}

	printf("  %s event caught at `%s \"%s\"', key=%p\n",
		s,
		ncnf_obj_type(obj),
		ncnf_obj_name(obj),
		key
	);

	return 0;
}

int
main(int ac, char **av) {
	ncnf_obj *root;
	ncnf_obj *new_root;
	ncnf_obj *process;
	ncnf_obj *iter;
	ncnf_obj *obj;
	char *s;
	int i;

	root = ncnf_read("ncnf_test.conf");
	if(root == NULL) {
		perror("Failed to read first configuration file");
		return 1;
	}

	new_root = ncnf_read("ncnf_test.conf2");
	if(new_root == NULL) {
		perror("Failed to read second configuration file");
		return 1;
	}

	ncnf_dump(stdout, root, 0, 0, 1, 4);

	process = NCNF_APP_resolve_path(root, "localhost/test");
	if(process == NULL || NCNF_APP_initialize_process(process) == -1) {
		perror("Failed to initialize process \"localhost/test\"");
		return 1;
	}

	process = NCNF_APP_resolve_sysid(root, "a-@b1-a-ploc@a-ploc");
	if(process == NULL) {
		perror("Failed to initialize process \"a-@b1-a-ploc@a-ploc\"");
		return 1;
	}


	{
		char buf[128];
		int wrote;

		wrote = ncnf_construct_path(process, "/", 0,
			NULL, buf, sizeof(buf));
		printf("ncnf_construct_path(\"/\", 0, _name) = [%s]:%d\n",
			buf, wrote);
		assert(wrote == 19);
		assert(strcmp(buf, "a-ploc/b1-a-ploc/a-") == 0);

		wrote = ncnf_construct_path(process, "@", 1,
			NULL, buf, sizeof(buf));
		printf("ncnf_construct_path(\"@\", 1, _name) = [%s]:%d\n",
			buf, wrote);
		assert(wrote == 19);
		assert(strcmp(buf, "a-@b1-a-ploc@a-ploc") == 0);

		wrote = ncnf_construct_path(process, "/", 0,
			ncnf_obj_type, buf, sizeof(buf));
		printf("ncnf_construct_path(\"/\", 0, _type) = [%s]:%d\n",
			buf, wrote);
		assert(wrote == 16);
		assert(strcmp(buf, "ploc/box/process") == 0);

		wrote = ncnf_construct_path(process, "@", 1,
			ncnf_obj_type, buf, sizeof(buf));
		printf("ncnf_construct_path(\"@\", 1, _type) = [%s]:%d\n",
			buf, wrote);
		assert(wrote == 16);
		assert(strcmp(buf, "process@box@ploc") == 0);

	}

	assert(strcmp(ncnf_get_attr(root, "spaceless"), "true") == 0);
	assert(ncnf_get_attr_int(root, "spaceless", &i) == 0 && i == 1);

	assert(strcmp(ncnf_get_attr(root, "multiline-attribute1"),
		"one two threefour\nfive") == 0);
	assert(strcmp(ncnf_get_attr(root, "multiline-attribute2"),
		"one two threefour\nfive") == 0);

	obj = ncnf_get_obj(root, "service", NULL, NCNF_FIRST_OBJECT);
	if(obj == NULL) {
		perror("ncnf_get_obj(\"service\") failed");
		return 1;
	}

	printf("Adding plain service notificator\n");
	if(ncnf_notificator_attach(obj, notify, NULL)) {
		perror("Can't attach notificator to service\n");
		return 1;
	}

	printf("Adding lazy service notificator for properties\n");
	if(ncnf_lazy_notificator(obj, "properties", notify, NULL)) {
		perror("Can't attach notificator to service\n");
		return 1;
	}

	iter = ncnf_get_obj(obj, "properties", NULL, NCNF_FIRST_OBJECT);
	if(iter == NULL) {
		perror("ncnf_get_obj(\"properties\") failed");
		return 1;
	}

	if(ncnf_notificator_attach(iter, notify, NULL)) {
		perror("Can't attach notificator to properties\n");
		return 1;
	}

	iter = ncnf_get_obj(obj, "properties", NULL, NCNF_ITER_OBJECTS);
	if(iter == NULL) {
		perror("ncnf_get_obj(\"properties\", ITER) failed");
		return 1;
	}

	while((obj = ncnf_iter_next(iter))) {
		printf("Inside %s:\n", ncnf_get_attr(obj, NULL));
		s = ncnf_get_attr(obj, "port");
		if(s == NULL) {
			perror("ncnf_get_attr(\"port\") failed");
			return 1;
		}
		printf("\tport = %s\n", s);
	}
	ncnf_iter_rewind(iter);

	printf("===\n");

	while((obj = ncnf_iter_next(iter))) {
		ncnf_obj *iter_attr;
		ncnf_obj *port_obj;

		printf("Inside %s:\n", ncnf_get_attr(obj, NULL));

		iter_attr = ncnf_get_obj(obj, "port", NULL,
			NCNF_ITER_ATTRIBUTES);
		if(iter_attr == NULL) {
			perror("Can't get iterator for ports");
			return 1;
		}

		while((port_obj = ncnf_iter_next(iter_attr))) {
			s = ncnf_get_attr(port_obj, "port");
			if(s == NULL) {
				perror("ncnf_get_attr(\"port\") failed");
				return 1;
			}
			printf("\tport = %s\n", s);

			if(ncnf_notificator_attach(port_obj, notify, NULL)) {
				perror("Can't attach notificator to port\n");
				return 1;
			}
		}

		ncnf_destroy(iter_attr);

		printf("    Alternate way of getting iterator:\n");

		iter_attr = ncnf_get_obj(obj, "port", NULL,
			NCNF_CHAIN_ATTRIBUTES);
		if(iter_attr == NULL) {
			perror("Can't get chain for ports");
			return 1;
		}

		while((port_obj = ncnf_iter_next(iter_attr))) {
			s = ncnf_get_attr(port_obj, "port");
			if(s == NULL) {
				perror("ncnf_get_attr(\"port\") failed");
				return 1;
			}
			printf("\tport = %s\n", s);
		}

		ncnf_iter_rewind(iter_attr);

		printf("    Alternate way of getting iterator after rewind:\n");

		while((port_obj = ncnf_iter_next(iter_attr))) {
			s = ncnf_get_attr(port_obj, "port");
			if(s == NULL) {
				perror("ncnf_get_attr(\"port\") failed");
				return 1;
			}
			printf("\tport = %s\n", s);
		}

	}

	ncnf_destroy(iter);

	obj = ncnf_get_obj(root, "upper-level", NULL, NCNF_FIRST_OBJECT);
	assert(obj);
	obj = ncnf_get_obj(obj, "lower-level", NULL, NCNF_FIRST_OBJECT);
	assert(obj);

	if(ncnf_notificator_attach(obj, notify, NULL)) {
		perror("Failure to attach notificator");
		return 1;
	}

	obj = ncnf_get_obj(root, "reference-holder", NULL, NCNF_FIRST_OBJECT);
	assert(obj);
	obj = ncnf_get_obj(obj, "abstract", "entity", NCNF_FIRST_OBJECT);
	assert(obj);
	obj = ncnf_get_obj(obj, "port", NULL, NCNF_FIRST_ATTRIBUTE);
	assert(obj);
	assert(strcmp(ncnf_obj_name(obj), "80") == 0);

	printf("\nDiffing two configuration trees\n");

	if(ncnf_diff(root, new_root)) {
		perror("Failure to diff two configuration trees");
		return 1;
	}

	printf("Destroying new root\n");
	ncnf_destroy(new_root);

	obj = ncnf_get_obj(root, "reference-holder", NULL, NCNF_FIRST_OBJECT);
	assert(obj);
	obj = ncnf_get_obj(obj, "abstract", "entity", NCNF_FIRST_OBJECT);
	assert(obj);
	obj = ncnf_get_obj(obj, "port", NULL, NCNF_FIRST_ATTRIBUTE);
	assert(obj);
	assert(strcmp(ncnf_obj_name(obj), "81") == 0);

	printf("\nSecond diffing.\n");

	new_root = ncnf_read("ncnf_test.conf2");
	if(new_root == NULL) {
		perror("Failed to read second configuration file");
		return 1;
	}

	if(ncnf_diff(root, new_root)) {
		perror("Failure to diff two configuration trees");
		return 1;
	}
	printf("Second diffing done.\n\n");

	printf("Third diffing.\n");

	new_root = ncnf_read("ncnf_test.conf2");
	if(new_root == NULL) {
		perror("Failed to read second configuration file");
		return 1;
	}


	if(ncnf_diff(root, new_root)) {
		perror("Failure to diff two configuration trees");
		return 1;
	}

	printf("Third diffing done.\n");

	printf("\nDestroying old root\n");
	ncnf_destroy(root);
	ncnf_destroy(new_root);

	return 0;
}

