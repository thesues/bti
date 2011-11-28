/*
 * Copyright (C) 2008-2011 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2009 Bart Trojanowski <bart@jukie.net>
 * Copyright (C) 2009-2010 Amir Mohammad Saied <amirsaied@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <pcre.h>
#include <termios.h>
#include <dlfcn.h>
#include <oauth.h>
#include "bti.h"
#include "account.h"
#include "twitter.h"



#define VERSION "test"



int debug;

static void display_help(void)
{
	fprintf(stdout, "bti - send tweet to twitter or identi.ca\n"
		"Version: %s\n"
		"Usage:\n"
		"  bti [options]\n"
		"options are:\n"
		"  --action action\n"
		"    ('update', 'friends', 'public', 'replies', or 'user')\n"
		"  --host HOST\n"
		"  --logfile logfile\n"
		"  --config configfile\n"
		"  --replyto ID\n"
		"  --retweet ID\n"
		"  --shrink-urls\n"
		"  --page PAGENUMBER\n"
		"  --bash\n"
		"  --background\n"
		"  --debug\n"
		"  --verbose\n"
		"  --dry-run\n"
		"  --version\n"
		"  --help\n", VERSION);
}

static void display_version(void)
{
	fprintf(stdout, "bti - version %s\n", VERSION);
}

static char *get_string(const char *name)
{
	char *temp;
	char *string;

	string = zalloc(1000);
	if (!string)
		exit(1);
	if (name != NULL)
		fprintf(stdout, "%s", name);
	if (!fgets(string, 999, stdin))
		return NULL;
	temp = strchr(string, '\n');
	if (temp)
		*temp = '\0';
	return string;
}

/*
 * Try to get a handle to a readline function from a variety of different
 * libraries.  If nothing is present on the system, then fall back to an
 * internal one.
 *
 * Logic originally based off of code in the e2fsutils package in the
 * lib/ss/get_readline.c file, which is licensed under the MIT license.
 *
 * This keeps us from having to relicense the bti codebase if readline
 * ever changes its license, as there is no link-time dependency.
 * It is a run-time thing only, and we handle any readline-like library
 * in the same manner, making bti not be a derivative work of any
 * other program.
 */
static void session_readline_init(struct session *session)
{
	/* Libraries we will try to use for readline/editline functionality */
	const char *libpath = "libreadline.so.6:libreadline.so.5:"
				"libreadline.so.4:libreadline.so:libedit.so.2:"
				"libedit.so:libeditline.so.0:libeditline.so";
	void *handle = NULL;
	char *tmp, *cp, *next;
	int (*bind_key)(int, void *);
	void (*insert)(void);

	/* default to internal function if we can't or won't find anything */
	session->readline = get_string;
	if (!isatty(0))
		return;
	session->interactive = 1;

	tmp = malloc(strlen(libpath)+1);
	if (!tmp)
		return;
	strcpy(tmp, libpath);
	for (cp = tmp; cp; cp = next) {
		next = strchr(cp, ':');
		if (next)
			*next++ = 0;
		if (*cp == 0)
			continue;
		handle = dlopen(cp, RTLD_NOW);
		if (handle) {
			dbg("Using %s for readline library\n", cp);
			break;
		}
	}
	free(tmp);
	if (!handle) {
		dbg("No readline library found.\n");
		return;
	}

	session->readline_handle = handle;
	session->readline = (char *(*)(const char *))dlsym(handle, "readline");
	if (session->readline == NULL) {
		/* something odd happened, default back to internal stuff */
		session->readline_handle = NULL;
		session->readline = get_string;
		return;
	}

	/*
	 * If we found a library, turn off filename expansion
	 * as that makes no sense from within bti.
	 */
	bind_key = (int (*)(int, void *))dlsym(handle, "rl_bind_key");
	insert = (void (*)(void))dlsym(handle, "rl_insert");
	if (bind_key && insert)
		bind_key('\t', insert);
}

static void session_readline_cleanup(struct session *session)
{
	if (session->readline_handle)
		dlclose(session->readline_handle);
}

static struct session *session_alloc(void)
{
	struct session *session;

	session = zalloc(sizeof(*session));
	if (!session)
		return NULL;
	return session;
}

static void session_free(struct session *session)
{
	if (!session)
		return;
	free(session->retweet);
	free(session->replyto);
	free(session->tweet);
	free(session->time);
	free(session->homedir);
	free(session->user);
	free(session->group);
	free(session->configfile);
	free(session);
}




static const char config_default[]	= "/etc/bti";
static const char config_user_default[]	= ".bti";






/* static void log_session(struct session *session, int retval) */
/* { */
/* 	FILE *log_file; */
/* 	char *filename; */

/* 	/\* Only log something if we have a log file set *\/ */
/* 	if (!session->logfile) */
/* 		return; */

/* 	filename = alloca(strlen(session->homedir) + */
/* 			  strlen(session->logfile) + 3); */

/* 	sprintf(filename, "%s/%s", session->homedir, session->logfile); */

/* 	log_file = fopen(filename, "a+"); */
/* 	if (log_file == NULL) */
/* 		return; */

/* 	switch (session->action) { */
/* 	case ACTION_UPDATE: */
/* 		if (retval) */
/* 			fprintf(log_file, "%s: host=%s tweet failed\n", */
/* 				session->time, session->hostname); */
/* 		else */
/* 			fprintf(log_file, "%s: host=%s tweet=%s\n", */
/* 				session->time, session->hostname, */
/* 				session->tweet); */
/* 		break; */
/* 	case ACTION_FRIENDS: */
/* 		fprintf(log_file, "%s: host=%s retrieving friends timeline\n", */
/* 			session->time, session->hostname); */
/* 		break; */
/* 	case ACTION_USER: */
/* 		fprintf(log_file, "%s: host=%s retrieving %s's timeline\n", */
/* 			session->time, session->hostname, session->user); */
/* 		break; */
/* 	case ACTION_REPLIES: */
/* 		fprintf(log_file, "%s: host=%s retrieving replies\n", */
/* 			session->time, session->hostname); */
/* 		break; */
/* 	case ACTION_PUBLIC: */
/* 		fprintf(log_file, "%s: host=%s retrieving public timeline\n", */
/* 			session->time, session->hostname); */
/* 		break; */
/* 	default: */
/* 		break; */
/* 	} */

/* 	fclose(log_file); */
/* } */



int main(int argc, char *argv[], char *envp[])
{
  //FIXME:user could choose only a website no all website.which means --host is available.
  
	static const struct option options[] = {
		{ "debug", 0, NULL, 'd' },
		{ "verbose", 0, NULL, 'V' },
		{ "action", 1, NULL, 'A' },
		{ "logfile", 1, NULL, 'L' },
		{ "shrink-urls", 0, NULL, 's' },
		{ "help", 0, NULL, 'h' },
		{ "bash", 0, NULL, 'b' },
		{ "background", 0, NULL, 'B' },
		{ "dry-run", 0, NULL, 'n' },
		{ "page", 1, NULL, 'g' },
		{ "version", 0, NULL, 'v' },
		{ "config", 1, NULL, 'c' },
		{ "replyto", 1, NULL, 'r' },
		{ "retweet", 1, NULL, 'w' },
		{ }
	};
	struct session *session;
	pid_t child;
	int retval = 0;
	int option;
	char *home;
	const char *config_file;
	time_t t;
	int page_nr;
	
	debug = 0;

	session = session_alloc();
	if (!session) {
		fprintf(stderr, "no more memory...\n");
		return -1;
	}

	/* get the current time so that we can log it later */
	time(&t);
	session->time = strdup(ctime(&t));
	session->time[strlen(session->time)-1] = 0x00;

	/*
	 * Get the home directory so we can try to find a config file.
	 * If we have no home dir set up, look in /etc/bti
	 */
	home = getenv("HOME");
	if (home) {
		/* We have a home dir, so this might be a user */
		session->homedir = strdup(home);
		config_file = config_user_default;
	} else {
		session->homedir = strdup("");
		config_file = config_default;
	}

	/* set up a default config file location (traditionally ~/.bti) */
	session->configfile = zalloc(strlen(session->homedir) + strlen(config_file) + 7);
	sprintf(session->configfile, "%s/%s", session->homedir, config_file);

	session_readline_init(session);
	
	struct account * account = parse_configfile(session);
	if (account == NULL) {
	  dbg("parse err,goto exit");
	}

	while (1) {
		option = getopt_long_only(argc, argv,
					  "dp:P:H:a:A:u:c:hg:G:sr:nVvw:",
					  options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'd':
			debug = 1;
			break;
		case 'V':
			session->verbose = 1;
			break;
		case 'g':
			page_nr = atoi(optarg);
			dbg("page = %d\n", page_nr);
			session->page = page_nr;
			break;
		case 'r':
			session->replyto = strdup(optarg);
			dbg("in_reply_to_status_id = %s\n", session->replyto);
			break;
		case 'A':
			if (strcasecmp(optarg, "update") == 0)
				session->action = ACTION_UPDATE;
			else if (strcasecmp(optarg, "friends") == 0)
				session->action = ACTION_FRIENDS;
			else if (strcasecmp(optarg, "user") == 0)
				session->action = ACTION_USER;
			else if (strcasecmp(optarg, "replies") == 0)
				session->action = ACTION_REPLIES;
			else if (strcasecmp(optarg, "public") == 0)
				session->action = ACTION_PUBLIC;
			else if (strcasecmp(optarg, "group") == 0)
				session->action = ACTION_GROUP;
			else if (strcasecmp(optarg, "retweet") == 0)
				session->action = ACTION_RETWEET;
			else
				session->action = ACTION_UNKNOWN;
			dbg("action = %d\n", session->action);
			break;
		case 'u':
			if (session->user)
				free(session->user);
			session->user = strdup(optarg);
			dbg("user = %s\n", session->user);
			break;

		case 'G':
			if (session->group)
				free(session->group);
			session->group = strdup(optarg);
			dbg("group = %s\n", session->group);
			break;
		case 'L':
			if (session->logfile)
				free(session->logfile);
			session->logfile = strdup(optarg);
			dbg("logfile = %s\n", session->logfile);
			break;
		case 's':
			session->shrink_urls = 1;
			break;
		case 'b':
			session->bash = 1;
			/* fall-through intended */
		case 'B':
			session->background = 1;
			break;
		case 'c':
			if (session->configfile)
				free(session->configfile);
			session->configfile = strdup(optarg);
			dbg("configfile = %s\n", session->configfile);

			/*
			 * read the config file now.  Yes, this could override
			 * previously set options from the command line, but
			 * the user asked for it...
			 */
			//bti_parse_configfile(session);
			break;
		case 'h':
			display_help();
			goto exit;
		case 'n':
			session->dry_run = 1;
			break;
		case 'v':
			display_version();
			goto exit;
		default:
			display_help();
			goto exit;
		}
	}


	/*
	 * Show the version to make it easier to determine what
	 * is going on here
	 */
	if (debug)
		display_version();




	if (session->action == ACTION_UNKNOWN) {
		fprintf(stderr, "Unknown action, valid actions are:\n"
			"'update', 'friends', 'public', 'replies', 'group' or 'user'.\n");
		goto exit;
	}


	
	dbg("config file = %s\n", session->configfile);
	dbg("action = %d\n", session->action);

	/* fork ourself so that the main shell can get on
	 * with it's life as we try to connect and handle everything
	 */
	if (session->background) {
	  child = fork();
	  if (child) {
	    dbg("child is %d\n", child);
	    exit(0);
	  }
	}
	switch(session->action) {
	case ACTION_PUBLIC:
	  retval = PUBLIC(account, session);
	  break;
	case ACTION_UPDATE:
	  retval = UPDATE(account, session);
	  break;
	case ACTION_FRIENDS:
	  retval = FRIENDS(account ,session);
	  break;
	default:
	  retval = -1;
	  break;
	}

	//	retval = send_request(session);
	
	if (retval && !session->background)
		fprintf(stderr, "operation failed\n");

	/* log_session(session, retval); */
exit:
	session_readline_cleanup(session);
	session_free(session);
	return retval;;
}
