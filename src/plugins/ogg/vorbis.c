/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* TODO 'ogg' should probably be replaced with 'oggvorbis' in all instances */

#include <tremor/ivorbisfile.h>
/* Macros to make Tremor's API look like libogg. Tremor always
   returns host-byte-order 16-bit signed data, and uses integer
   milliseconds where libogg uses double seconds.
*/
#define ov_read(VF, BUFFER, LENGTH, BIGENDIANP, WORD, SGNED, BITSTREAM) \
        ov_read(VF, BUFFER, LENGTH, BITSTREAM)
#define ov_time_total(VF, I) ((double)ov_time_total(VF, I)/1000)
#define ov_time_tell(VF) ((double)ov_time_tell(VF)/1000)
#define ov_time_seek_page(VF, S) (ov_time_seek_page(VF, (S)*1000))

#include <glib.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "vorbis"
#define OGG_CHUNK_SIZE 4096

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define OGG_DECODE_USE_BIGENDIAN	1
#else
#define OGG_DECODE_USE_BIGENDIAN	0
#endif

typedef struct _OggCallbackData {
	struct decoder *decoder;

	struct input_stream *input_stream;
	bool seekable;
} OggCallbackData;

static size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *vdata)
{
	size_t ret;
	OggCallbackData *data = (OggCallbackData *) vdata;

	ret = decoder_read(data->decoder, data->input_stream, ptr, size * nmemb);

	errno = 0;

	return ret / size;
}

static const char *
vorbis_comment_value(const char *comment, const char *needle)
{
	size_t len = strlen(needle);

	if (g_ascii_strncasecmp(comment, needle, len) == 0 &&
	    comment[len] == '=')
		return comment + len + 1;

	return NULL;
}

static struct replay_gain_info *
vorbis_comments_to_replay_gain(char **comments)
{
	struct replay_gain_info *rgi;
	const char *temp;
	bool found = false;

	rgi = replay_gain_info_new();

	while (*comments) {
		if ((temp =
		     vorbis_comment_value(*comments, "replaygain_track_gain"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].gain = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_album_gain"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].gain = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_track_peak"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].peak = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_album_peak"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].peak = atof(temp);
			found = true;
		}

		comments++;
	}

	if (!found) {
		replay_gain_info_free(rgi);
		rgi = NULL;
	}

	return rgi;
}

static const char *VORBIS_COMMENT_TRACK_KEY = "tracknumber";
static const char *VORBIS_COMMENT_DISC_KEY = "discnumber";

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
vorbis_copy_comment(struct tag *tag, const char *comment,
		    const char *name, enum tag_type tag_type)
{
	const char *value;

	value = vorbis_comment_value(comment, name);
	if (value != NULL) {
		tag_add_item(tag, tag_type, value);
		return true;
	}

	return false;
}

static void
vorbis_parse_comment(struct tag *tag, const char *comment)
{
	assert(tag != NULL);

	if (vorbis_copy_comment(tag, comment, VORBIS_COMMENT_TRACK_KEY,
				TAG_ITEM_TRACK) ||
	    vorbis_copy_comment(tag, comment, VORBIS_COMMENT_DISC_KEY,
				TAG_ITEM_DISC) ||
	    vorbis_copy_comment(tag, comment, "album artist",
				TAG_ITEM_ALBUM_ARTIST))
		return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (vorbis_copy_comment(tag, comment,
					tag_item_names[i], i))
			return;
}

static struct tag *
vorbis_comments_to_tag(char **comments)
{
	struct tag *tag = tag_new();

	while (*comments)
		vorbis_parse_comment(tag, *comments++);

	if (tag_is_empty(tag)) {
		tag_free(tag);
		tag = NULL;
	}

	return tag;
}

static void
vorbis_send_comments(struct decoder *decoder, struct input_stream *is,
		     char **comments)
{
	struct tag *tag;

	tag = vorbis_comments_to_tag(comments);
	if (!tag)
		return;

	decoder_tag(decoder, is, tag);
	tag_free(tag);
}


/// Переделать все на em_keyword_list_t* вместо struct tag *
em_keyword_list_t*
vorbis_tag_dup(const char *file, em_keyword_list_t* prev)
{
	em_keyword_list_t * ret;
	FILE *fp;
	OggVorbis_File vf;

	fp = fopen(file, "r");
	if (!fp) {
		return NULL;
	}

	if (ov_open(fp, &vf, NULL, 0) < 0) {
		fclose(fp);
		return NULL;
	}

	ret = vorbis_comments_to_tag(ov_comment(&vf, -1)->user_comments);

//	ret->time = (int)(ov_time_total(&vf, -1) + 0.5);

	ov_clear(&vf);
    fclose(fp);
	return ret;
}

