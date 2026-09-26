# SPDX-License-Identifier: MIT
# Copyright (C) 2018-present iced project and contributors

# Runs an example and compares its stdout with the expected output.
# Usage: cmake -DEXAMPLE=<exe> -DEXPECTED=<expected.txt> [-DACTUAL=<file>] -P run_example.cmake
# If ACTUAL is set, the actual output is written to that file (useful when the test fails).

if(NOT DEFINED EXAMPLE OR NOT DEFINED EXPECTED)
	message(FATAL_ERROR "Usage: cmake -DEXAMPLE=<exe> -DEXPECTED=<expected.txt> [-DACTUAL=<file>] -P run_example.cmake")
endif()

execute_process(
	COMMAND "${EXAMPLE}"
	OUTPUT_VARIABLE actual
	ERROR_VARIABLE stderr
	RESULT_VARIABLE exit_code
)
# Windows: stdout is in text mode (\n -> \r\n)
string(REPLACE "\r\n" "\n" actual "${actual}")
if(DEFINED ACTUAL)
	file(WRITE "${ACTUAL}" "${actual}")
endif()
if(NOT exit_code STREQUAL "0")
	message(FATAL_ERROR "${EXAMPLE} failed (exit code: ${exit_code})\nstderr:\n${stderr}\nstdout:\n${actual}")
endif()

file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT actual STREQUAL expected)
	message(FATAL_ERROR "Output of ${EXAMPLE} doesn't match ${EXPECTED}\n"
		"---- expected ----\n${expected}\n---- actual ----\n${actual}\n----\n"
		"Actual output: ${ACTUAL}")
endif()
