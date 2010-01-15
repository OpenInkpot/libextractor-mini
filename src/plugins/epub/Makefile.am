INCLUDES = @EXPAT_CFLAGS@ @LIBZIP_CFLAGS@ -I${top_srcdir}/src/lib
AM_CFLAGS=-Wall

plugin_LTLIBRARIES = libextractor_epub.la

libextractor_epub_la_SOURCES = extractor_epub.c
libextractor_epub_la_LIBADD = \
		 ${top_builddir}/src/lib/libextractor_mini.la \
		@EXPAT_LIBS@ @LIBZIP_LIBS@
libextractor_epub_la_LDFLAGS = -module -avoid-version

plugindir = $(libdir)/extractor-mini
