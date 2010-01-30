/*
 * extractor_epub.c -- part of libextractor-oi-extras
 *
 * Part of OpenInkpot project (http://openinkpot.org/)
 *
 * (c) 2010 Alan Jenkins <alan-jenkins@tuffmail.co.uk>
 *
 * Copied from extractor_fb2.c which is
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <extractor-mini.h>
#include <zip.h>

#define BUF_SIZE 4096

/* Either NULL or zero-terminated string */
typedef struct {
    char *value;
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

typedef struct author_t_ {
    str_t name;

    struct author_t_ *next;
} author_t;

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
    while (author) {
        author_t *next = author->next;
        str_fini(&author->name);
        free(author);
        author = next;
    }
    *authors = NULL;
}

typedef struct data_t_ {
    /* parsing state */
    int metadataflag;
    int authorflag;
    int titleflag;
    int doneflag;

    /* results */
    str_t opf_filename;
    author_t *authors;
    str_t title;
} data_t;

static void
initdata(data_t *data)
{
    memset(data, 0x00, sizeof(data_t));
}

static void
freedata(data_t *data)
{
    str_fini(&data->opf_filename);
    str_fini(&data->title);
    free_authors(&data->authors);
}

#define streq(a,b) (strcmp((a),(b)) == 0)
#define strstarts(a,start) (strncmp((a),(start), strlen(start)) == 0)

/* Strip dc namespace. dc and dcterms are identical for our purposes */
static const char *
dcname(const char *name)
{
    if (strstarts(name, "http://purl.org/dc/elements/1.1/|"))
        name += strlen("http://purl.org/dc/elements/1.1/|");
    else if (strstarts(name, "http://purl.org/dc/terms/|"))
        name += strlen("http://purl.org/dc/terms/|");
    else
        name = NULL;

    return name;
}

/*
 * XML is case sensitive.
 * Nevertheless, OEBPS 1.2 uses "dc:Title" whereas OPF 2.0 uses "dc:title".
 * I suspect the OEBPS definition is buggy.
 */
static int
dcmatch(const char *name, const char *match)
{
    if (name[0] >= 'A' && name[0] <= 'Z') {
        if (tolower(name[0]) == match[0])
            return 0;
        name++;
        match++;
    }
    return streq(name, match);
}

static void
handlestart(void *userData, const XML_Char *name, const XML_Char **atts)
{
    data_t *data = userData;
    const char *dc;

    /* OEBPS <= 1.2 and OPF 2.0 respectively */
    if (streq(name, "metadata") ||
        streq(name, "http://www.idpf.org/2007/opf|metadata")) {
        data->metadataflag = 1;
        return;
    }

    if (data->metadataflag) {
        dc = dcname(name);
        if (dc) {
            if (dcmatch(dc, "title"))
                data->titleflag = 1;
            else if (dcmatch(dc, "creator")) {
                add_author(&data->authors);
                data->authorflag = 1;
            }
        }
    }
}

static void
handleend(void *userData, const XML_Char *name)
{
    data_t *data = userData;
    const char *dc;

    if (data->metadataflag) {
        if (streq(name, "metadata")) {
            data->doneflag = 1;
            return;
        }

        dc = dcname(name);
        if (dc) {
            if (dcmatch(dc, "title"))
                data->titleflag = 0;
            else if (dcmatch(dc, "creator"))
                data->authorflag = 0;
        }
    }
}

static void
handlechar(void *userData, const XML_Char *s, int len)
{
    data_t *data = userData;

    if (data->titleflag)
        str_append(&data->title, s, len);
    else if (data->authorflag)
        str_append(&data->authors->name, s, len);
}

static void
ocf_handlestart(void *userData, const XML_Char *name, const XML_Char **atts)
{
    data_t *data = userData;

    if (streq(name, "rootfile")) {
        const char *filename = NULL;
        const char *type = NULL;

        for(; *atts; ++atts) {
            if (streq(*atts, "media-type")) {
                ++atts;
                type = *atts;
            } else if (streq(*atts, "full-path")) {
                ++atts;
                filename = *atts;
            } else {
                ++atts;
            }
        }

        if (streq(type, "application/oebps-package+xml")) {
            str_append(&data->opf_filename, filename, strlen(filename));
            data->doneflag = 1;
        }
    }
}

static void
setup_opf_parser(XML_Parser myparse, data_t *data)
{
    XML_SetUserData(myparse, data);
    XML_SetElementHandler(myparse, handlestart, handleend);
    XML_SetCharacterDataHandler(myparse, handlechar);
}

static void
setup_ocf_parser(XML_Parser myparse, data_t *data)
{
    XML_SetUserData(myparse, data);
    XML_SetElementHandler(myparse, ocf_handlestart, NULL);
}

static em_keyword_list_t *
append_epub_keywords(em_keyword_list_t *prev, data_t *data)
{
    if(data->title.value) {
        prev = em_keywords_add(prev, EXTRACTOR_TITLE, data->title.value);
    }

    author_t *author = data->authors;
    while (author) {
        if (author->name.value)
            prev = em_keywords_add(prev, EXTRACTOR_AUTHOR, author->name.value);
        author = author->next;
    }

    return prev;
}

static bool
parse_epub(struct zip *z, data_t *data)
{
    bool success = false;
    struct zip_file *zf;
    char buf[BUF_SIZE];
    int nr;
    XML_Parser myparse;

    /* Check it's really an EPUB and not some other zip-based format */
    zf = zip_fopen(z, "mimetype", 0);
    if (!zf)
        goto free_zip;

    nr = zip_fread(zf, buf, BUF_SIZE);
    if (nr < strlen("application/epub+zip"))
        goto free_zipfile;
    if (!strstarts(buf, "application/epub+zip"))
        goto free_zipfile;

    zip_fclose(zf);

    /* Parse container.xml to find the OPF file */
    zf = zip_fopen(z, "META-INF/container.xml", 0);
    if (!zf)
        goto free_zip;

    myparse = XML_ParserCreate(NULL);
    setup_ocf_parser(myparse, data);

    while (!data->doneflag) {
        nr = zip_fread(zf, buf, BUF_SIZE);
        if (nr == -1)
            goto free_xml;

        if (XML_Parse(myparse, buf, nr, nr == 0) == XML_STATUS_ERROR)
            goto free_xml;

        if (nr == 0)
            break;
    }
    XML_ParserFree(myparse);
    zip_fclose(zf);

    /* Parse OPF file */
    if (!data->opf_filename.value)
        return 0;
    zf = zip_fopen(z, data->opf_filename.value, 0);
    if (!zf)
        return 0;

    myparse = XML_ParserCreateNS(NULL, '|');
    setup_opf_parser(myparse, data);
    data->doneflag = 0;

    while (!data->doneflag) {
        nr = zip_fread(zf, buf, BUF_SIZE);
        if (nr == -1)
            goto free_xml;

        if (XML_Parse(myparse, buf, nr, nr == 0) == XML_STATUS_ERROR)
            goto free_xml;

        if (nr == 0)
            break;
    }
    success = true;

free_xml:
    XML_ParserFree(myparse);
free_zipfile:
    zip_fclose(zf);
free_zip:
    zip_close(z);
    return success;
}

em_keyword_list_t *
libextractor_epub_extract(const char *filename, char *data,
                          size_t size, em_keyword_list_t *prev)
{
    data_t metadata;
    struct zip *z;

    initdata(&metadata);

    /* libzip does not allow to open zip file from memory. Wink-wink. */
    z = zip_open(filename, 0, NULL);
    if (!z)
        return prev;

    if (parse_epub(z, &metadata)) {
        prev = em_keywords_add(prev, EXTRACTOR_MIMETYPE,
                           "application/epub+zip");
        prev = append_epub_keywords(prev, &metadata);
    }
    freedata(&metadata);

    return prev;
}
