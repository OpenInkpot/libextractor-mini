/*
 * MadShelf - bookshelf application.
 *
 * Copyright (C) 2009 Mikhail Gusarov <dottedmag@dottedmag.net>
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

#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "extractor-mini.h"

#define EXTRACTORS_DIR LIBDIR"/extractor-mini"

/*
 * Returns full path, not need to be freeed.
 */
const char* get_extractors_dir()
{
    return getenv("EXTRACTORS_DIR")
        ? getenv("EXTRACTORS_DIR")
        : EXTRACTORS_DIR;
}

/*
 * Returns full path, to be free(3)ed later.
 */
static char* get_full_path(const char* extractor_file)
{
    char* full_path;
    if(!asprintf(&full_path, "%s/%s", get_extractors_dir(), extractor_file))
        return NULL;
    return full_path;
}

static int filter_files(const struct dirent* d)
{
    unsigned short int len = _D_EXACT_NAMLEN(d);
    return (len > 2) && !strcmp(d->d_name + len - 3, ".so");
}

/* FIXME: name is modified inside*/
em_extractors_t* em_load_extractor(em_extractors_t* head, char* name)
{
    em_extractors_t* nhead;
    void* libhandle;
    void* extract_handle;
    char* extract_name;
    char* libname = get_full_path(name);
    if(!libname)
    {
        fprintf(stderr, "Out of memory while loading extractor %s",
                name);
        exit(1);
    }

    libhandle = dlopen(libname, RTLD_NOW);
    if(!libhandle)
    {
        fprintf(stderr, "Unable to load %s: %s\n", libname, dlerror());
        free(libname);
        return head;
    }

    free(libname);

    /* Remove '.so' from filename */
    name[strlen(name)-3] = 0;
    /* libextractor_foo_extract */
    if(!asprintf(&extract_name, "%s_extract", name))
        return NULL;
    if(!extract_name)
    {
        fprintf(stderr, "Out of memory while loading extractor %s",
                name);
        exit(1);
    }

    extract_handle = dlsym(libhandle, extract_name);
    if(!extract_handle)
    {
        fprintf(stderr, "Unable to get entry point in %s: %s\n",
                name, dlerror());
        free(extract_name);
        dlclose(libhandle);
        return head;
    }

    free(extract_name);

    nhead = malloc(sizeof(em_extractors_t));
    if(!nhead)
    {
        fprintf(stderr, "Out of memory while loading extractor %s",
                name);
        exit(1);
    }

    nhead->handle = libhandle;
    nhead->method = (em_extract_method)extract_handle;
    nhead->next = head;

    return nhead;
}

em_extractors_t* em_load_extractors()
{
    struct dirent** files;
    int nfiles = scandir(get_extractors_dir(),
                         &files, &filter_files, &versionsort);
    int i;

    if(nfiles == -1)
    {
        fprintf(stderr, "Warning: unable to load extractors from %s: %s\n",
                get_extractors_dir(), strerror(errno));
        return NULL;
    }

    em_extractors_t* head = NULL;

    for(i = 0; i != nfiles; ++i)
    {
        head = em_load_extractor(head, files[i]->d_name);
        free(files[i]);
    }

    free(files);

    if(!head)
    {
        fprintf(stderr, "Warning: no extractors found in %s\n",
                get_extractors_dir());
    }

    return head;
}

void em_unload_extractors(em_extractors_t* extractors)
{
    while(extractors)
    {
        em_extractors_t* c = extractors;
        dlclose(c->handle);
        extractors = c->next;
        free(c);
    }
}

em_keyword_list_t* em_extractor_get_keywords(em_extractors_t* extractors,
                                           const char* filename)
{
    em_keyword_list_t* list = NULL;
    int fd = open(filename, O_RDONLY);
    struct stat s;
    void* m = NULL;
    if(fd == -1)
    {
        fprintf(stderr, "Unable to open %s for obtaining keywords: %s",
                filename, strerror(errno));
        goto err1;
    }

    if(-1 == fstat(fd, &s))
    {
        fprintf(stderr, "Unable to stat %s for obtaining keywords: %s",
                filename, strerror(errno));
        goto err2;
    }

    if(s.st_size)
    {
        m = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE | MAP_NONBLOCK, fd, 0);
        if(m == (void*)-1)
        {
            fprintf(stderr, "Unable to mmap %s for obtaining keywords: %s",
                    filename, strerror(errno));
            goto err2;
        }
    }

    while(extractors)
    {
        list = (*extractors->method)(filename, m, s.st_size, list);
        extractors = extractors->next;
    }

    if(m)
        munmap(m, s.st_size);
err2:
    close(fd);
err1:
    return list;
}

const char*
em_keywords_get_last(const em_keyword_type_t type,
                     const em_keyword_list_t* keywords)
{
    const char* result = NULL;
    while(keywords)
    {
        if(keywords->keyword_type == type)
            result = keywords->keyword;
        keywords = keywords->next;
    }
    return result;
}

const char*
em_keywords_get_first(const em_keyword_type_t type,
                      const em_keyword_list_t* keywords)
{
    while(keywords)
    {
        if(keywords->keyword_type == type)
            return keywords->keyword;
        keywords = keywords->next;
    }
    return NULL;
}

void em_keywords_free(em_keyword_list_t* keywords)
{
    while(keywords) {
        free(keywords->keyword);
        em_keyword_list_t* old = keywords;
        keywords = keywords->next;
        free(old);
    };
}

em_keyword_list_t*
em_keywords_add(em_keyword_list_t* next,
                em_keyword_type_t  type,
                const char* keyword)
{
    em_keyword_list_t* c = malloc(sizeof(em_keyword_list_t));
    c->keyword = strdup(keyword); /* FIXME: check strdup */
    c->keyword_type = type;
    c->next = next;
    return c;
}
