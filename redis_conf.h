#ifndef REDIS_CONF_H
#define REDIS_CONF_H

struct redis_conf {
    /* connection to Redis */
    char *redis_host;
    short redis_port;
    char *redis_auth;
    
    /* database number */
    int database;
    
    /* max connection retry times */
    int max_conn_retry_times;
    
};

// struct redis_conf *conf_read(const char *filename);
// void conf_free(struct redis_conf *conf);

#endif /* REDIS_CONF_H */
