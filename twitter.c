/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
/*
 * forked from Greg Kroah-Hartman's bti project
 * see https://github.com/thesues/bti
 */
#include "account.h"
#include "my_oauth.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int twitter_public( struct account * account ,struct session * session)
{
	struct oauth_data *data = (struct oauth_data *)account->data;
	return oauth_public(account, session, NULL);
}
static int twitter_update( struct account * account ,struct session * session)
{
	struct oauth_data *data = (struct oauth_data *)account->data;
	return oauth_update(account, session, NULL);
}
static int twitter_friends( struct account * account ,struct session * session)
{
	struct oauth_data *data = (struct oauth_data *)account->data;
	return oauth_friends(account, session, NULL);
}

static struct ops_t twitter_ops = {
	.action_public = twitter_public,
	.action_update = twitter_update,
	.action_friends = twitter_friends,
	.action_replies = oauth_replies,
	.destory = oauth_destory
};

/*if twitter has not authorized,promote users to authorize it*/
struct account *twitter_init(const char *consumer_key,
			     const char *consumer_secret)
{
	struct account *tw = (struct account *)malloc(sizeof(struct account));
	struct oauth_data *data = malloc(sizeof(struct oauth_data));
	if (data == NULL)
		dbg("cannot alloc memory for struct twitter\n");

	tw->data = data;
	tw->opts = &twitter_ops;

	data->consumer_key = strdup(consumer_key);
	data->consumer_secret = strdup(consumer_secret);
	data->host = TWITTER;
	data->host_url = strdup("http://api.twitter.com/1/statuses");
	data->request_token_uri =
	    strdup("http://twitter.com/oauth/request_token");
	data->access_token_uri =
	    strdup("http://twitter.com/oauth/access_token");
	data->access_authrize_uri =
	    strdup("http://twitter.com/oauth/authorize?oauth_token=");
	data->user_uri = strdup("/user_timeline/");
	data->update_uri = strdup("/update.xml");
	data->public_uri = strdup("/public_timeline.xml");
	data->friends_uri = strdup("/friends_timeline.xml");
	data->mentions_uri = strdup("/mentions.xml");
	data->replies_uri = strdup("/replies.xml");
	data->retweet_uri = strdup("/retweet/");
	return tw;
}
