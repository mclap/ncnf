#undef	NDEBUG
#include <stdio.h>
#include <assert.h>

#include "ncnf.h"
#include "ncnf_app.h"
#include "ncnf_int.h"

ncnf_obj *root;
int iters = 0;

static int
cb(ncnf_obj *obj, void *key) {
	char buf[128];
	ncnf_obj *obj2;
	int write;

	if(obj->obj_class != NOBJ_COMPLEX)
		return 0;

	write = ncnf_construct_path(obj, "@", 1, ncnf_obj_name,
		buf, sizeof(buf));
	assert(write > 0);
	assert(write < sizeof(buf));

	printf("[%s]", buf); fflush(stdout);
	if(write < 32) {
		obj2 = NCNF_APP_resolve_sysid(root, buf);
		assert(obj2);
		assert(obj2 == obj);
		printf(" <OK>\n");
	} else {
		printf(" <unchecked:lim(32)>\n");
	}

	write = ncnf_construct_path(obj, "/", 0, ncnf_obj_name,
		buf, sizeof(buf));
	assert(write > 0);
	assert(write < sizeof(buf));

	printf("[%s]", buf); fflush(stdout);
	obj2 = NCNF_APP_resolve_path(root, buf);
	assert(obj2);
	assert(obj2 == obj);
	printf(" <OK>\n");

	iters++;

	return 0;
}

int
main(int ac, char **av) {
	int ret;

	if(ac == 2)
		root = ncnf_read(av[1]);
	else
		root = ncnf_read("ncnf_test.conf");
	assert(root);

	ret = ncnf_walk_tree(root, cb, NULL);
	assert(ret == 0);

	assert(iters > 0);

	return 0;
}
