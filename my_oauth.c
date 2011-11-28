#include "account.h"
#include "my_oauth.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oauth.h>
#include <termios.h>
#include <pcre.h>

static void read_password(char *buf, size_t len, char *host)
{
	char pwd[80];
	int retval;
	struct termios old;
	struct termios tp;

	tcgetattr(0, &tp);
	old = tp;

	tp.c_lflag &= (~ECHO);
	tcsetattr(0, TCSANOW, &tp);

	fprintf(stdout, "Enter password for %s: ", host);
	fflush(stdout);
	tcflow(0, TCOOFF);
	retval = scanf("%79s", pwd);
	tcflow(0, TCOON);
	fprintf(stdout, "\n");

	tcsetattr(0, TCSANOW, &old);

	strncpy(buf, pwd, len);
	buf[len-1] = '\0';
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
	int ovector[10] = {0,};
	const size_t ovsize = sizeof(ovector)/sizeof(*ovector);
	int startoffset, tweetlen;
	int i, rc;
	int rbound = 10;
	int rcount = 0;
	int *ranges = malloc(sizeof(int) * rbound);

	re = pcre_compile(re_magic,
			PCRE_NO_AUTO_CAPTURE,
			&errptr, &erroffset, NULL);
	if (!re) {
		fprintf(stderr, "pcre_compile @%u: %s\n", erroffset, errptr);
		exit(1);
	}

	tweetlen = strlen(tweet);
	for (startoffset = 0; startoffset < tweetlen; ) {

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
			if ((rcount+2) == rbound) {
				rbound *= 2;
				ranges = realloc(ranges, sizeof(int) * rbound);
			}

			ranges[rcount++] = ovector[i];
			ranges[rcount++] = ovector[i+1];
		}

		startoffset = ovector[1];
	}

	pcre_free(re);

	*pranges = ranges;
	return rcount;
}

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
	while (smalllen && isspace(small[smalllen-1]))
			small[--smalllen] = 0;

	free(big);
	return small;

error_free_small:
	free(small);
	return big;
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
		int url_end = ranges[i+1];
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


static int parse_osp_reply(const char *reply, char **token, char **secret)
{
  int rc;
  int retval = 1;
  int i=1;
  char **rv = NULL;
  rc = oauth_split_url_parameters(reply, &rv);
  char ** p =rv;
  while(i<=rc)
  {
    if(!strncmp(*p, "oauth_token=",12))
    {
      if(token)
	*token = strdup(&(*p)[12]);
    }
    
    if(!strncmp(*p, "oauth_token_secret=",19))
    {
      if(secret)
	*secret = strdup(&(*p)[19]);
    }
    i++;
    p++;
  }

  if(*secret&&*token)
  {
    dbg("token: %s\n", *token);
    dbg("secret: %s\n", *secret);
    retval=0;
  }
  
  /* qsort(rv, rc, sizeof(char *), oauth_cmpstringp); */
  /* if (rc == 2 || rc == 4) { */
  /*   if (!strncmp(rv[0], "oauth_token=", 11) && */
  /* 	!strncmp(rv[1], "oauth_token_secret=", 18)) { */
  /*     if (token) */
  /* 	*token = strdup(&(rv[0][12])); */
  /*     if (secret) */
  /* 	*secret = strdup(&(rv[1][19])); */
      
  /*     retval = 0; */
  /*   } */
  /* } else if (rc == 3) { */
  /*   //FIXME:use loop to parse reply; */
  /*   if (!strncmp(rv[0], "oauth_token=", 11) && */
  /* 	!strncmp(rv[1], "oauth_token_secret=", 18)) { */
  /*     if (token) */
  /* 	*token = strdup(&(rv[0][12])); */
  /*     if (secret) */
  /* 	*secret = strdup(&(rv[1][19])); */
      
  /*     retval = 0; */
  /*   } */
  /* } */
  
  
  if (rv)
    free(rv);
  
  return retval;
}


int oauth_access_token(struct account * ac ,struct session * session){
  char *post_params = NULL;
  char *request_url = NULL;
  char *reply       = NULL;
  char *at_key      = NULL;
  char *at_secret   = NULL;
  char *verifier    = NULL;
  char at_uri[90];

  struct oauth_data * data = (struct oauth_data *)ac->data;

  request_url = oauth_sign_url2(data->request_token_uri,NULL,
			      OA_HMAC,NULL,
			      data->consumer_key,
			      data->consumer_secret,NULL,NULL);
  
  reply = oauth_http_get(request_url,post_params);
  if (request_url)
    free(post_params);
  if (!reply)
    return -1;
  if (parse_osp_reply(reply, &at_key,&at_secret))
    return -1;
  dbg("first round token:%s\n"
      "            secret:%s\n", at_key, at_secret);
  free(reply);
  fprintf(stdout,
	  "Please open the following link in your browser, and "
	  "allow 'bti' to access your account. Then paste "
	  "back the provided PIN in here.\n");
  fprintf(stdout,"%s%s\nPIN: ",
	  data->access_authrize_uri,at_key);
  verifier = session->readline(NULL);
  sprintf(at_uri, "%s?oauth_verifier=%s",
	  data->access_token_uri, verifier);
  
  request_url = oauth_sign_url2(at_uri, NULL, OA_HMAC, NULL,
				data->consumer_key,
				data->consumer_secret,
				at_key,
				at_secret);
  //FIXME:check at_key and at_secret first;
  reply = oauth_http_get(request_url, post_params);
  
  if (request_url)
    free(request_url);
  
  if (post_params)
    free(post_params);
  
  if (!reply)
    return -1;
  
  if (parse_osp_reply(reply, &at_key, &at_secret))
  {
    free(reply);
    return -1;
  }
  
  dbg("second round token:%s\n"
      "            secret:%s\n", at_key, at_secret);

  fprintf(stderr,
	  "Please put these two lines in your bti "
	  "configuration file (~/.bti):\n"
	  "[twitter]\n"
	  "access_token_key=%s\n"
	  "access_token_secret=%s\n",
	  at_key, at_secret);
  
  data->access_token_key = strdup(at_key);
  data->access_token_secret = strdup(at_secret);
  return 0;
}


static void bti_output_line(struct session *session, xmlChar *user,
			    xmlChar *id, xmlChar *created, xmlChar *text)
{
	if (session->verbose)
		printf("[%s] {%s} (%.16s) %s\n", user, id, created, text);
	else
		printf("[%s] %s\n", user, text);
}

static void parse_statuses(struct session *session,
			   xmlDocPtr doc, xmlNodePtr current)
{
  xmlChar *text = NULL;
  xmlChar *user = NULL;
  xmlChar *created = NULL;
  xmlChar *id = NULL;
  xmlNodePtr userinfo;

  current = current->xmlChildrenNode;
  while (current != NULL) {
    if (current->type == XML_ELEMENT_NODE) {
      if (!xmlStrcmp(current->name, (const xmlChar *)"created_at"))
	created = xmlNodeListGetString(doc, current->xmlChildrenNode, 1);
      if (!xmlStrcmp(current->name, (const xmlChar *)"text"))
	text = xmlNodeListGetString(doc, current->xmlChildrenNode, 1);
      if (!xmlStrcmp(current->name, (const xmlChar *)"id"))
	id = xmlNodeListGetString(doc, current->xmlChildrenNode, 1);
      if (!xmlStrcmp(current->name, (const xmlChar *)"user")) {
	userinfo = current->xmlChildrenNode;
	while (userinfo != NULL) {
	  if ((!xmlStrcmp(userinfo->name, (const xmlChar *)"screen_name"))) {
	    if (user)
	      xmlFree(user);
	    user = xmlNodeListGetString(doc, userinfo->xmlChildrenNode, 1);
	  }
	  userinfo = userinfo->next;
	}
      }

      if (user && text && created && id) {
	bti_output_line(session, user, id,
			created, text);
	xmlFree(user);
	xmlFree(text);
	xmlFree(created);
	xmlFree(id);
	user = NULL;
	text = NULL;
	created = NULL;
	id = NULL;
      }
    }
    current = current->next;
  }

  return;
}


void parse_timeline(char *document, struct session *session)
{
	xmlDocPtr doc;
	xmlNodePtr current;

	doc = xmlReadMemory(document, strlen(document), "timeline.xml",
			    NULL, XML_PARSE_NOERROR);
	if (doc == NULL)
		return;

	current = xmlDocGetRootElement(doc);
	if (current == NULL) {
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(doc);
		return;
	}

	if (xmlStrcmp(current->name, (const xmlChar *) "statuses")) {
		fprintf(stderr, "unexpected document type\n");
		xmlFreeDoc(doc);
		return;
	}

	current = current->xmlChildrenNode;
	while (current != NULL) {
		if ((!xmlStrcmp(current->name, (const xmlChar *)"status")))
			parse_statuses(session, doc, current);
		current = current->next;
	}
	xmlFreeDoc(doc);

	return;
}


int oauth_public(struct account * account ,struct session * session)
{
  struct oauth_data *data=(struct oauth_data *)account->data;
  char endpoint[500];
  char * req_url = NULL;
  char * reply   = NULL;
  sprintf(endpoint,"%s%s?source=%s", data->host_url, data->public_uri, data->consumer_key);
  
  if (!session->dry_run) {
    req_url = oauth_sign_url2(endpoint, NULL, OA_HMAC,
			      NULL, data->consumer_key,
			      data->consumer_secret,
			      data->access_token_key,
			      data->access_token_secret);
    reply = oauth_http_get(req_url, NULL);
    dbg("%s\n", req_url);
    dbg("%s\n", reply);
    if(req_url)
      free(req_url);
  }

  if(!reply){
    dbg("Error retrieving from URL(%s)\n",endpoint);
    return -1;
  }
  parse_timeline(reply, session);
  return 0;
}



int oauth_update(struct account * account ,struct session * session)
{
  struct oauth_data *data=(struct oauth_data *)account->data;
  char endpoint[500];
  char * req_url = NULL;
  char * reply   = NULL;
  char * postarg = NULL;
  char * tweet   = NULL;
  
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
	    getuid() ? '$' : '#', tweet);
  else
    sprintf(session->tweet, "%s", tweet);
  if(tweet)
    free(tweet);
  dbg("tweet = %s\n", session->tweet);
	
  char * escaped_tweet = oauth_url_escape(session->tweet);
  sprintf(endpoint, "%s%s?source=%s&status=%s", data->host_url, data->update_uri, data->consumer_key, escaped_tweet);
  if (!session->dry_run) {
    req_url = oauth_sign_url2(endpoint, &postarg, OA_HMAC, NULL,
			      data->consumer_key,
			      data->consumer_secret,
			      data->access_token_key,
			      data->access_token_secret);
    
    reply = oauth_http_post(req_url, postarg);
    dbg("%s\n", req_url);
    dbg("%s\n", reply);
    if(req_url)
      free(req_url);
    if(escaped_tweet)
      free(escaped_tweet);
  }

  if(!reply){
    dbg("Error retrieving from URL(%s)\n",endpoint);
    return -1;
  }
  //FIXME:sina return xml is different
  parse_timeline(reply, session);
  return 0;
}

int oauth_friends(struct account * account, struct session * session)
{
    struct oauth_data *data=(struct oauth_data *)account->data;
  char endpoint[500];
  char * req_url = NULL;
  char * reply   = NULL;
  sprintf(endpoint,"%s%s?source=%s", data->host_url, data->friends_uri, data->consumer_key);
  
  if (!session->dry_run) {
    req_url = oauth_sign_url2(endpoint, NULL, OA_HMAC,
			      NULL, data->consumer_key,
			      data->consumer_secret,
			      data->access_token_key,
			      data->access_token_secret);
    reply = oauth_http_get(req_url, NULL);
    dbg("%s\n", req_url);
    dbg("%s\n", reply);
    if(req_url)
      free(req_url);
  }

  if(!reply){
    dbg("Error retrieving from URL(%s)\n",endpoint);
    return -1;
  }
  parse_timeline(reply, session);
  return 0;
}

int oauth_destory(struct account * account)
{
  struct oauth_data *data=(struct oauth_data *)account->data;
  free(data->consumer_key);
  free(data->consumer_secret);
  free(data->access_token_key);
  free(data->access_token_secret);
  free(data->host_url);
  free(data->request_token_uri);
  free(data->access_authrize_uri);
  free(data->access_token_uri);
  free(data->user_uri);
  free(data->update_uri);
  free(data->public_uri);
  free(data->friends_uri);
  free(data->mentions_uri);
  free(data->replies_uri);
  free(data->retweet_uri);
  return 0;
}

