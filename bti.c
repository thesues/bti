/*
 * forked from Greg Kroah-Hartman's bti project
 * see https://github.com/thesues/bti
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

#define VERSION "test"

/**
 * bidirectional popen() call
 *
 * @param rwepipe - int array of size three
 * @param exe - program to run
 * @param argv - argument list
 * @return pid or -1 on error
 *
 * The caller passes in an array of three integers (rwepipe), on successful
 * execution it can then write to element 0 (stdin of exe), and read from
 * element 1 (stdout) and 2 (stderr).
 */
static int popenRWE(int *rwepipe, const char *exe, const char *const argv[])
{
	int in[2];
	int out[2];
	int err[2];
	int pid;
	int rc;

	rc = pipe(in);
	if (rc < 0)
		goto error_in;

	rc = pipe(out);
	if (rc < 0)
		goto error_out;

	rc = pipe(err);
	if (rc < 0)
		goto error_err;

	pid = fork();
	if (pid > 0) {
		/* parent */
		close(in[0]);
		close(out[1]);
		close(err[1]);
		rwepipe[0] = in[1];
		rwepipe[1] = out[0];
		rwepipe[2] = err[0];
		return pid;
	} else if (pid == 0) {
		/* child */
		close(in[1]);
		close(out[0]);
		close(err[0]);
		close(0);
		rc = dup(in[0]);
		close(1);
		rc = dup(out[1]);
		close(2);
		rc = dup(err[1]);

		execvp(exe, (char **)argv);
		exit(1);
	} else
		goto error_fork;

	return pid;

 error_fork:
	close(err[0]);
	close(err[1]);
 error_err:
	close(out[0]);
	close(out[1]);
 error_out:
	close(in[0]);
	close(in[1]);
 error_in:
	return -1;
}

static int pcloseRWE(int pid, int *rwepipe)
{
	int rc, status;
	close(rwepipe[0]);
	close(rwepipe[1]);
	close(rwepipe[2]);
	rc = waitpid(pid, &status, 0);
	return status;
}

static char *get_string_from_stdin(void)
{
	char *temp;
	char *string;

	string = zalloc(1000);
	if (!string)
		return NULL;

	if (!fgets(string, 999, stdin))
		return NULL;
	temp = strchr(string, '\n');
	if (temp)
		*temp = '\0';
	return string;
}

static char *shrink_one_url(int *rwepipe, char *big)
{
	int biglen = strlen(big);
	char *small;
	int smalllen;
	int rc;

	rc = dprintf(rwepipe[0], "%s\n", big);
	if (rc < 0)
		return big;

	smalllen = biglen + 128;
	small = malloc(smalllen);
	if (!small)
		return big;

	rc = read(rwepipe[1], small, smalllen);
	if (rc < 0 || rc > biglen)
		goto error_free_small;

	if (strncmp(small, "http://", 7))
		goto error_free_small;

	smalllen = rc;
	while (smalllen && isspace(small[smalllen - 1]))
		small[--smalllen] = 0;

	free(big);
	return small;

 error_free_small:
	free(small);
	return big;
}

static int find_urls(const char *tweet, int **pranges)
{
	/*
	 * magic obtained from
	 * http://www.geekpedia.com/KB65_How-to-validate-an-URL-using-RegEx-in-Csharp.html
	 */
	static const char *re_magic =
	    "(([a-zA-Z][0-9a-zA-Z+\\-\\.]*:)/{1,3}"
	    "[0-9a-zA-Z;/~?:@&=+$\\.\\-_'()%]+)"
	    "(#[0-9a-zA-Z;/?:@&=+$\\.\\-_!~*'()%]+)?";
	pcre *re;
	const char *errptr;
	int erroffset;
	int ovector[10] = { 0, };
	const size_t ovsize = sizeof(ovector) / sizeof(*ovector);
	int startoffset, tweetlen;
	int i, rc;
	int rbound = 10;
	int rcount = 0;
	int *ranges = malloc(sizeof(int) * rbound);

	re = pcre_compile(re_magic,
			  PCRE_NO_AUTO_CAPTURE, &errptr, &erroffset, NULL);
	if (!re) {
		fprintf(stderr, "pcre_compile @%u: %s\n", erroffset, errptr);
		exit(1);
	}

	tweetlen = strlen(tweet);
	for (startoffset = 0; startoffset < tweetlen;) {

		rc = pcre_exec(re, NULL, tweet, strlen(tweet), startoffset, 0,
			       ovector, ovsize);
		if (rc == PCRE_ERROR_NOMATCH)
			break;

		if (rc < 0) {
			fprintf(stderr, "pcre_exec @%u: %s\n",
				erroffset, errptr);
			exit(1);
		}

		for (i = 0; i < rc; i += 2) {
			if ((rcount + 2) == rbound) {
				rbound *= 2;
				ranges = realloc(ranges, sizeof(int) * rbound);
			}

			ranges[rcount++] = ovector[i];
			ranges[rcount++] = ovector[i + 1];
		}

		startoffset = ovector[1];
	}

	pcre_free(re);

	*pranges = ranges;
	return rcount;
}

static char *shrink_urls(char *text)
{
	int *ranges;
	int rcount;
	int i;
	int inofs = 0;
	int outofs = 0;
	const char *const shrink_args[] = {
		"bti-shrink-urls",
		NULL
	};
	int shrink_pid;
	int shrink_pipe[3];
	int inlen = strlen(text);

	dbg("before len=%u\n", inlen);

	shrink_pid = popenRWE(shrink_pipe, shrink_args[0], shrink_args);
	if (shrink_pid < 0)
		return text;

	rcount = find_urls(text, &ranges);
	if (!rcount)
		return text;

	for (i = 0; i < rcount; i += 2) {
		int url_start = ranges[i];
		int url_end = ranges[i + 1];
		int long_url_len = url_end - url_start;
		char *url = strndup(text + url_start, long_url_len);
		int short_url_len;
		int not_url_len = url_start - inofs;

		dbg("long  url[%u]: %s\n", long_url_len, url);
		url = shrink_one_url(shrink_pipe, url);
		short_url_len = url ? strlen(url) : 0;
		dbg("short url[%u]: %s\n", short_url_len, url);

		if (!url || short_url_len >= long_url_len) {
			/* The short url ended up being too long
			 * or unavailable */
			if (inofs) {
				strncpy(text + outofs, text + inofs,
					not_url_len + long_url_len);
			}
			inofs += not_url_len + long_url_len;
			outofs += not_url_len + long_url_len;

		} else {
			/* copy the unmodified block */
			strncpy(text + outofs, text + inofs, not_url_len);
			inofs += not_url_len;
			outofs += not_url_len;

			/* copy the new url */
			strncpy(text + outofs, url, short_url_len);
			inofs += long_url_len;
			outofs += short_url_len;
		}

		free(url);
	}

	/* copy the last block after the last match */
	if (inofs) {
		int tail = inlen - inofs;
		if (tail) {
			strncpy(text + outofs, text + inofs, tail);
			outofs += tail;
		}
	}

	free(ranges);

	(void)pcloseRWE(shrink_pid, shrink_pipe);

	text[outofs] = 0;
	dbg("after len=%u\n", outofs);
	return text;
}

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
		"  --dry-run\n" "  --version\n" "  --help\n", VERSION);
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
	int (*bind_key) (int, void *);
	void (*insert) (void);

	/* default to internal function if we can't or won't find anything */
	session->readline = get_string;
	if (!isatty(0))
		return;
	session->interactive = 1;

	tmp = malloc(strlen(libpath) + 1);
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

static const char config_default[] = "/etc/bti";
static const char config_user_default[] = ".bti";

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
		{"debug", 0, NULL, 'd'},
		{"verbose", 0, NULL, 'V'},
		{"action", 1, NULL, 'A'},
		{"logfile", 1, NULL, 'L'},
		{"shrink-urls", 0, NULL, 's'},
		{"help", 0, NULL, 'h'},
		{"bash", 0, NULL, 'b'},
		{"background", 0, NULL, 'B'},
		{"dry-run", 0, NULL, 'n'},
		{"page", 1, NULL, 'g'},
		{"version", 0, NULL, 'v'},
		{"config", 1, NULL, 'c'},
		{"replyto", 1, NULL, 'r'},
		{"retweet", 1, NULL, 'w'},
		{}
	};
	struct session *session;
	pid_t child;
	int retval = 0;
	int option;
	char *home;
	const char *config_file;
	time_t t;
	int page_nr;
	char *tweet;

	debug = 0;

	session = session_alloc();
	if (!session) {
		fprintf(stderr, "no more memory...\n");
		return -1;
	}

	/* get the current time so that we can log it later */
	time(&t);
	session->time = strdup(ctime(&t));
	session->time[strlen(session->time) - 1] = 0x00;

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
	session->configfile =
	    zalloc(strlen(session->homedir) + strlen(config_file) + 7);
	sprintf(session->configfile, "%s/%s", session->homedir, config_file);

	session_readline_init(session);

	struct account *account = parse_configfile(session);
	if (account == NULL) {
		fprintf(stderr, "parse err, goto exit\n");
		exit(-1);
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
	switch (session->action) {
	case ACTION_PUBLIC:
		PUBLIC(account, session, retval);
		break;
	case ACTION_UPDATE:
		if (session->background || !session->interactive)
			tweet = get_string_from_stdin();
		else
			tweet = session->readline("tweet: ");
		if (!tweet || strlen(tweet) == 0) {
			dbg("no tweet?\n");
			return -1;
		}

		if (session->shrink_urls)
			tweet = shrink_urls(tweet);
		session->tweet = zalloc(strlen(tweet) + 10);
		if (session->bash)
			sprintf(session->tweet, "%c %s",
				getuid()? '$' : '#', tweet);
		else
			sprintf(session->tweet, "%s", tweet);
		if (tweet)
			free(tweet);
		dbg("tweet = %s\n", session->tweet);
		UPDATE(account, session, retval);
		break;
	case ACTION_FRIENDS:
		FRIENDS(account, session, retval);
		break;
	case ACTION_REPLIES:
		REPLIES(account, session, retval);
		break;
	default:
		retval = -1;
		break;
	}

	//      retval = send_request(session);

	if (retval && !session->background)
		fprintf(stderr, "operation failed\n");

	/* log_session(session, retval); */
	DESTORY(account);
 exit:
	session_readline_cleanup(session);
	session_free(session);
	return retval;;
}
