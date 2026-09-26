# SPDX-License-Identifier: MIT
# Copyright (C) 2018-present iced project and contributors

# Examples (the same examples as the Rust crate's README.md). Each *.cpp file is a separate executable.
# Each example also gets a test that runs it and compares its stdout with expected/<name>.txt
# (run_example.cmake). The example code is copied to ../README.md by update_readme.py.
file(GLOB ICED_X86_EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(src ${ICED_X86_EXAMPLE_SOURCES})
	get_filename_component(name ${src} NAME_WE)
	set(target iced_x86_example_${name})
	add_executable(${target} ${src})
	target_link_libraries(${target} PRIVATE iced_x86::iced_x86)
	iced_x86_set_warnings(${target})
	add_test(
		NAME example_${name}
		COMMAND ${CMAKE_COMMAND}
			-DEXAMPLE=$<TARGET_FILE:${target}>
			-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${name}.txt
			-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/${name}.actual.txt
			-P ${CMAKE_CURRENT_SOURCE_DIR}/run_example.cmake
	)
endforeach()

# Verify that the example code in README.md is identical to the example files
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_test(NAME examples_readme COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/update_readme.py --check)
endif()
