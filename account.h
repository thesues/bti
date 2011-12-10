/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- */
/*
 * forked from Greg Kroah-Hartman's bti project
 * see https://github.com/thesues/bti
 */
#ifndef __ACCOUNT_H
#define __ACCOUNT_H

/*void * is account data*/
struct session;
struct account {
	struct ops_t *opts;
	void *data;
	struct account *next;
};

struct ops_t {
	int (*action_update) (struct account *, struct session *);
	int (*action_friends) (struct account *, struct session *);
	int (*action_user) (struct account *, struct session *);
	int (*action_replies) (struct account *, struct session *);
	int (*action_public) (struct account *, struct session *);
	int (*action_group) (struct account *, struct session *);
	int (*action_retweet) (struct account *, struct session *);
	int (*destory) (struct account *);
};

#define UPDATE(acc, session, ret)							\
	do {													\
		struct account *p = acc;							\
		while (p!=NULL) {									\
			if (p->opts->action_update)						\
				ret = p->opts->action_update(p, session);	\
			p = p->next;									\
		}													\
	}while(0)
			

#define FRIENDS(acc, session , ret)							\
    do {													\
		struct account *p = acc;							\
		while (p!=NULL && p->opts->action_update){			\
			if (p->opts->action_update)						\
				ret = p->opts->action_friends(p, session);	\
			p = p->next;									\
		}													\
	} while(0)

#define PUBLIC(acc, session, ret)							\
	do {													\
		struct account *p = acc;							\
		while (p != NULL){									\
			if (p->opts->action_public)						\
				ret = p->opts->action_public(p, session);	\
			p = p->next;									\
		}													\
	} while(0)

#define REPLIES(acc, session, ret)							\
	do {													\
		struct account *p = acc;							\
		while (p != NULL){									\
			if (p->opts->action_replies)					\
				ret = p->opts->action_replies(p, session);	\
			p = p->next;									\
		}													\
	} while(0)

#define DESTORY(acc)							\
	do {										\
		struct account *p = acc;				\
		struct account *tmp;					\
		while (p!=NULL){						\
			tmp=p->next;						\
			p->opts->destory(p);				\
			p=tmp;								\
		}										\
	} while(0)

#endif
