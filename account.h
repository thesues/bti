/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
#ifndef __ACCOUNT_H
#define __ACCOUNT_H
//interface
/*void * is account data*/
struct session;
struct account{
	struct ops_t * opts;
	void * data;
};


struct ops_t {
	int (*action_update)(struct account *, struct session *);
	int (*action_friends)(struct account *, struct session *);
	int (*action_user)(struct account *, struct session *);
	int (*action_replies)(struct account *, struct session *);
	int (*action_public)(struct account *, struct session *);
	int (*action_group)(struct account *, struct session *);
	int (*action_retweet)(struct account *, struct session *);
	int (*destory)(struct account *);
};

#define UPDATES(account, session)							\
	account->opts->action_update(account,session);

#define FRIENDS(account, session) \
	account->opts->action_friends(account,session);

#define PUBLIC(account, session) \
	account->opts->action_public(account,session);

#define DESTORY(account) \
	account->opts->destory(account);


#endif

