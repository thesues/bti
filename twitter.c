#include "account.h"
/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
struct account{
  char * consumer_key;
  char * consumer_secret;
  char * access_token_key;
  char * access_token_secret;
};

static const char user_uri[]     = "/user_timeline/";
static const char update_uri[]   = "/update.xml";
static const char public_uri[]   = "/public_timeline.xml";
static const char friends_uri[]  = "/friends_timeline.xml";
static const char mentions_uri[] = "/mentions.xml";
static const char replies_uri[]  = "/replies.xml";
static const char retweet_uri[]  = "/retweet/";
static const char group_uri[]    = "/../statusnet/groups/timeline/";
static const char host[]="http://api.twitter.com/1/statuses";
static const char request_token_uri[]="http://twitter.com/oauth/request_token";
static const char access_authrize_uir[]="http://twitter.com/oauth/authorize?oauth_token=";


struct twitter{
  struct ops_t * opts;
  void * data;
};


/*if twitter has not authorized,promote users to autor it*/
struct twitter * twitter_init(char * consumer_key,char *consumer_secret,char * access_token_key,char * access_token_secret){
  if(){
    
  }
}
static ops_t funcs_for_ops={
};
