INCLUDES = -I${top_srcdir}/src/lib
AM_CFLAGS=-Wall

plugin_LTLIBRARIES =  libextractor_ogg.la 

libextractor_ogg_la_SOURCES = extractor_ogg.c
libextractor_ogg_la_LIBADD = -lvorbisidec \
	${top_builddir}/src/lib/libextractor_mini.la
libextractor_ogg_la_LDFLAGS = -module -avoid-version
plugindir = $(libdir)/extractor-mini
