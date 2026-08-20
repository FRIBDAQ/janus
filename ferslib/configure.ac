#	Copyright (C) 2020-2023 CAEN SpA
#
#	This file is part of the CAEN Dig2 Low Level Library.

# Tested with autoconf 2.69, released in 2012
# [m4_esyscmd_s([echo $VERSION])]
VERSION=1.2.0
m4_define([FL_VERSION], [2.2.1])
AC_PREREQ([2.69])
AC_INIT(caenferslib, [FL_VERSION], support.computing@caen.it)

# Search in m4 folder for custom macros. It requires also ACLOCAL_AMFLAGS on
# Makefile.am, unless we use AC_CONFIG_MACRO_DIRS introduced on autoconf 2.70.
AC_CONFIG_MACRO_DIR([m4])

# Define _GNU_SOURCE
AC_USE_SYSTEM_EXTENSIONS

# tar-ustar required to remove the filename length limit on default tar-v7 format
# is old enough to be supported everywhere
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects tar-ustar])
AM_PROG_AR

# Requires constructor to be invoked at startup, cannot be used as static
LT_INIT([disable-static])

# Set langage to C
AC_PROG_CC
AC_PROG_CXX

AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory], [AC_MSG_ERROR([C++17 support is required but not available.])])

CXXFLAGS="$CXXFLAGS -Dlinux"
CFLAGS="$CFLAGS -Dlinux"
#AX_CHECK_COMPILE_FLAG([-std=c++11], [AX_APPEND_FLAG([-std=c++11])], [AC_MSG_ERROR([C++11 support required])])
#AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])

# Checks for other programs
AC_PROG_INSTALL

# Check fo libusb-1.0
#AC_SEARCH_LIBS([libusb_init], [usb-1.0], [], [AC_MSG_ERROR([LIBUSB MISSING])])
PKG_CHECK_MODULES([LIBUSB], [libusb-1.0], [], [AC_MSG_ERROR([libusb-1.0 is required but it was not found.])])

#AC_SUBST([LIBUSB_CFLAGS])
#AC_SUBST([LIBUSB_LIBS])

# If available, use -fvisibility=hidden to hide internal names
#AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden])

AX_PTHREAD([], [AC_MSG_ERROR([pthread required.])])
#AC_CHECK_LIB([pthread], [pthread_create], [LIBPTHREAD="-lphtread"], [LIBPTHREAD=""])
#AC_SUBST([LIBPTHREAD])
#AC_CHECK_HEADERS([pthread.h])

# If available, use -zdefs to improve linker consistency
AX_APPEND_LINK_FLAGS([-Wl,-z,defs])

# Add support for --disable-assert
AC_HEADER_ASSERT

# Checks for Boost 1.67.0 or greater
#AX_BOOST_BASE([1.67.0], [], [AC_MSG_ERROR(Boost library >= 1.67.0 (headers only) required.)])

# Check for unsafe character(s) in path
AS_CASE(["$PWD"], [*[!A-Za-z0-9./-]*], [AC_MSG_ERROR([Unsafe build path detected])])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
