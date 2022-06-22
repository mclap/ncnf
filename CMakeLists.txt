cmake_minimum_required(VERSION 3.23)
project(ncnf
	VERSION 3.0.1
	DESCRIPTION "Netli Configuration library"
	LANGUAGES C
	)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

enable_testing()
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(strfunc)

add_subdirectory(src)
