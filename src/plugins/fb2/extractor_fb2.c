/*
 * extractor_fb2.c -- part of libextractor-oi-extras
 *
 * Part of OpenInkpot project (http://openinkpot.org/)
 *
 * (c) 2008 Marc Lajoie <quickhand@openinkpot.org>
 * (c) 2008 Mikhail Gusarov <dottedmag@dottedmag.net>
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

#include <extractor-mini.h>
#include <zip.h>

#define BUF_SIZE 4096

/* Either NULL or zero-terminated string */
typedef struct {
    char* value;
    size_t len;
} str_t;

static void
str_fini(str_t *s)
{
    free(s->value);
    s->value = NULL;
    s->len = 0;
}

static void
str_append(str_t *s, const XML_Char *to_append, int len)
{
    if (!len)
        return;

    s->value = realloc(s->value, (s->len + len + 1) * sizeof(char));
    if (!s->value)
        perror("str_append");
    memcpy(s->value + s->len, to_append, len);
    s->len += len;
    s->value[s->len] = 0;
}

static const char *
str_get(str_t *s)
{
    return s->value ? s->value : "";
}

typedef struct author_t_ {
    str_t first;
    str_t middle;
    str_t last;

    struct author_t_* next;
} author_t;

typedef struct data_t_ {
    /* parsing state */
    int authorflag;
    int titleflag;
    int titleinfoflag;
    int firstnameflag;
    int middlenameflag;
    int lastnameflag;
    int doneflag;

    /* results */
    author_t* authors;
    str_t title;
    str_t series;
    str_t seq_number;
} data_t;

static void
add_author(author_t **authors)
{
    author_t *newauthor = calloc(1, sizeof(author_t));
    newauthor->next = *authors;
    *authors = newauthor;
}

static void
free_authors(author_t **authors)
{
    author_t *author = *authors;
    while(author)
    {
        author_t *next = author->next;
        str_fini(&author->first);
        str_fini(&author->middle);
        str_fini(&author->last);
        free(author);
        author = next;
    }
    *authors = NULL;
}

static void
initdata(data_t *data)
{
    memset(data, 0x00, sizeof(data_t));
}

static void
freedata(data_t *data)
{
    str_fini(&data->title);
    str_fini(&data->series);
    str_fini(&data->seq_number);
    free_authors(&data->authors);
}

#define streq(a,b) (strcmp((a),(b)) == 0)

static void
parse_sequence_info(data_t *data, const XML_Char **atts)
{
    for(; *atts; ++atts) {
        if(streq(*atts, "name")) {
            ++atts;
            str_append(&data->series, *atts, strlen(*atts));
        } else if(streq(*atts, "number")) {
            ++atts;
            str_append(&data->seq_number, *atts, strlen(*atts));
        }
    }
}

static void
handlestart(void *userData, const XML_Char *name, const XML_Char **atts)
{
    data_t *data = userData;

    if(streq(name, "body"))
        data->doneflag = 1;

    if (!data->titleinfoflag) {
        if (streq(name, "title-info"))
            data->titleinfoflag = 1;
        return;
    }

    if (!data->authorflag) {
        if (streq(name, "author")) {
            add_author(&data->authors);
            data->authorflag = 1;
        }
    } else /* (data->authorflag) */ {
        if (streq(name, "first-name"))
            data->firstnameflag = 1;
        else if (streq(name, "middle-name"))
            data->middlenameflag = 1;
        else if (streq(name, "last-name"))
            data->lastnameflag = 1;
        return;
    }

    if (streq(name, "book-title"))
        data->titleflag = 1;
    else if(streq(name, "sequence"))
        parse_sequence_info(data, atts);
}

static void
handleend(void *userData,const XML_Char *name)
{
    data_t *data = userData;

    if (!data->titleinfoflag)
        return;

    if (data->authorflag) {
        if (data->firstnameflag && streq(name, "first-name"))
            data->firstnameflag = 0;
        else if (data->middlenameflag && streq(name, "middle-name"))
            data->middlenameflag = 0;
        else if (data->lastnameflag && streq(name, "last-name"))
            data->lastnameflag = 0;
        else if (streq(name, "author"))
            data->authorflag = 0;          
        return;
    }

    if (streq(name, "book-title"))
        data->titleflag = 0;
    else if (streq(name, "title-info"))
        data->doneflag = 1;
}

static void
handlechar(void *userData, const XML_Char *s,int len)
{
    data_t *data = userData;

    if (data->titleflag)
        str_append(&data->title, s, len);
    else if (data->firstnameflag)
        str_append(&data->authors->first, s, len);
    else if (data->middlenameflag)
        str_append(&data->authors->middle, s, len);
    else if (data->lastnameflag)
        str_append(&data->authors->last, s, len);
}

/*
 * This function fills the 256-byte Unicode conversion table for single-byte
 * encoding. The easy way to do it is to convert every byte to UTF-32 and then
 * construct Unicode character from 4-byte representation.
 */
int fill_byte_encoding_table(const char *encoding, XML_Encoding *info)
{
    int i;

    iconv_t ic = iconv_open("UTF-32BE", encoding);
    if (ic == (iconv_t)-1)
        return XML_STATUS_ERROR;

    for (i = 0; i < 256; ++i)
    {
        char from[1] = { i };
        unsigned char to[4];

        char* fromp = from;
        unsigned char* top = to;
        size_t fromleft = 1;
        size_t toleft = 4;

        size_t res = iconv(ic, &fromp, &fromleft, (char **)&top, &toleft);

        if(res == (size_t) -1 && errno == EILSEQ)
        {
            info->map[i] = -1;
        }
        else if(res == (size_t) -1)
        {
            iconv_close(ic);
            return XML_STATUS_ERROR;
        }
        else
            info->map[i] = to[0] * (1 << 24) + to[1] * (1 << 16) + to[2] * (1 << 8) + to[3];
    }

    iconv_close(ic);
    return XML_STATUS_OK;
}

static int unknown_encoding_handler(void *user,
                                    const XML_Char *name,
                                    XML_Encoding *info)
{
    /*
     * Just pretend that all encodings are single-byte :)
     */
    return fill_byte_encoding_table(name, info);
}


static void setup_fb2_parser(XML_Parser myparse, data_t *data)
{
    XML_SetUserData(myparse, data);
    XML_SetElementHandler(myparse, handlestart, handleend);
    XML_SetCharacterDataHandler(myparse, handlechar);
    XML_SetUnknownEncodingHandler(myparse, unknown_encoding_handler, NULL);
}

static em_keyword_list_t *append_fb2_keywords(em_keyword_list_t *prev,
                                              data_t *data)
{
    if (data->title.value) {
        prev = em_keywords_add(prev, EXTRACTOR_TITLE, data->title.value);
    }

    author_t *author = data->authors;
    while (author) {
        if (author->first.value || author->middle.value || author->last.value) {
            char *a;

            int r = asprintf(&a, "%s%s%s%s%s",
                             str_get(&author->first),
                             author->middle.value ? " " : "",
                             str_get(&author->middle),
                             author->last.value ? " " : "",
                             str_get(&author->last));
            if (!r)
                perror("append_fb2_keywords");

            prev = em_keywords_add(prev, EXTRACTOR_AUTHOR, a);
            free(a);
        }
        author = author->next;
    }

    if (data->series.value) {
        prev = em_keywords_add(prev, EXTRACTOR_ALBUM, data->series.value);
    }

    if (data->seq_number.value) {
        prev = em_keywords_add(prev, EXTRACTOR_TRACK_NUMBER, data->seq_number.value);
    }

    return prev;
}

em_keyword_list_t *libextractor_fb2_extract(const char *filename,
                                            char *data,
                                            size_t size,
                                            em_keyword_list_t *prev)
{
    XML_Parser myparse = XML_ParserCreate(NULL);
    data_t metadata;

    initdata(&metadata);
    setup_fb2_parser(myparse, &metadata);

    /* Read file in chunks, stopping as soon as necessary */
    while (!metadata.doneflag && size) {
        size_t part_size = BUF_SIZE < size ? BUF_SIZE : size;

        if (XML_Parse(myparse, data, part_size, part_size == size) == XML_STATUS_ERROR)
            goto err;

        data += part_size;
        size -= part_size;
    }

    prev = em_keywords_add(prev, EXTRACTOR_MIMETYPE, "application/x-fictionbook+xml");
    prev = append_fb2_keywords(prev, &metadata);

err:
    freedata(&metadata);
    XML_ParserFree(myparse);
    return prev;
}

static int parse_zipped_fb2(XML_Parser myparse,
                            const char *filename,
                            data_t *data)
{
    struct zip *z;
    struct zip_file *zf;

    /* libzip does not allow to open zip file from memory. Wink-wink. */
    z = zip_open(filename, 0, NULL);
    if (!z)
        return 0;

    zf = zip_fopen_index(z, 0, 0);
    if (!zf)
        goto err2;

    while (!data->doneflag) {
        char buf[BUF_SIZE];
        int nr = zip_fread(zf, buf, BUF_SIZE);

        if (nr == -1)
            goto err1;

        if (XML_Parse(myparse, buf, nr, nr == 0) == XML_STATUS_ERROR)
            goto err1;

        if (nr == 0)
            break;
    }

    zip_fclose(zf);
    zip_close(z);
    return 1;

err1:
    zip_fclose(zf);
err2:
    zip_close(z);
    return 0;
}

em_keyword_list_t *libextractor_fb2_zip_extract(const char *filename,
                                                char *data,
                                                size_t size,
                                                em_keyword_list_t *prev)
{
    XML_Parser myparse = XML_ParserCreate(NULL);
    data_t metadata;

    initdata(&metadata);
    setup_fb2_parser(myparse, &metadata);

    if (parse_zipped_fb2(myparse, filename, &metadata)) {
        prev = em_keywords_add(prev, EXTRACTOR_MIMETYPE,
                           "application/x-zip-compressed-fb2");
        prev = append_fb2_keywords(prev, &metadata);
    }

    freedata(&metadata);
    XML_ParserFree(myparse);
    return prev;
}
