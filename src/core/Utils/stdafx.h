#pragma once

// Ensure Windows target/version and lean/minimal API macros are set before including system headers.

#ifndef PROJECT_STDAFX_H
#define PROJECT_STDAFX_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Target Windows 10. Change if you need a different target.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Pull in SDK versioning header and the main Windows header.
#include <SDKDDKVer.h>

// Common C++ Standard Library includes
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Windows specific includes
#include <windows.h>
#include <io.h>
#include <fcntl.h>

// C++20 specific headers if needed
#ifdef __cplusplus
#if __cplusplus >= 202002L
#include <concepts>
#include <span>
#endif
#endif
#endif