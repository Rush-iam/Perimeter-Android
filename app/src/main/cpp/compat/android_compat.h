#ifndef ANDROID_COMPAT_H
#define ANDROID_COMPAT_H

/**
 * Compatibility header for Android NDK build of Perimeter Engine.
 * This header is force-included via CMake to provide missing definitions
 * and fix standard header issues.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cerrno>
#include <ctime>
#endif

// Map some common Win32-like things that might be missing
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#define _open open
#define _close close
#define _read read
#define _write write
#define _lseek lseek
#endif

#endif // ANDROID_COMPAT_H
