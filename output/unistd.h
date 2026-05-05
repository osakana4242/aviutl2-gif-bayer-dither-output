#pragma once

#ifdef _WIN32

#define _CRT_SECURE_NO_WARNINGS

#include <io.h>
#include <fcntl.h>
#include <stdio.h>

#define open    _open
#define close   _close
#define write   _write
#define read    _read
#define fdopen  _fdopen

#endif
