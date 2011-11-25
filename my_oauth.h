#ifndef _MY_OAUTH_H
#define _MY_OAUTH_H
#include "bti.h"
struct oauth_data {
  char * consumer_key;
  char * consumer_secret;
  char * access_token_key;
  char * access_token_secret;
  char * host;
  char * request_token_uri;
  char * access_authrize_uri;
  char * access_token_uri;
  /*uri list like /user_timeline*/
  char * user_uri;
  char * update_uri;
  char * public_uri;
  char * friends_uri;
  char * mentions_uri;
  char * replies_uri;
  char * retweet_uri;
  char * group_uri;
};

int oauth_destory(struct account *);
int oauth_public(struct account * ,struct session *);
/*TODO: other methods is going*/

#endif
