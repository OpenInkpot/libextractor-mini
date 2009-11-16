INCLUDES = @ID3TAG_CFLAGS@ @GLIB_CFLAGS@ -I${top_srcdir}/src/lib
AM_CFLAGS=-Wall -Werror -DHAVE_ID3TAG

plugin_LTLIBRARIES = libextractor_mp3.la

libextractor_mp3_la_SOURCES = extractor_mp3.c tag_id3.c riff.c aiff.c utf8.c
libextractor_mp3_la_LIBADD = @ID3TAG_LIBS@ @GLIB_LIBS@ \
	    ${top_builddir}/src/lib/libextractor_mini.la
libextractor_mp3_la_LDFLAGS = -module -avoid-version

plugindir = $(libdir)/extractor-mini
