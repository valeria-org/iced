# SPDX-License-Identifier: MIT
# Copyright (C) 2018-present iced project and contributors

cmake_minimum_required(VERSION 3.16)
project(iced_x86 VERSION 1.21.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(ICED_X86_IS_TOP_LEVEL ON)
else()
	set(ICED_X86_IS_TOP_LEVEL OFF)
endif()

option(ICED_X86_BUILD_TESTS "Build the iced_x86 unit tests" ${ICED_X86_IS_TOP_LEVEL})
option(ICED_X86_BUILD_BENCH "Build the iced_x86 benchmarks" ${ICED_X86_IS_TOP_LEVEL})
option(ICED_X86_NO_EXCEPTIONS_RTTI "Build the library with -fno-exceptions -fno-rtti" ON)
option(ICED_X86_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
# Max stack frame size (bytes) before the compiler warns (GCC/Clang). 0 = disabled
set(ICED_X86_MAX_FRAME_SIZE "1024" CACHE STRING "Warn if a function's stack frame is larger than this")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# All library sources. Each component lives in its own directory (src/decoder, src/encoder, src/formatter/gas, ...).
# Since it's a static library, only the object files that are used get linked into the final binary.
file(GLOB_RECURSE ICED_X86_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ICED_X86_PUBLIC_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

add_library(iced_x86 STATIC ${ICED_X86_SOURCES} ${ICED_X86_PUBLIC_HEADERS})
add_library(iced_x86::iced_x86 ALIAS iced_x86)
target_include_directories(iced_x86
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(iced_x86 PUBLIC cxx_std_17)
set_target_properties(iced_x86 PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)

function(iced_x86_set_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
		if(ICED_X86_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE /WX)
		endif()
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(${target} PRIVATE -Wshadow=local)
		else()
			# Clang doesn't support -Wshadow=local
			target_compile_options(${target} PRIVATE -Wshadow-uncaptured-local)
		endif()
		if(ICED_X86_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE -Werror)
		endif()
	endif()
endfunction()

iced_x86_set_warnings(iced_x86)
if(NOT MSVC)
	# Separate sections so unused code/data can be removed by the linker (--gc-sections)
	target_compile_options(iced_x86 PRIVATE -ffunction-sections -fdata-sections)
	if(ICED_X86_NO_EXCEPTIONS_RTTI)
		target_compile_options(iced_x86 PRIVATE -fno-exceptions -fno-rtti)
	endif()
	if(NOT ICED_X86_MAX_FRAME_SIZE STREQUAL "0")
		target_compile_options(iced_x86 PRIVATE -Wframe-larger-than=${ICED_X86_MAX_FRAME_SIZE})
	endif()
endif()

if(ICED_X86_BUILD_TESTS)
	enable_testing()
	file(GLOB_RECURSE ICED_X86_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
	add_executable(iced_x86_tests ${ICED_X86_TEST_SOURCES})
	target_link_libraries(iced_x86_tests PRIVATE iced_x86)
	# Tests can use library internals
	target_include_directories(iced_x86_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
	get_filename_component(ICED_X86_UNIT_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../UnitTests/Intel" ABSOLUTE)
	target_compile_definitions(iced_x86_tests PRIVATE ICED_X86_UNIT_TESTS_DIR="${ICED_X86_UNIT_TESTS_DIR}")
	iced_x86_set_warnings(iced_x86_tests)
	if(NOT MSVC)
		# Some generated test files are huge
		target_compile_options(iced_x86_tests PRIVATE -fexceptions -Wno-unused-function)
	endif()
	add_test(NAME iced_x86_tests COMMAND iced_x86_tests)
endif()

if(ICED_X86_BUILD_BENCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bench/CMakeLists.txt")
	add_subdirectory(bench)
endif()

include(GNUInstallDirs)
install(TARGETS iced_x86 EXPORT iced_x86Targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/iced_x86 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT iced_x86Targets NAMESPACE iced_x86:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iced_x86)
