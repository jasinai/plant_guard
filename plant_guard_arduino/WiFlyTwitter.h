#ifndef WIFLYTWITTER_H
#define WIFLYTWITTER_H

class WiFlyTwitter { 
public:
    WiFlyTwitter();
    ~WiFlyTwitter();
    void setupWiFly();
    int post(const char *msg);
    
private:
    bool postToTwitter(const char *msg);
    bool checkStatus(Print *debug);
    int wait(Print *debug);
};

#endif