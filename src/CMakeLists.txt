macro(ncnf_test NAME)
	add_executable(${NAME} ${NAME}.c)
	target_link_libraries(${NAME} ncnf)
	add_test(NAME ${NAME} COMMAND ${NAME} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endmacro()

bison_target(ncnf_cr_y ncnf_cr_y.y ${CMAKE_CURRENT_BINARY_DIR}/ncnf_cr_y.c COMPILE_FLAGS "-p ncnf_cr_ -d")
flex_target(ncnf_cr_l ncnf_cr_l.l ${CMAKE_CURRENT_BINARY_DIR}/ncnf_cr_l.c COMPILE_FLAGS "-sp -Cem -Pncnf_cr_")
add_flex_bison_dependency(ncnf_cr_l ncnf_cr_y)

if (strfunc_FOUND)
	set(sf_sources ncnf_ql.c)
endif()

add_library(ncnf SHARED
	headers.h
	ncnf.c ncnf.h ncnf_int.h
	ncnf_coll.c ncnf_coll.h
	ncnf_constr.c ncnf_constr.h
	ncnf_walk.c ncnf_walk.h
	ncnf_diff.c ncnf_diff.h
	ncnf_notif.c ncnf_notif.h
	ncnf_dump.c
	ncnf_cr.c ncnf_cr.h
	ncnf_cr_y.y ncnf_cr_l.l
	${BISON_ncnf_cr_y_OUTPUTS}
	${FLEX_ncnf_cr_l_OUTPUTS}
	ncnf_vr.c ncnf_vr.h
	ncnf_vr_read.c ncnf_vr_constr.c
	ncnf_sf_lite.c ncnf_sf_lite.h
	genhash.c genhash.h bstr.c bstr.h
	asn_SET_OF.c asn_SET_OF.h
	ncnf_app.c ncnf_app_int.c ncnf_app_int.h
	ncnf_find.c ncnf_find.h
	ncnf_policy.c ncnf_policy.h
	ncnf_policy_1.c
	${sf_sources}
	)
target_include_directories(ncnf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(strfunc_FOUND)
	target_link_libraries(ncnf strfunc)
endif()

add_executable(ncnf-validator
	ncnf-validator.c
	)
target_link_libraries(ncnf-validator ncnf)


ncnf_test(check_ncnf)
ncnf_test(ncnf_coll)
target_compile_definitions(ncnf_coll PRIVATE -DMODULE_TEST)
ncnf_test(check_reload)
ncnf_test(check_find)
ncnf_test(check_stress)
ncnf_test(check_constr)
