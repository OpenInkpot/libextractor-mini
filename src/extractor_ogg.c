/*
 * extractor_ogg.c -- part of libextractor-oi-extras
 *
 * Part of OpenInkpot project (http://openinkpot.org/)
 *
 * (c) 2008 Marc Lajoie <quickhand@openinkpot.org>
 * (c) 2008 Alexander V. Nikolaev <avn@daemon.hole.ru>
 * (c) 2009 Mikhail Gusarov <dottedmag@dottedmag.net>
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

#define BUF_SIZE 4096


static EXTRACTOR_KeywordList* add_to_list(EXTRACTOR_KeywordList* next,
                                          EXTRACTOR_KeywordType type,
                                          char* keyword)
{
    EXTRACTOR_KeywordList* c = malloc(sizeof(EXTRACTOR_KeywordList));
    c->keyword = keyword;
    c->keywordType = type;
    c->next = next;
    return c;
}



EXTRACTOR_KeywordList* libextractor_ogg_extract(const char* filename,
                                                char* data,
                                                size_t size,
                                                EXTRACTOR_KeywordList* prev,
                                                const char* options)
{
    return prev;
}



