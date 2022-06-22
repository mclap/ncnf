find_path(strfunc_INCLUDE_DIR NAMES strfunc.h HINTS ${WITH_LIBSTRFUNC}/include)
find_library(strfunc_LIBRARY NAMES strfunc libstrfunc HINTS ${WITH_LIBSTRFUNC}/lib)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(strfunc DEFAULT_MSG strfunc_INCLUDE_DIR strfunc_LIBRARY)
mark_as_advanced(strfunc_INCLUDE_DIR strfunc_LIBRARY)

if(strfunc_INCLUDE_DIR AND strfunc_LIBRARY)
	set(strfunc_FOUND 1)
	set(strfunc_INCLUDE_DIRS ${strfunc_INCLUDE_DIR})
	set(strfunc_LIBRARIES ${strfunc_LIBRARY})
	set(strfunc_DEFINITIONS -DHAVE_LIBSTRFUNC)

	add_library(strfunc SHARED IMPORTED)
	set_property(TARGET strfunc PROPERTY IMPORTED_LOCATION ${strfunc_LIBRARY})
	target_include_directories(strfunc SYSTEM INTERFACE ${strfunc_INCLUDE_DIRS})
	target_compile_definitions(strfunc INTERFACE ${strfunc_DEFINITIONS})
endif()
