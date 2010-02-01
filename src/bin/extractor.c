/*
 * libextractor - metadata extraction tool
 *
 * Copyright (C) 2010 Mikhail Gusarov <dottedmag@dottedmag.net>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extractor-mini.h"

static void
parse(em_extractors_t* ex, const char *filename)
{
    em_keyword_list_t *list = em_extractor_get_keywords(ex, filename);
    em_keyword_list_t *item = list;
    while (item) {
        printf("%d -> %s\n", item->keyword_type, item->keyword);
        item = item->next;
    }
    em_keywords_free(list);
}


int
main(int argc, char *argv[])
{
    em_extractors_t *ex = em_load_extractors();

    if (argc == 1) {
        char buf[65535];
        char *f = fgets(buf, 65535, stdin);
        while (f) {
            char *r = strrchr(f, '\n');
            if (r) *r = '\0';
            parse(ex, f);
            f = fgets(buf, 65535, stdin);
        }
    } else {
        for (int i = 1; i < argc; ++i) {
            printf(" = %d =\n", i);
            parse(ex, argv[i]);
        }
    }

    em_unload_extractors(ex);
    return 0;
}
