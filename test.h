namespace bti
{
  class Account
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
    virtual bool update()=0;
    virtual bool friends()=0;
    virtual bool user()=0;
    virtual bool reples()=0;
    virtual bool publicline()=0;
    virtual bool group()=0;
    virtual bool retweet()=0;
  };

  class OauthAcount:public Acount
  {
  public:
    virtual bool update();
    virtual bool friends();
    virtual bool user();
    virtual bool reples();
    virtual bool publicline();
    virtual bool group();
    virtual bool retweet();
  };

  class BaseAcount:public Acount
  {
  public:
    virtual bool update();
    virtual bool friends();
    virtual bool user();
    virtual bool reples();
    virtual bool publicline();
    virtual bool group();
    virtual bool retweet();
  };

  class Session
  {
  public:
    vector<Account*> accountList;
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
    Session(){}
  };
}
