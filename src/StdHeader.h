////////////////////////////////////////////////////////////
// standard header for pre-compile
//
// Written by Manuke

////////////////////////////////////////////////////////////
// platform & compiler

#ifdef _MSC_VER // Windows platform & Visual C++

// conditional expression is constant
#pragma warning(disable : 4127)
// nonstandard extension used(in windows.h)
#pragma warning(disable : 4201)
// private operator= are good to have
#pragma warning(disable : 4512)
// cannot create copy constructor
#pragma warning(disable : 4511)
// delete unused inline function
#pragma warning(disable : 4514)
// unreachable code?(Unknown)
#pragma warning(disable : 4702)
// unreferenced inline/local function has been removed
#pragma warning(disable : 4710)
// identifier was truncated in the debug information
#pragma warning(disable : 4786)
// old-type function
#pragma warning(disable : 4996)

#define X88_COMPILER_TEMPLATE_EXPLICIT_SUPPORT
#define X88_COMPILER_TEMPLATE_STATICVAL_NORMAL

#elif defined(__BORLANDC__) // Windows platform & Borland C++

// unreferenced inline/local function has been removed
#pragma warning(disable : 4710)

#define X88_COMPILER_TEMPLATE_EXPLICIT_SUPPORT
#define X88_COMPILER_TEMPLATE_STATICVAL_NORMAL

#else // g++

#define X88_COMPILER_TEMPLATE_EXPLICIT_SUPPORT
#define X88_COMPILER_TEMPLATE_STATICVAL_SPECIAL

#endif // compiler check

#ifdef _WINDOWS // Windows Platform

#define X88_PLATFORM_WINDOWS
#define X88_GUI_WINDOWS
#define X88_ENCODE_WINDOWS
#define X88_ENCODING_SOURCE_SJIS
#define X88_ENCODING_GUI_SJIS

#else // UNIX Platform

#define X88_PLATFORM_UNIX

#ifndef X88_GUI_SDL3
#define X88_GUI_SDL3
#endif // X88_GUI_SDL3

#define X88_ENCODE_ICONV
#define X88_ENCODING_SOURCE_UTF8
#define X88_ENCODING_GUI_UTF8

#endif // Platform

////////////////////////////////////////////////////////////
// include

// Standard Library

#ifdef X88_PLATFORM_WINDOWS

#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include <direct.h>
#include <io.h>
#include <math.h>

#define strcasecmp	stricmp
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define unlink		_unlink

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;

#elif defined(X88_PLATFORM_UNIX)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/mman.h>

#define _MAX_PATH	(_POSIX_PATH_MAX+1)

#endif // X88_PLATFORM

// GUI

#ifdef X88_GUI_WINDOWS

#define X88_BYTEORDER_LITTLE_ENDIAN

#define _WINVER			0x0400
#define _WIN32_WINNT	0x0400
#define _WIN32_IE		0x0300

#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
#include <imm.h>

// DirectX

#define DIRECTDRAW_VERSION	0x0300
#define DIRECTSOUND_VERSION	0x0300
#define DIRECTINPUT_VERSION	0x0300

#include <ddraw.h>
#include <dsound.h>
#include <dinput.h>

typedef HWND CX88WndHandle;

#elif defined(X88_GUI_SDL3)

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define X88_BYTEORDER_LITTLE_ENDIAN
#else
#define X88_BYTEORDER_BIG_ENDIAN
#endif

#endif // X88_GUI

// STL

#include <vector>
#include <list>
#include <deque>
#include <set>
#include <map>
#include <queue>
#include <iterator>
#include <functional>
#include <algorithm>
#include <string>

////////////////////////////////////////////////////////////
// macro

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif // M_PI

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#define min_value(a, b)	(((a) <= (b))? (a): (b))
#define max_value(a, b)	(((a) >= (b))? (a): (b))
