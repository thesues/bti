#include "test.h"
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>


using namespace bti;

bit_parse_configfile(Session * session)
{
  FILE * config_file;
  char * line =NULL;
  char * key = NULL;
  cahr * value =  NULL;
  
}
int main(int argc, char *argv[])
{
  Session session;
  time_t t;
  session.time=strdup(ctime(&t));
  session.time[strlen(session->time)-1]=0;
  bti_parse_configfile(&session);
  
		 
}











