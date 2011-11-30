/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
/*
 * forked from Greg Kroah-Hartman's bti project
 * see https://github.com/thesues/bti
 */
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

struct host_map {
	enum HOST type;
	char *name;
};

struct host_map hosts[NUMOFHOSTS] = {
	{TWITTER, "twitter"},
	{SINA, "sina"},
	{DOUBAN, "douban"},
};

extern struct account * twitter_init(const char *, const char *);
extern struct account * sina_init(const char*, const char *);
	
static struct account *readConfig(enum HOST m_host, dictionary * ini,
				  struct session *session);

struct account *parse_configfile(struct session *session)
{

	dictionary *ini;
	int item;
	char *s;
	int bool;
	if ((ini = iniparser_load(session->configfile)) == NULL) {
		fprintf(stderr, "cannot parse file\n");
		return NULL;
	}
	struct account *account = NULL;
	struct account *head = NULL;

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

	if ((account = readConfig(TWITTER, ini, session)) != NULL) {
		account->next = NULL;
		if (head)
			head->next = account;
		else
			head = account;
	}

	if ((account = readConfig(SINA, ini, session)) != NULL) {
		account->next = NULL;
		if (head)
			head->next = account;
		else
			head = account;
	}

	iniparser_freedict(ini);

	return head;
}

struct account *readConfig(enum HOST m_host, dictionary * ini,
			   struct session *session)
{
	char *s1, *s2;
	char consumer_key_name[30];
	char consumer_secret_name[30];
	char access_token_key_name[30];
	char access_token_secret_name[30];
	char *host = NULL;
	struct account *account = NULL;
	dbg("try to read %s config\n", hosts[m_host].name);

	sprintf(consumer_key_name, "%s:consumer_key", hosts[m_host].name);
	sprintf(consumer_secret_name, "%s:consumer_secret", hosts[m_host].name);
	sprintf(access_token_key_name, "%s:access_token_key",
		hosts[m_host].name);
	sprintf(access_token_secret_name, "%s:access_token_secret",
		hosts[m_host].name);
	free(host);

	s1 = iniparser_getstring(ini, consumer_key_name, NULL);
	s2 = iniparser_getstring(ini, consumer_secret_name, NULL);

	if (s1 != NULL && s2 != NULL) {
		switch (m_host) {
		case TWITTER:
			account = twitter_init(s1, s2);
			break;
		case SINA:
			account = sina_init(s1, s2);
			break;
		case DOUBAN:
			account = NULL;
			break;
		}

		struct oauth_data *data = (struct oauth_data *)account->data;
		/*check for token key */
		s1 = iniparser_getstring(ini, access_token_key_name, NULL);
		s2 = iniparser_getstring(ini, access_token_secret_name, NULL);
		if (s1 && s2) {
			dbg("yes!,we have access_token_key");
			//add final token and token_secret to data
			data->access_token_key = strdup(s1);
			data->access_token_secret = strdup(s2);
		} else {
			dbg("no access_token_key now!");
			if (oauth_access_token(account, session) == 1) {
				fprintf(stderr, "%s access token failed!",
					hosts[m_host].name);
				account->opts->destory(account);
				return NULL;
			} else {
				dbg("token:%s,\n secret:%s\n",
				    data->access_token_key,
				    data->access_token_secret);
				iniparser_setstr(ini, access_token_key_name,
						 data->access_token_key);
				iniparser_setstr(ini, access_token_secret_name,
						 data->access_token_secret);
				FILE *fp = fopen(session->configfile, "w");
				iniparser_dump_ini(ini, fp);
				fclose(fp);
			}

		}
	}

	return account;
}
