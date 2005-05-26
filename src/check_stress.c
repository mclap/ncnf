#undef	NDEBUG
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "ncnf.h"
#include "ncnf_app.h"

const int cycles = 1000;

int
main(int ac, char **av) {
	ncnf_obj *root;
	ncnf_obj *new_root;
	char *configs[] = { "ncnf_test.conf", "ncnf_test.conf2" };
	int i;

	if(ac > 1) configs[0] = av[1];
	if(ac > 2) configs[1] = av[2];

	/*

		printf("Phase 0 started\n");

		for(i = 0; i < cycles * 10; i++) {
			printf("=========================\n");
			root = ncnf_read(configs[0]);
			if(root) {
				perror("Succeeded to read config file");
				return 1;
			}
		}
	*/

	printf("Phase I started\n");

	root = ncnf_read(configs[0]);
	ncnf_destroy(root);

	for(i = 0; i < cycles; i++) {
		root = ncnf_read(configs[0]);
		if(root == NULL) {
			perror("Failed to read configuration file");
			return 1;
		}
		ncnf_destroy(root);
	}

	printf("Phase I ended\n");
	printf("Phase II started\n");

	root = ncnf_read(configs[0]);
	if(root == NULL) {
		perror("Failed to read configuration file");
		return 1;
	}

	for(i = 0; i < cycles; i++) {

		new_root = ncnf_read(configs[(i + 1) & 1]);
		if(new_root == NULL) {
			perror("Failed to read second configuration file");
			return 1;
		}
	
		if(ncnf_diff(root, new_root)) {
			perror("Failure to diff two configuration trees");
			return 1;
		}
	
		ncnf_destroy(new_root);
	}

	ncnf_destroy(root);

	printf("Phase II ended\n");
	printf("Phase III started\n");

	root = ncnf_read(configs[0]);
	if(root == NULL) {
		perror("Failed to read configuration file");
		return 1;
	}

	ncnf_destroy(root);

	printf("Phase III ended\n");

	printf("Done\n");

	return 0;
}

