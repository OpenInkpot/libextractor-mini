INCLUDES = @EXTRACT_CFLAGS@ @ID3TAG_CFLAGS@ @GLIB_CFLAGS@
AM_CFLAGS=-Wall -Werror -DHAVE_ID3TAG

lib_LTLIBRARIES = libextractor_mp3.la

libextractor_mp3_la_SOURCES = extractor_mp3.c tag_id3.c riff.c aiff.c
libextractor_mp3_la_LIBADD = @ID3TAG_LIBS@ @GLIB_LIBS@ @EXTRACT_LIBS@
libextractor_mp3_la_LDFLAGS = -module

