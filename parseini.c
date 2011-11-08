#include <iniparser.h>
#include "bti.h"
#include <stdio.h>
#include <string.h>

parse_configfile(struct session * session){
      if((ini=iniparser_load("file.ini"))==NULL){
      fprintf(stderr,"cannot parse file\n");
      return -1;
    }
    iniparser_dump(ini, stderr);
    s=iniparser_getstring(ini,"twitter:consumer_key",NULL);
    if(s!=NULL){
      printf("%s",s);
      free(s);
    }
    s=iniparser_getstring(ini,"sina:access_token_key",NULL);
    if(s!=NULL){
      printf(s);
    }
    iniparser_freedict(ini);
    
    return status ;
}
