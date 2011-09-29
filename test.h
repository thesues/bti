namespace bti
{
  class Acount
  {
  public:
    char * account;
    char * password;
    char * consumer_key;
    char * consumer_secret;
    char * access_token_key;
    char * access_token_secret;
    char * hosturl;
    char * hostname;
    int oauth;
    virtual bool update()=0;
    virtual bool friends()=0;
    virtual bool user()=0;
    virtual bool reples()=0;
    virtual bool publicline()=0;
    virtual bool group()=0;
    virtual bool retweet()=0;
  };
  
  class Session
  {
  private:
    struct Acount * acountList;
    int dry_run;
    int replyto;
    char * retweet;
    char * tweet;
    char * proxy;
    char * time;
    char * homedir;
    char * user;
    char * group;
    int bash;
    int background;
    int interactive;
    int shrink_urls;
    int page;
    char *(*readline)(const char *);
    void * readline_handle;
  public:
    Session();
    Session(const Session& lhs);
    const Session& operater= (Session& lhs);
  };
}
