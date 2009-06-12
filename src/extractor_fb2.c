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

#include <extractor.h>
#include <zip.h>

#define BUF_SIZE 4096

int authorflag;
int titleflag;
int titleinfoflag;
int firstnameflag;
int middlenameflag;
int lastnameflag;
int doneflag;

/* Either NULL or zero-terminated string */
typedef struct
{
    char* value;
    size_t len;
} str_t;

void str_init(str_t* s)
{
    s->value = NULL;
    s->len = 0;
}

void str_fini(str_t* s)
{
    free(s->value);
    s->value = NULL;
    s->len = 0;
}

void str_append(str_t* s, const XML_Char* to_append, int len)
{
    if(!len)
        return;

    if(!s->value)
    {
        s->len = len;
        s->value = malloc((s->len + 1) * sizeof(char));
        if(!s->value)
            perror("str_append");
        memcpy(s->value, to_append, s->len);
        s->value[s->len] = 0;
    }
    else
    {
        s->value = realloc(s->value, (s->len + len + 1) * sizeof(char));
        if(!s->value)
            perror("str_append");
        memcpy(s->value + s->len, to_append, len);
        s->len += len;
        s->value[s->len] = 0;
    }
}

const char* str_get(str_t* s)
{
    return s->value ? s->value : "";
}

typedef struct author_t_
{
    str_t first;
    str_t middle;
    str_t last;

    struct author_t_* next;
} author_t;

author_t* authors;
str_t title;
str_t series;
str_t seq_number;

void add_author()
{
    author_t* newauthor = malloc(sizeof(author_t));
    str_init(&newauthor->first);
    str_init(&newauthor->middle);
    str_init(&newauthor->last);
    newauthor->next = authors;
    authors = newauthor;
}

void free_authors()
{
    author_t* author = authors;
    while(author)
    {
        author_t* next = author->next;
        str_fini(&author->first);
        str_fini(&author->middle);
        str_fini(&author->last);
        free(author);
        author = next;
    }
    authors = NULL;
}

void initvars()
{
    authorflag=0;
    titleflag=0;
    titleinfoflag=0;
    firstnameflag=0;
    middlenameflag=0;
    lastnameflag=0;
    doneflag=0;

    str_init(&title);
    str_init(&series);
    str_init(&seq_number);
}

void freevars()
{
    str_fini(&title);
    str_fini(&series);
    str_fini(&seq_number);
    free_authors();
}

void parse_sequence_info(const XML_Char** atts)
{
    for(; *atts; ++atts)
    {
        if(!strcmp(*atts, "name"))
        {
            ++atts;
            str_append(&series, *atts, strlen(*atts));
        }
        else if(!strcmp(*atts, "number"))
        {
            ++atts;
            str_append(&seq_number, *atts, strlen(*atts));
        }
    }
}

void handlestart(void* userData, const XML_Char* name, const XML_Char** atts)
{
    if(!strcmp(name, "title-info"))
        titleinfoflag = 1;

    if(titleinfoflag)
    {
        if(!strcmp(name, "book-title"))
            titleflag = 1;
        else if(!strcmp(name, "author"))
        {
            add_author();
            authorflag = 1;
        }
        else if(!strcmp(name, "sequence"))
            parse_sequence_info(atts);
    }

    if(authorflag)
    {
        if(!strcmp(name, "first-name"))
            firstnameflag = 1;
        else if(!strcmp(name, "middle-name"))
            middlenameflag = 1;
        else if(!strcmp(name, "last-name"))
            lastnameflag = 1;
    }

    if(!strcmp(name, "body"))
        doneflag = 1;
}

void handleend(void *userData,const XML_Char *name)
{
    if(strcmp(name,"title-info")==0)
        titleinfoflag=0;
    else if(strcmp(name,"book-title")==0 &&titleinfoflag)
        titleflag=0;
    else if(strcmp(name,"author")==0 && titleinfoflag)
        authorflag=0;
    else if(strcmp(name,"first-name")==0 && authorflag)
        firstnameflag=0;
    else if(strcmp(name,"middle-name")==0 && authorflag)
        middlenameflag=0;
    else if(strcmp(name,"last-name")==0 && authorflag)
        lastnameflag=0;
    else if(strcmp(name,"title-info")==0)
        doneflag=1;
}

void handlechar(void *userData,const XML_Char *s,int len)
{
    if(titleflag==1)
        str_append(&title, s, len);
    else if(firstnameflag)
    {
        str_append(&authors->first, s, len);
    }
    else if(middlenameflag)
    {
        str_append(&authors->middle, s, len);
    }
    else if(lastnameflag)
    {
        str_append(&authors->last, s, len);
    }
}

/*
 * This function fills the 256-byte Unicode conversion table for single-byte
 * encoding. The easy way to do it is to convert every byte to UTF-32 and then
 * construct Unicode character from 4-byte representation.
 */
int fill_byte_encoding_table(const char* encoding, XML_Encoding* info)
{
    int i;

    iconv_t ic = iconv_open("UTF-32BE", encoding);
    if(ic == (iconv_t)-1)
        return XML_STATUS_ERROR;

    for(i = 0; i < 256; ++i)
    {
        char from[1] = { i };
        unsigned char to[4];

        char* fromp = from;
        unsigned char* top = to;
        size_t fromleft = 1;
        size_t toleft = 4;

        size_t res = iconv(ic, &fromp, &fromleft, (char**)&top, &toleft);

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
            info->map[i] = to[0] * (1<<24) + to[1] * (1 << 16) + to[2] * (1 << 8) + to[3];
    }

    return XML_STATUS_OK;
}

static int unknown_encoding_handler(void* user,
                                    const XML_Char* name,
                                    XML_Encoding* info)
{
    /*
     * Just pretend that all encodings are single-byte :)
     */
    return fill_byte_encoding_table(name, info);
}


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

static void setup_fb2_parser(XML_Parser myparse)
{
    XML_UseParserAsHandlerArg(myparse);
    XML_SetElementHandler(myparse,handlestart,handleend);
    XML_SetCharacterDataHandler(myparse,handlechar);
    XML_SetUnknownEncodingHandler(myparse, unknown_encoding_handler, NULL);
}

static EXTRACTOR_KeywordList* append_fb2_keywords(EXTRACTOR_KeywordList* prev)
{
    if(title.value)
    {
        char* title_copy = strdup(title.value);
        if(!title_copy)
            perror("append_fb2_keywords");
        prev = add_to_list(prev, EXTRACTOR_TITLE, title_copy);
    }

    author_t* author = authors;
    while(author)
    {
        if(author->first.value || author->middle.value || author->last.value)
        {
            char* a;

            int r = asprintf(&a, "%s%s%s%s%s",
                             str_get(&author->first),
                             author->middle.value ? " " : "",
                             str_get(&author->middle),
                             author->last.value ? " " : "",
                             str_get(&author->last));
            if(!r)
                perror("append_fb2_keywords");

            prev = add_to_list(prev, EXTRACTOR_AUTHOR, a);
        }
        author = author->next;
    }

    if(series.value)
    {
        char* series_copy = strdup(series.value);
        if(!series_copy)
            perror("append_fb2_keywords");
        prev = add_to_list(prev, EXTRACTOR_ALBUM, series_copy);
    }

    if(seq_number.value)
    {
        char* seq_number_copy = strdup(seq_number.value);
        if(!seq_number_copy)
            perror("append_fb2_keywords");
        prev = add_to_list(prev, EXTRACTOR_TRACK_NUMBER, seq_number_copy);
    }

    return prev;
}

EXTRACTOR_KeywordList* libextractor_fb2_extract(const char* filename,
                                                char* data,
                                                size_t size,
                                                EXTRACTOR_KeywordList* prev,
                                                const char* options)
{
    XML_Parser myparse = XML_ParserCreate(NULL);
    initvars();
    setup_fb2_parser(myparse);

    /* Read file in chunks, stopping as soon as necessary */
    while(!doneflag && size)
    {
        size_t part_size = BUF_SIZE < size ? BUF_SIZE : size;

        if(XML_Parse(myparse, data, part_size, part_size == size) == XML_STATUS_ERROR)
            goto err;

        data += part_size;
        size -= part_size;
    }

    prev = add_to_list(prev, EXTRACTOR_MIMETYPE, "application/x-fb2");
    prev = append_fb2_keywords(prev);

err:
    freevars();
    XML_ParserFree(myparse);
    return prev;
}

static int parse_zipped_fb2(XML_Parser myparse, const char* filename)
{
    struct zip* z;
    struct zip_file* zf;

    /* libzip does not allow to open zip file from memory. Wink-wink. */
    z = zip_open(filename, 0, NULL);
    if(!z)
        return 0;

    zf = zip_fopen_index(z, 0, 0);
    if(!zf)
        goto err2;

    while(!doneflag)
    {
        char buf[BUF_SIZE];
        int nr = zip_fread(zf, buf, BUF_SIZE);

        if(nr == -1)
            goto err1;

        if(XML_Parse(myparse, buf, nr, nr == 0) == XML_STATUS_ERROR)
            goto err1;

        if(nr == 0)
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

EXTRACTOR_KeywordList* libextractor_fb2_zip_extract(const char* filename,
                                                    char* data,
                                                    size_t size,
                                                    EXTRACTOR_KeywordList* prev,
                                                    const char* options)
{
    XML_Parser myparse = XML_ParserCreate(NULL);

    initvars();
    setup_fb2_parser(myparse);

    if(parse_zipped_fb2(myparse, filename))
    {
        prev = add_to_list(prev, EXTRACTOR_MIMETYPE,
                           "application/x-zip-compressed-fb2");
        prev = append_fb2_keywords(prev);
    }

    XML_ParserFree(myparse);
    return prev;
}
