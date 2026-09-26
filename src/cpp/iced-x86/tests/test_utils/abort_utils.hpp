// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Used to port Rust's `#[should_panic]` tests: the library aborts where Rust panics.

#pragma once

#if defined(__unix__) || defined(__APPLE__)
#define ICED_X86_TESTS_CAN_CHECK_ABORT 1
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#define ICED_X86_TESTS_CAN_CHECK_ABORT 0
#endif

namespace iced_x86::tests {

/// Returns `true` if `fn()` aborts the process (it's called in a child process).
/// Always returns `true` if it's not supported by the OS (the function isn't called).
template <typename F>
bool aborts(F&& fn) {
#if ICED_X86_TESTS_CAN_CHECK_ABORT
	pid_t pid = fork();
	if (pid < 0)
		return false;
	if (pid == 0) {
		fn();
		_exit(0);
	}
	int status = 0;
	if (waitpid(pid, &status, 0) != pid)
		return false;
	return WIFSIGNALED(status);
#else
	(void)fn;
	return true;
#endif
}

} // namespace iced_x86::tests
