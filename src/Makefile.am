
AM_CPPFLAGS =

AM_YFLAGS = -p ncnf_cr_ -d 
AM_LFLAGS = -sp -Cem -Pncnf_cr_ -olex.yy.c

if LIBSTRFUNC
sf_tests = check_nql
sf_includes = ncnf_ql.h
sf_sources = ncnf_ql.c ncnf_ql.h
endif

TESTS = check_ncnf check_coll check_reload check_find \
	check_stress check_constr
	check_bstr check_lazy_interactive $(sf_tests)

check_PROGRAMS = $(TESTS)

bin_PROGRAMS = ncnf-validator

LDADD = libncnf.la

check_coll_SOURCES = ncnf_coll.c
check_coll_CFLAGS = -DMODULE_TEST

include_HEADERS = ncnf.h ncnf_app.h bstr.h genhash.h
nodist_include_HEADERS = ncnf_coll.h \
	ncnf_int.h ncnf_walk.h ncnf_diff.h \
	ncnf_notif.h ncnf_constr.h $(sf_includes)

lib_LTLIBRARIES = libncnf.la
libncnf_la_LDFLAGS = -version-info 3:0:1
libncnf_la_SOURCES =	\
	headers.h				\
	ncnf.c ncnf.h ncnf_int.h		\
	ncnf_coll.c ncnf_coll.h			\
	ncnf_constr.c ncnf_constr.h		\
	ncnf_walk.c ncnf_walk.h			\
	ncnf_diff.c ncnf_diff.h			\
	ncnf_notif.c ncnf_notif.h		\
	ncnf_dump.c				\
	ncnf_cr.c ncnf_cr.h			\
	ncnf_cr_y.y ncnf_cr_l.l			\
	ncnf_vr.c ncnf_vr.h			\
	ncnf_vr_read.c ncnf_vr_constr.c		\
	ncnf_sf_lite.c ncnf_sf_lite.h		\
	genhash.c genhash.h bstr.c bstr.h	\
	asn_SET_OF.c asn_SET_OF.h		\
	ncnf_app.c ncnf_app_int.c ncnf_app_int.h\
	ncnf_find.c ncnf_find.h			\
	$(sf_sources)				\
	ncnf_policy.c ncnf_policy.h		\
	ncnf_policy_1.c
#
# Read ncnf_policy.h to get help about how to add a new policy.
#

EXTRA_DIST = ncnf_test.conf ncnf_test.conf2 ncnf_test.vr
