/* -*- mode: c; c-basic-offset: 8; tab-width: 8; indent-tabs-mode: t -*- */
#include "account.h"
#include "my_oauth.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <oauth.h>

int sina_public(struct account * account ,struct session * session)
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

static struct ops_t sina_ops = {
	.action_public  = sina_public,
	.action_update  = oauth_update,
	.action_friends = oauth_friends,
	.destory = oauth_destory
};

struct account * sina_init(const char * consumer_key,const char *consumer_secret){
  struct account * sina = malloc(sizeof(struct account));
  struct oauth_data * data = malloc(sizeof(struct oauth_data));
  if ( data == NULL)
    dbg("cannot alloc memory for struct twitter\n");
  
  sina->data = data;
  sina->opts = &sina_ops;
  
  data->consumer_key        = strdup(consumer_key);
  data->consumer_secret     = strdup(consumer_secret);
  data->host                = SINA;
  data->host_url            = strdup("http://api.t.sina.com.cn/statuses");
  data->request_token_uri   = strdup("http://api.t.sina.com.cn/oauth/request_token");
  data->access_token_uri    = strdup("http://api.t.sina.com.cn/oauth/access_token");
  data->access_authrize_uri = strdup("http://api.t.sina.com.cn/oauth/authorize?oauth_token=");
  
  data->user_uri            = strdup("/user_timeline/");
  data->update_uri          = strdup("/update.xml");                   
  data->public_uri          = strdup("/public_timeline.xml");          
  data->friends_uri         = strdup("/home_timeline.xml");         
  data->mentions_uri	    = strdup("/mentions.xml");                 
  data->replies_uri         = strdup("/replies.xml");                  
  data->retweet_uri         = strdup("/retweet/");
  return sina;
}

		



