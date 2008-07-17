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
char *title=NULL;
char *authorfirst=NULL;
char *authormiddle=NULL;
char *authorlast=NULL;

void initvars()
{
    authorflag=0;
    titleflag=0;
    titleinfoflag=0;
    firstnameflag=0;
    middlenameflag=0;
    lastnameflag=0;
    doneflag=0;
    title=NULL;
    authorfirst=NULL;
    authormiddle=NULL;
    authorlast=NULL;   
}

void handlestart(void *userData,const XML_Char *name,const XML_Char **atts)
{
    if(strcmp(name,"title-info")==0)
        titleinfoflag=1;
    else if(strcmp(name,"book-title")==0 && titleinfoflag)
        titleflag=1;
    else if(strcmp(name,"author")==0 &&titleinfoflag)
        authorflag=1;
    else if(strcmp(name,"first-name")==0 && authorflag)
        firstnameflag=1;
    else if(strcmp(name,"middle-name")==0 && authorflag)
        middlenameflag=1;
    else if(strcmp(name,"last-name")==0 && authorflag)
        lastnameflag=1;
    else if(strcmp(name,"body")==0)
        doneflag=1;
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
    char *temp2=(char *)calloc(len+1,sizeof(char));
    
    strncpy(temp2,s,len);
    temp2[len]='\0';
    
    char *temp=NULL;
    if(titleflag==1&&len>0)
    {
        temp=title;
        title=(char *)calloc(len+(temp==NULL?0:strlen(temp))+1,sizeof(char));
        if(temp)
        {
            strncpy(title,temp,strlen(temp));
            strncpy(&title[strlen(temp)],s,len);
            title[len+strlen(temp)]='\0';
            free(temp);
        }
        else
        {
            strncpy(title,s,len);
            title[len]='\0';
        }
    }
    else if(firstnameflag&&len>0)
    {
        temp=authorfirst;
        authorfirst=(char *)calloc(len+(temp==NULL?0:strlen(temp))+1,sizeof(char));
        if(temp)
        {
            strncpy(authorfirst,temp,strlen(temp));
            strncpy(&authorfirst[strlen(temp)],s,len);
            authorfirst[len+strlen(temp)]='\0';
            free(temp);
        }
        else
        {
            strncpy(authorfirst,s,len);
            authorfirst[len]='\0';
        }
        
    }
    else if(middlenameflag &&len>0)
    {
        temp=authormiddle;
        authormiddle=(char *)calloc(len+(temp==NULL?0:strlen(temp))+1,sizeof(char));
        if(temp)
        {
            strncpy(authormiddle,temp,strlen(temp));
            strncpy(&authormiddle[strlen(temp)],s,len);
            authormiddle[len+strlen(temp)]='\0';
            free(temp);
        }
        else
        {
            strncpy(authormiddle,s,len);
            authormiddle[len]='\0';
        }
    }
    else if(lastnameflag&&len>0)
    {
        temp=authorlast;
        authorlast=(char *)calloc(len+(temp==NULL?0:strlen(temp))+1,sizeof(char));
        if(temp)
        {
            strncpy(authorlast,temp,strlen(temp));
            strncpy(&authorlast[strlen(temp)],s,len);
            authorlast[len+strlen(temp)]='\0';
            free(temp);
        }
        else
        {
            strncpy(authorlast,s,len);
            authorlast[len]='\0';
        }
    }
}

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
    if(title)
        prev = add_to_list(prev, EXTRACTOR_TITLE, title);
    
    if(authorfirst||authormiddle||authorlast)
    {
        char* author = calloc((authorfirst?strlen(authorfirst):0)
                              + (authormiddle?(strlen(authormiddle)+1):0)
                              + (authorlast?(strlen(authorlast)+1):0)+1,
                              sizeof(char));
        
        if(authorfirst)
        {
            strcat(author,authorfirst);
            free(authorfirst);
        }
        
        if(authormiddle)
        {
            strcat(author," ");
            strcat(author,authormiddle);
            free(authormiddle);
        }
        
        if(authorlast)
        {
            strcat(author," ");
            strcat(author,authorlast);
            free(authorlast);
        }

        prev = add_to_list(prev, EXTRACTOR_AUTHOR, author);
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

    if(XML_Parse(myparse, data, size, 1) == XML_STATUS_ERROR)
    {
        printf("%s\n", XML_ErrorString(XML_GetErrorCode(myparse)));
        return prev;
    }

    XML_ParserFree(myparse);

    return append_fb2_keywords(prev);
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

    if(!parse_zipped_fb2(myparse, filename))
    {
        XML_ParserFree(myparse);
        return prev;
    }

    XML_ParserFree(myparse);
    return append_fb2_keywords(prev);
}
