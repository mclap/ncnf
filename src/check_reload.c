#undef	NDEBUG
#include <stdio.h>
#include <assert.h>

#include "ncnf.h"
#include "ncnf_app.h"

int
main(int ac, char **av) {
	ncnf_obj *root;
	ncnf_obj *new_root;
	char *configs[] = { "ncnf_test.conf", "ncnf_test.conf2" };
	int i;

	if(ac > 1) configs[0] = av[1];
	if(ac > 2) configs[1] = av[2];

	root = ncnf_read(configs[0]);
	if(root == NULL) {
		perror("Failed to read first configuration file");
		return 1;
	}

	for(i = 0; i < 10; i++) {

		new_root = ncnf_read(configs[(i + 1) & 1]);
		if(new_root == NULL) {
			perror("Failed to read subsequent configuration file");
			return 1;
		}
	
		if(ncnf_diff(root, new_root)) {
			perror("Failure to diff two configuration trees");
			return 1;
		}

		ncnf_destroy(new_root);
	}

	ncnf_destroy(root);

	return 0;
}
