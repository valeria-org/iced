# SPDX-License-Identifier: MIT
# Copyright (C) 2018-present iced project and contributors

# Benchmarks. Each bench_*.cpp file is a separate executable.
file(GLOB ICED_X86_BENCH_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bench_*.cpp")
foreach(src ${ICED_X86_BENCH_SOURCES})
	get_filename_component(name ${src} NAME_WE)
	add_executable(iced_x86_${name} ${src})
	target_link_libraries(iced_x86_${name} PRIVATE iced_x86)
	iced_x86_set_warnings(iced_x86_${name})
endforeach()
