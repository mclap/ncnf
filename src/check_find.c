#undef	NDEBUG
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "ncnf.h"
#include "ncnf_app.h"

int filter(ncnf_obj *, void *);

int
main(int ac, char **av) {
	ncnf_obj *root;
	ncnf_obj *iter;
	ncnf_obj *obj;
	int count;

	printf("%s\n", av[0]);
	ncnf_destroy(ncnf_read("ncnf_test.conf"));

	root = ncnf_read("ncnf_test.conf");
	if(root == NULL) {
		perror("Failed to read first configuration file");
		return 1;
	}

	printf("nothing:\n");

	iter = NCNF_APP_find_objects(root, "", NULL, NULL);
	assert(iter == NULL && errno == EINVAL);

	iter = NCNF_APP_find_objects(root, "nothing", NULL, NULL);
	assert(iter == NULL && errno == ESRCH);

	printf("\nservice/properties:\n");

	iter = NCNF_APP_find_objects(root, "service/properties", NULL, NULL);
	assert(iter);
	for(count = 0; (obj = ncnf_iter_next(iter)); count++) {
		printf("%s \"%s\"\n",
			ncnf_obj_type(obj),
			ncnf_obj_name(obj)
		);
	}
	assert(count == 2);

	ncnf_destroy(iter);

	printf("\nnloc/ploc/box/process/si:\n");

	iter = NCNF_APP_find_objects(root, "nloc/ploc/box/process/si",
		filter, NULL);
	assert(iter);
	for(count = 0; (obj = ncnf_iter_next(iter)); count++) {
		printf("%s \"%s\"\n",
			ncnf_obj_type(obj),
			ncnf_obj_name(obj)
		);
	}
	assert(count == 19);

	ncnf_destroy(iter);

	ncnf_destroy(root);

	return 0;
}

int
filter(ncnf_obj *obj, void *key) {
	(void)key;

	printf("\tfilter(%s \"%s\")\n",
		ncnf_obj_type(obj),
		ncnf_obj_name(obj)
	);

	return 0;
}
