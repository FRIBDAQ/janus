#	Copyright (C) 2020-2023 CAEN SpA
#
#	This file is part of the CAEN FERS Low Level Library.

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src
include_HEADERS = include/FERSlib.h \
	include/ExportTypes.h \
	include/FERS_Registers_520X.h \
	include/FERS_Registers_5215.h
dist_noinst_HEADERS = \
	include/FERS_MultiPlatform.h \
	include/FERS_LL.h \
	include/FERS_LLusb.h \
	include/FERS_config.h \
	include/FERS_paramparser.h 	
EXTRA_DIST = FERSlibDemo README ReleaseNotes.txt doc/html
#ferslibdocdir = $(docdir)
#dist_ferslibdoc_DATA = \
#	doc/documentation.tar.gz
