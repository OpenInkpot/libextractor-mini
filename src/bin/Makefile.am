INCLUDES=-I${top_srcdir}/src/lib
AM_CFLAGS=-Wall -Werror --std=gnu99

bin_PROGRAMS = extractor

extractor_SOURCES = extractor.c
extractor_LDADD = ${top_builddir}/src/lib/libextractor_mini.la -lpthread
