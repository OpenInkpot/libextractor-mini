INCLUDES = -I${top_srcdir}/src/lib
AM_CFLAGS=-Wall

plugin_LTLIBRARIES = libextractor_rtf.la

libextractor_rtf_la_SOURCES = extractor_rtf.c
libextractor_rtf_la_LIBADD = \
		 ${top_builddir}/src/lib/libextractor_mini.la
libextractor_rtf_la_LDFLAGS = -module -avoid-version

plugindir = $(libdir)/extractor-mini

install-exec-hook:
	mkdir -p  ${DESTDIR}${plugindir}
	$(LN_S) -f libextractor_rtf.so
