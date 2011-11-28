/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
#include <iniparser.h>
#include <stdio.h>
#include <string.h>
#include "bti.h"
#include "my_oauth.h"

enum {
	LOGFILE = 0,
	VERBOSE,
	SHRINK_URLS,
	NUMOFCONFIG
};

char *global_configs[] = {
	"logfile",
	"verbose",
	"shrink_urls"
};

struct account *parse_configfile(struct session *session)
{

	dictionary *ini;
	int item;
	char *s, *s1, *s2;
	int bool;
	if ((ini = iniparser_load(session->configfile)) == NULL) {
		fprintf(stderr, "cannot parse file\n");
		return NULL;
	}
	struct account *account = NULL;

#ifdef DEBUG
	iniparser_dump(ini, stderr);
#endif

	for (item = 0; item < NUMOFCONFIG; item++) {
		switch (item) {
		case LOGFILE:
			s = iniparser_getstring(ini, global_configs[item],
						NULL);
			if (s != NULL)
				//              session->log_file=strdup(s);
				;
			break;
		case VERBOSE:
			bool =
			    iniparser_getboolean(ini, global_configs[item], -1);
			if (bool != -1 && bool)
				session->verbose = 1;
			break;
		case SHRINK_URLS:
			bool =
			    iniparser_getboolean(ini, global_configs[item], -1);
			if (s != NULL)
				session->shrink_urls = 1;
			break;
		}
	}

	/* for twitter account */
	
	/* s1 = iniparser_getstring(ini, "twitter:consumer_key", NULL); */
	/* s2 = iniparser_getstring(ini, "twitter:consumer_secret", NULL); */
	/* if (s1 != NULL && s2 != NULL) { */
	/* 	account = twitter_init(s1, s2); */
	/* 	struct oauth_data * data = (struct oauth_data *)account->data; */
	/* 	/\*check for token key *\/ */
	/* 	s1 = iniparser_getstring(ini, "twitter:access_token_key", NULL); */
	/* 	s2 = iniparser_getstring(ini, "twitter:access_token_secret", NULL); */
	/* 	if (s1 && s2) { */
	/* 		dbg("yes!,we have access_token_key"); */
	/* 		//add final token and token_secret to data */
	/* 		data->access_token_key = strdup(s1); */
	/* 		data->access_token_secret = strdup(s2); */
	/* 	} else { */
	/* 		dbg("no access_token_key now!"); */
	/* 		char * token; */
	/* 		char * secret; */
	/* 		oauth_access_token(account, session); */
	/* 		dbg("token:%s,\n secret:%s\n",data->access_token_key, data->access_token_secret); */
			
	/* 	} */
	/* } */

	//sina account;
	s1 = iniparser_getstring(ini, "sina:consumer_key", NULL);
	s2 = iniparser_getstring(ini, "sina:consumer_secret", NULL);
	if (s1 != NULL && s2 != NULL) {
		account = sina_init(s1, s2);
		struct oauth_data * data = (struct oauth_data *)account->data;
		/*check for token key */
		s1 = iniparser_getstring(ini, "sina:access_token_key", NULL);
		s2 = iniparser_getstring(ini, "sina:access_token_secret", NULL);
		if (s1 && s2) {
			dbg("yes!,we have access_token_key");
			//add final token and token_secret to data
			data->access_token_key = strdup(s1);
			data->access_token_secret = strdup(s2);
		} else {
			dbg("no access_token_key now!");
			if(oauth_access_token(account, session)==1)
			{
				dbg("%s access token failed!", "twitter");
			}
			else
			{
				dbg("token:%s,\n secret:%s\n",data->access_token_key, data->access_token_secret);
				iniparser_setstr(ini, "sina:access_token_key", data->access_token_key);
				iniparser_setstr(ini, "sina:access_token_secret", data->access_token_secret);
				FILE * fp=fopen(session->configfile,"w");
				iniparser_dump_ini(ini, fp);
				fclose(fp);
			}
			
		}
	}


	iniparser_freedict(ini);

	return account;

}
