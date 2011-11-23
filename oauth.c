#include "account"
#include "oauth.h"

int parse_osp_reply(const char *reply, char **token, char **secret)
{
  int rc;
  int retval = 1;
  char **rv = NULL;
  rc = oauth_split_url_parameters(reply, &rv);
  qsort(rv, rc, sizeof(char *), oauth_cmpstringp);
  if (rc == 2 || rc == 4) {
    if (!strncmp(rv[0], "oauth_token=", 11) &&
	!strncmp(rv[1], "oauth_token_secret=", 18)) {
      if (token)
	*token = strdup(&(rv[0][12]));
      if (secret)
	*secret = strdup(&(rv[1][19]));
      
      retval = 0;
    }
  } else if (rc == 3) {
    if (!strncmp(rv[1], "oauth_token=", 11) &&
	!strncmp(rv[2], "oauth_token_secret=", 18)) {
      if (token)
	*token = strdup(&(rv[1][12]));
      if (secret)
	*secret = strdup(&(rv[2][19]));
      
      retval = 0;
    }
  }
  
  dbg("token: %s\n", *token);
  dbg("secret: %s\n", *secret);
  
  if (rv)
    free(rv);
  
  return retval;
}

int twitter_get_token(struct twitter * tw,struct session * session){
  char *post_params = NULL;
  char *request_url = NULL;
  char *reply       = NULL;
  char *at_key      = NULL;
  char *at_secret   = NULL;
  char *verifier    = NULL;
  char at_uri[90];
  
  request_url = oauth_sign_url2(tw->data->request_token_uri,NULL,
			      OA_HMAC,NULL,
			      tw->data->consumer_key,
			      tw->data->consumer_secret,NULL,NULL);
  
  reply = oauth_http_get(request_url,post_params);
  if (request_url)
    free(post_params);
  if (!reply)
    return 1;
  if (parse_osp_reply(reply, &at_key,&at_secret))
    return 1;
  free(reply);
  fprintf(stdout,
	  "Please open the following link in your browser, and "
	  "allow 'bti' to access your account. Then paste "
	  "back the provided PIN in here.\n");
  fprintf(stdout,"%s%s\nPIN: ",
	  tw->data->access_authrize_uri,at_key);
  verifier = session->readline(NULL);
  sprintf(at_uri, "%s?oauth_verifier=%s",
	  tw->data->access_token_uri, verifier);
  
  request_url = oauth_sign_url2(at_uri, NULL, OA_HMAC, NULL,
				tw->data->consumer_key,
				tw->data->consumer_secret,
				at_key,
				at_secret);
  //FIXME:check at_key and at_secret first;
  
  tw->data->access_token_key = strdup(at_key);
  tw->data->access_token_secret = strdup(at_secret);
  
  if (request_url)
    free(request_url);
  
  if (post_params)
    free(post_params);
  
  if (!reply)
    return 1;
  
  if (parse_osp_reply(reply, &at_key, &at_secret))
    return 1;
  free(reply);

  fprintf(stdout,
	  "Please put these two lines in your bti "
	  "configuration file (~/.bti):\n"
	  "[twitter]\n"
	  "access_token_key=%s\n"
	  "access_token_secret=%s\n",
	  at_key, at_secret);
  
  exit(0);
}

