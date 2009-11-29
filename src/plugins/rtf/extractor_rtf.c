/*
 * extractor_rtf.c -- part of libextractor-oi-extras
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

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <extractor-mini.h>

#define RTF_START "{\\rtf"
#define RTF_START_LEN 5  
#define TAG_BUFFER 2048

typedef enum {
    RTF_STRING,
    RTF_DATE
} data_type_t;

typedef struct {
    char* rtf_field;
    em_keyword_type_t extractor_keyword;
    data_type_t data_type;
} field_t;

#define NUM_FIELDS (sizeof(fields)/sizeof(fields[0]))
field_t fields[] = {
    {"\\title", EXTRACTOR_TITLE, RTF_STRING},
    {"\\subject", EXTRACTOR_SUBJECT, RTF_STRING},
    {"\\author", EXTRACTOR_AUTHOR, RTF_STRING},
    {"\\manager", EXTRACTOR_MANAGER, RTF_STRING},
    {"\\company", EXTRACTOR_COMPANY, RTF_STRING},
    {"\\category", EXTRACTOR_CATEGORY, RTF_STRING},
    {"\\keywords", EXTRACTOR_KEYWORDS, RTF_STRING},
    {"\\doccomm", EXTRACTOR_COMMENT, RTF_STRING},
    {"\\creatim", EXTRACTOR_CREATION_DATE, RTF_DATE},
    {"\\revtim", EXTRACTOR_MODIFICATION_DATE, RTF_DATE},
    {"\\printim", EXTRACTOR_LAST_PRINTED, RTF_DATE}
};
/* Some other ignored fields:
 * \comment: Usually the software used to create the file
 * \operator
 */
 

/* Parse a date and return a string in ISO format (or similar)
   Return string must be freed */
char *
parse_date(char *tags)
{
    char *pos = tags;
    int year, month, day, hour, min, sec;
    year = month = day = hour = min = sec = -1;
    
    while (pos != NULL && pos < tags + strlen(tags)) {
        if (strncmp(pos, "\\yr", 3) == 0) {
            pos += 3;
            year = atoi(pos);
            pos = index(pos, '\\');
        } else if (strncmp(pos, "\\mo", 3) == 0) {
            pos += 3;
            month = atoi(pos);
            pos = index(pos, '\\');
        } else if (strncmp(pos, "\\dy", 3) == 0) {
            pos += 3;
            day = atoi(pos);
            pos = index(pos, '\\');
        } else if (strncmp(pos, "\\hr", 3) == 0) {
            pos += 3;
            hour = atoi(pos);
            pos = index(pos, '\\');
        } else if (strncmp(pos, "\\min", 4) == 0) {
            pos += 4;
            min = atoi(pos);
            pos = index(pos, '\\');
        } else if (strncmp(pos, "\\sec", 4) == 0) {
            pos += 4;
            sec = atoi(pos);
            pos = index(pos, '\\');
        }
    }
    char *date;
    char *hours;
    bool adate, ahours;
    adate = ahours = false;
    if (year > 0) {
        adate = true;
        if (month > 0 && day > 0)
            asprintf(&date, "%d-%02d-%02d", year, month, day);
        else
            asprintf(&date, "%d", year);
    }

    if (hour >= 0 && min >= 0) {
        ahours = true;
        if (sec >= 0)
            asprintf(&hours, "%02d:%02d:%02d", hour, min, sec);
        else
            asprintf(&hours, "%02d:%02d", hour, min);
    }
    
    char *result;
    asprintf(&result, "%s%s%s", adate ? date : "", (adate && ahours) ? " " : "", ahours ? hours : "");
    if (adate)
        free(date);
    if (ahours)
        free(hours);
    return result;
}

/* Parse a tag and append it to the libextractor keywords */
em_keyword_list_t *
parse_tag(char *tag, em_keyword_list_t *prev)
{
    int len = strlen(tag);
    int i;
    for (i = 0; i < NUM_FIELDS; i++) {
        int l = strlen(fields[i].rtf_field);
        if (l <= len && strncmp(fields[i].rtf_field, tag, l) == 0) {
            char *date;
            switch (fields[i].data_type) {
                case RTF_STRING:
                    prev = em_keywords_add(prev, fields[i].extractor_keyword, strdup(tag+l+1));
                    break;
                case RTF_DATE:
                    date = parse_date(tag+l);
                    prev = em_keywords_add(prev, fields[i].extractor_keyword, date);
                    break;
            }
            break;
        }
    }
    return prev;
}

em_keyword_list_t *
libextractor_rtf_extract(const char *filename, 
                         char *data,
                         size_t size,
                         em_keyword_list_t* prev)
{
    if (size < RTF_START_LEN || strncmp(data, RTF_START, RTF_START_LEN != 0))
        return prev;
    
    char ch;
    int level = 0;
    bool last_slash = false;
    bool started = false;
    bool info_found = false;
    char waiting = 'i';
    int pos = RTF_START_LEN;
    
    /* Mini parser to skip to \info */
    while (level >= 0 && pos < size) {
        ch = data[pos++];
        switch (ch) {
            case '\\':
                last_slash = !last_slash;
                if (level == 0 && started)
                    level = -1;
                waiting = 'i';
                break;
            case '{':
                if (!last_slash) {
                    level++;
                    started = true;
                }
                break;
            case '}':
                if (!last_slash)
                    level--;
                break;
            default:
                last_slash = false;
                if (ch == waiting) {
                    switch (ch) {
                        case 'i':
                            waiting = 'n';
                            break;
                        case 'n':
                            waiting = 'f';
                            break;
                        case 'f':
                            waiting = 'o';
                            break;
                        case 'o':
                            info_found = true;
                            level = -1;
                            break;
                    }
                }
                else
                    waiting = '\0';
                break;
        }
    }
    
    /* We found \info or some other tag that comes after \info */
    if (info_found) {
        prev = em_keywords_add(prev, EXTRACTOR_FILENAME, filename);
        prev = em_keywords_add(prev, EXTRACTOR_MIMETYPE, "application/rtf");
        
        level = 0;
        last_slash = false;
        char tag[TAG_BUFFER];
        int tag_pos = 0;
        
        /* Scan tags */
        while (level >= 0 && pos < size) {
            ch = data[pos++];
            /* At least don't overrun the buffer */
            if (tag_pos >= TAG_BUFFER - 1)
                tag_pos = TAG_BUFFER - 1;
            switch (ch) {
                case '\\':
                    tag[tag_pos++] = ch;
                    last_slash = !last_slash;
                    break;
                case '{':
                    if (!last_slash) {
                        level = 1; /* Helps in some invalid file situations */
                        tag_pos = 0;
                    }
                    else
                        tag[tag_pos++] = ch;
                    break;
                case '}':
                    if (!last_slash) {
                        level--;
                        if (level >= 0) {
                            tag[tag_pos] = '\0';
                            prev = parse_tag(tag, prev);
                        }
                    }
                    else
                        tag[tag_pos++] = ch;
                    break;
                default:
                    last_slash = false;
                    tag[tag_pos++] = ch;
                    break;
            }
        }
    }
    return prev;
}
