/* -*- mode: c; c-basic-offset: 8; tab-width: 8; indent-tabs-mode: t -*- */
#include "account.h"
#include "my_oauth.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static struct ops_t twitter_ops = {
	.action_public  = oauth_public,
	.action_update  = oauth_update,
	.destory = oauth_destory
};

void twitter_add_token(struct acccount * tw,const char * token_key, const char * token_secret)
{
	struct oauth_data * data = (struct oauth_data *)tw->data;
	data->access_token_key = strdup(token_key);
	data->access_token_secret = strdup(token_secret);
}
/*if twitter has not authorized,promote users to authorize it*/
struct account * twitter_init(const char * consumer_key,const char *consumer_secret){
  struct account * tw = malloc(sizeof(struct account));
  struct oauth_data * data = malloc(sizeof(struct oauth_data));
  if ( data == NULL)
    dbg("cannot alloc memory for struct twitter\n");
  
  tw->data = data;
  tw->opts = &twitter_ops;
  
  data->consumer_key        = strdup(consumer_key);
  data->consumer_secret     = strdup(consumer_secret);
  data->host                = TWITTER;
  data->host_url            = strdup("http://api.twitter.com/1/statuses");
  data->request_token_uri   = strdup("http://twitter.com/oauth/request_token");
  data->access_token_uri    = strdup("http://twitter.com/oauth/access_token");
  data->access_authrize_uri = strdup("http://twitter.com/oauth/authorize?oauth_token=");
  data->user_uri            = strdup("/user_timeline/");
  data->update_uri          = strdup("/update.xml");                   
  data->public_uri          = strdup("/public_timeline.xml");          
  data->friends_uri         = strdup("/friends_timeline.xml");         
  data->mentions_uri	    = strdup("/mentions.xml");                 
  data->replies_uri         = strdup("/replies.xml");                  
  data->retweet_uri         = strdup("/retweet/");
  return tw;
}

		



