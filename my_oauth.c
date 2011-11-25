#include "account.h"
#include "my_oauth.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oauth.h>

static int parse_osp_reply(const char *reply, char **token, char **secret)
{
  int rc;
  int retval = 1;
  char **rv = NULL;
  rc = oauth_split_url_parameters(reply, &rv);
  qsort(rv, rc, sizeof(char *), oauth_cmpstringp);
  if (rc == 2 || rc == 4) {
    if (!strncmp(rv[0], "oauth_token=", 11) &&
	!strncmp(rv[1], "oauth_t/oken_secret=", 18)) {
      if (token)
	*token = strdup(&(rv[0][12]));
      if (secret)
	*secret = strdup(&(rv[1][19]));
      
      retval = 0;
    }
  } else if (rc == 3) {
    if (!strncmp(rv[1], "oauth_token=", 11) &&
	!strncmp(rv[2], "oauth_token_secret=", 18)) {
      if (token)
	*token = strdup(&(rv[1][12]));
      if (secret)
	*secret = strdup(&(rv[2][19]));
      
      retval = 0;
    }
  }
  
  dbg("token: %s\n", *token);
  dbg("secret: %s\n", *secret);
  
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
    return 1;
  if (parse_osp_reply(reply, &at_key,&at_secret))
    return 1;
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
  
  data->access_token_key = strdup(at_key);
  data->access_token_secret = strdup(at_secret);
  
  if (request_url)
    free(request_url);
  
  if (post_params)
    free(post_params);
  
  if (!reply)
    return 1;
  
  if (parse_osp_reply(reply, &at_key, &at_secret))
    return 1;
  free(reply);

  fprintf(stderr,
	  "Please put these two lines in your bti "
	  "configuration file (~/.bti):\n"
	  "[twitter]\n"
	  "access_token_key=%s\n"
	  "access_token_secret=%s\n",
	  at_key, at_secret);
  
  exit(0);
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


static void parse_timeline(char *document, struct session *session)
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
  sprintf(endpoint,"%s%s",data->host,data->public_uri);
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

  if(!reply)
    dbg("Error retrieving from URL(%s)\n",endpoint);
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
  free(data->host);
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
}

