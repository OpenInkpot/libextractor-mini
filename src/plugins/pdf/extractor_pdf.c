/*
 * extractor_pdf.cpp -- part of libextractor-oi-extras
 *
 * Part of OpenInkpot project (http://openinkpot.org/)
 *
 * (c) 2009 Javier Torres <javitonino@gmail.com>
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

#include "fitz.h"
#include "mupdf.h"
#include <iconv.h>
#include <stdio.h>
#include <extractor-mini.h>

typedef enum {
    PDF_STRING,
    PDF_DATE
} data_type_t;

typedef struct {
    char *pdf_field;
    em_keyword_type_t extractor_keyword;
    data_type_t data_type;
} field_t;

#define NUM_FIELDS (sizeof(fields)/sizeof(fields[0]))
static field_t fields[] = {
    {"Title", EXTRACTOR_TITLE, PDF_STRING},
    {"Subject", EXTRACTOR_SUBJECT, PDF_STRING},
    {"Author", EXTRACTOR_AUTHOR, PDF_STRING},
    {"Creator", EXTRACTOR_CREATOR, PDF_STRING},
    {"Producer", EXTRACTOR_PRODUCER, PDF_STRING},
    {"Keywords", EXTRACTOR_KEYWORDS, PDF_STRING},
    {"CreationDate", EXTRACTOR_CREATION_DATE, PDF_DATE},
    {"ModDate", EXTRACTOR_MODIFICATION_DATE, PDF_DATE}
};

em_keyword_list_t *
libextractor_pdf_extract(const char *filename,
                         char *data,
                         size_t size,
                         em_keyword_list_t *prev)
{
    pdf_xref *xref;
    fz_error error;
    fz_obj *obj;

    xref = pdf_newxref();
    if (!xref)
        return prev;

    /* Open file instead of using passed data */
    error = pdf_loadxref(xref, (char*)filename);
    if (error)
        return prev;

    error = pdf_decryptxref(xref);
    if (error || xref->crypt) {
        pdf_closexref(xref);
        return prev;
    }

    /* Get Info dictionary */
    obj = fz_dictgets(xref->trailer, "Info");
    xref->info = fz_resolveindirect(obj);
    if (xref->info)
        fz_keepobj(xref->info);

    if (xref->info) {
        /* We have some metadata */
        prev = em_keywords_add(prev, EXTRACTOR_FILENAME, filename);
        prev = em_keywords_add(prev, EXTRACTOR_MIMETYPE, "application/pdf");

        int i;
        for (i = 0; i < NUM_FIELDS; i++) {
            obj = fz_dictgets(xref->info, fields[i].pdf_field);
            if (obj) {
                char *data = pdf_toutf8(obj);
                if (!data) {
                    int year, month, day, hour, minute, second;
                    char *date;
                    switch (fields[i].data_type) {
                        case PDF_STRING:
                            prev = em_keywords_add(prev, fields[i].extractor_keyword, data);
                            break;
                        case PDF_DATE:
                            date = data;
                            if (date[0] == 'D')
                                date += 2;
                            sscanf(date, "%4d%2d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute, &second);
                            if (asprintf(&date, "%d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second) >= 0)
                                prev = em_keywords_add(prev, fields[i].extractor_keyword, date);

                            free(data); /* Free original string */
                            break;
                    }
                }
            }
        }
    }
    pdf_closexref(xref);
    return prev;
}
