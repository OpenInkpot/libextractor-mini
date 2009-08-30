INCLUDES = @EXPAT_CFLAGS@ @LIBZIP_CFLAGS@ -I${top_srcdir}/src/lib
AM_CFLAGS=-Wall

plugin_LTLIBRARIES = libextractor_fb2.la

libextractor_fb2_la_SOURCES = extractor_fb2.c
libextractor_fb2_la_LIBADD = \
		 ${top_builddir}/src/lib/libextractor_mini.la \
		@EXPAT_LIBS@ @LIBZIP_LIBS@
libextractor_fb2_la_LDFLAGS = -module -avoid-version

plugindir = $(libdir)/extractor-mini

install-exec-hook:
	mkdir -p  ${DESTDIR}${plugindir}
	$(LN_S) -f libextractor_fb2.so ${DESTDIR}${plugindir}/libextractor_fb2_zip.so
