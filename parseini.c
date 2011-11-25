/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
#include <iniparser.h>
#include <stdio.h>
#include <string.h>
#include "bti.h"

enum {
  PROXY=0,
  LOGFILE,
  VERBOSE,
  SHRINK_URLS,
  NUMOFCONFIG
};

char * global_configs[]={
  "proxy",
  "logfile",
  "verbose",
  "shrink_urls"
};
  

struct account *  parse_configfile(struct session * session){

  dictionary * ini;
  int item;
  char * s,*s1,*s2;
  int bool;
  if((ini=iniparser_load(session->configfile))==NULL){
	fprintf(stderr,"cannot parse file\n");
      return;
  }
  struct account * account=NULL;
  
  #ifdef DEBUG
  iniparser_dump(ini, stderr);
  #endif
  
  for (item=0; item < NUMOFCONFIG; item++) {
      switch(item) {
	  case PROXY:
		  s = iniparser_getstring(ini,global_configs[item],NULL);
		  if (s != NULL)
			  session->proxy=strdup(s);
		  break;
	  case LOGFILE:
		  s = iniparser_getstring(ini,global_configs[item],NULL);
		  if (s != NULL)
			  //		  session->log_file=strdup(s);
			  ;
		  break;
	  case VERBOSE:
		  bool = iniparser_getboolean(ini,global_configs[item],-1);
		  if (bool!=-1 && bool)
			  session->verbose=1;
		  break;
	  case SHRINK_URLS:
		  bool = iniparser_getboolean(ini,global_configs[item],-1);
		  if (s != NULL)
			  session->shrink_urls=1;
		  break;
    }
  }
  
  
  /* for twitter account */
  s1 = iniparser_getstring(ini,"twitter:consumer_key", NULL);
  s2 = iniparser_getstring(ini,"twitter:consumer_secret", NULL);

		  
  if( s1 != NULL && s2 != NULL ){
    account = twitter_init(s1,s2);
    /*check for token key*/
    s1 = iniparser_getstring(ini,"twitter:access_token_key",NULL);
    s2 = iniparser_getstring(ini,"twitter:access_token_secret",NULL);
	
    if( s1 && s2){
		dbg("yes!,we have access_token_key");
    }else{
		dbg("no access_token_key now!");
		oauth_access_token(account,session);
    }
  }
  
  s1 = iniparser_getstring(ini,"sina:consumer_key",NULL);
  s2 = iniparser_getstring(ini,"sina:consumer_secret",NULL);
  
  iniparser_freedict(ini);
  
  return account;
    
}
