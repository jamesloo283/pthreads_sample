#pragma once
#include <unistd.h>
#include <sys/syscall.h>

/*
 * gettid(...) isn't portable and isn't implemented on every platform
 * so unfortunately, this has to be done via syscall
 */
#define GETTID() (pid_t)syscall(SYS_gettid)
