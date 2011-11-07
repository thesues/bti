#ifndef __ACCOUNT_H
#define __ACCOUNT_H

/*void * is account data*/
struct ops_t {
  void (*action_update)(void *,struct session *);
  void (*action_friends)(void  *,struct session *);
  void (*action_user)(void  *.struct session *);
  void (*action_replies)(void *,struct session *);
  void (*action_public)(void  *,struct session *);
  void (*action_group)(void  *,struct session *);
  void (*action_retweet)(void  *,struct session *);
  void (*parse_statuses)(void  *,struct session *);
  void (*log_session)(void  * ,struct session*);
};

#endif
