INCLUDES = @MUPDF_CFLAGS@ -I${top_srcdir}/src/lib
AM_CFLAGS=-Wall

plugin_LTLIBRARIES = libextractor_pdf.la

libextractor_pdf_la_SOURCES = extractor_pdf.c
libextractor_pdf_la_LIBADD = \
		 ${top_builddir}/src/lib/libextractor_mini.la @MUPDF_LIBS@
libextractor_pdf_la_LDFLAGS = -module -avoid-version

plugindir = $(libdir)/extractor-mini

install-exec-hook:
	mkdir -p  ${DESTDIR}${plugindir}
	$(LN_S) -f libextractor_pdf.so
