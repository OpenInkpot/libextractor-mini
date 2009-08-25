INCLUDES = @EXTRACT_CFLAGS@ @EXPAT_CFLAGS@ @LIBZIP_CFLAGS@
AM_CFLAGS=-Wall

lib_LTLIBRARIES = libextractor_fb2.la libextractor_ogg.la 

libextractor_fb2_la_SOURCES = extractor_fb2.c
libextractor_fb2_la_LIBADD = @EXTRACT_LIBS@ @EXPAT_LIBS@ @LIBZIP_LIBS@
libextractor_fb2_la_LDFLAGS = -module

libextractor_ogg_la_SOURCES = extractor_ogg.c
libextractor_ogg_la_LIBADD = @EXTRACT_LIBS@ -lvorbisidec
libextractor_ogg_la_LDFLAGS = -module

