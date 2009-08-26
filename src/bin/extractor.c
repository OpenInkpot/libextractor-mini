#include <stdio.h>
#include <stdlib.h>
#include "extractor-mini.h"

int
main(int argc, char *argv[])
{
    if(argc!=2)
    {
        printf("Single argument required\n");
        exit(1);
    }

    em_extractors_t* ex = em_load_extractors();
    em_keyword_list_t* list = em_extractor_get_keywords(ex, argv[1]);
    em_keyword_list_t* item = list;
    while(item)
    {
        printf("%d -> %s\n", item->keyword_type, item->keyword);
        item = item->next;
    }
    em_keywords_free(list);
    em_unload_extractors(ex);
    return 0;
}
