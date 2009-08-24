/*
 * extractor_mp3.c -- part of libextractor-oi-extras
 *
 * Part of OpenInkpot project (http://openinkpot.org/)
 *
 * (c) 2008 Marc Lajoie <quickhand@openinkpot.org>
 * (c) 2008 Mikhail Gusarov <dottedmag@dottedmag.net>
 * (c) 2008 Alexander V. Nikolaev <avn@daemon.hole.ru>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <expat.h>
#include <fcntl.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <extractor.h>
#include <id3tag.h>
#include "tag_id3.h"


EXTRACTOR_KeywordList* add_to_list(EXTRACTOR_KeywordList* next,
                                          EXTRACTOR_KeywordType type,
                                          char* keyword)
{
    EXTRACTOR_KeywordList* c = malloc(sizeof(EXTRACTOR_KeywordList));
    c->keyword = keyword;
    c->keywordType = type;
    c->next = next;
    return c;
}

EXTRACTOR_KeywordList* libextractor_mp3_extract(const char* filename,
                                                char* data,
                                                size_t size,
                                                EXTRACTOR_KeywordList* prev,
                                                const char* options)
{
   struct id3_tag *tag = tag_id3_load(filename);
   if (tag) {
       prev = add_to_list(prev, EXTRACTOR_MIMETYPE, "audio/mpeg");
       tag_id3_import(&prev, tag);
       id3_tag_delete(tag);
    };
   return prev;
}



