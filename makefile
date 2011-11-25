test:bti.c bti.h twitter.c twitter.h account.h parseini.c  my_oauth.c my_oauth.h
	gcc -g -DDEBUG -obti bti.c twitter.c parseini.c my_oauth.c -loauth -lxml2 -liniparser -lpcre
