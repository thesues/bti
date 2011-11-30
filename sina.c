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
#include <oauth.h>

static struct ops_t sina_ops = {
	.action_public = oauth_public,
	.action_update = oauth_update,
	.action_friends = oauth_friends,
	.action_replies = oauth_replies,
	.destory = oauth_destory
};

struct account *sina_init(const char *consumer_key, const char *consumer_secret)
{
	struct account *sina = malloc(sizeof(struct account));
	struct oauth_data *data = malloc(sizeof(struct oauth_data));
	if (data == NULL)
		dbg("cannot alloc memory for struct twitter\n");

	sina->data = data;
	sina->opts = &sina_ops;

	data->consumer_key = strdup(consumer_key);
	data->consumer_secret = strdup(consumer_secret);
	data->host = SINA;
	data->host_url = strdup("http://api.t.sina.com.cn/statuses");
	data->request_token_uri =
	    strdup("http://api.t.sina.com.cn/oauth/request_token");
	data->access_token_uri =
	    strdup("http://api.t.sina.com.cn/oauth/access_token");
	data->access_authrize_uri =
	    strdup("http://api.t.sina.com.cn/oauth/authorize?oauth_token=");
	data->user_uri = strdup("/user_timeline/");
	data->update_uri = strdup("/update.xml");
	data->public_uri = strdup("/public_timeline.xml");
	data->friends_uri = strdup("/home_timeline.xml");
	data->mentions_uri = strdup("/mentions.xml");
	data->replies_uri = strdup("/replies.xml");
	data->retweet_uri = strdup("/retweet/");
	return sina;
}
