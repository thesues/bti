struct ops_t {
  void (*action_update);
  void (*action_friends);
  void (*action_user);
  void (*action_replies);
  void (*action_public);
  void (*action_group);
  void (*action_retweet);
};
struct account{
  char * password;
  char * account;
  char * consumer_key;
  char * consumer_secret;
  char * access_token_key;
  char * access_token_secret;
  char * tweet;
  char * hosturl;
  struct ops_t ops;
};
