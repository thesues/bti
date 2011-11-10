/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
#include "account.h"

struct twitter_account{
  char * consumer_key;
  char * consumer_secret;
  char * access_token_key;
  char * access_token_secret;
  char * host;
  char * request_token_uri;
  char * access_authrize_uri;
  char * access_token_uri;
};

const char user_uri[]     = "/user_timeline/";
const char update_uri[]   = "/update.xml";
const char public_uri[]   = "/public_timeline.xml";
const char friends_uri[]  = "/friends_timeline.xml";
const char mentions_uri[] = "/mentions.xml";
const char replies_uri[]  = "/replies.xml";
const char retweet_uri[]  = "/retweet/";
const char group_uri[]    = "/../statusnet/groups/timeline/";
/* const char host[]="http://api.twitter.com/1/statuses"; */
/* const char request_token_uri[]="http://twitter.com/oauth/request_token"; */
/* const char access_authrize_uir[]="http://twitter.com/oauth/authorize?oauth_token="; */


struct twitter{
  struct ops_t * opts;
  struct twitter_account data;
};
