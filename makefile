bti:bti.c bti.h twitter.c account.h parseini.c  my_oauth.c my_oauth.h makefile sina.c
	gcc -g -obti bti.c twitter.c parseini.c my_oauth.c sina.c -loauth -lxml2 -liniparser -lpcre
clean:
	rm bti
