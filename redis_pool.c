
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <event.h>
#include <hiredis/adapters/libevent.h>
#include "redis_pool.h"

/* ----------------------------------------------
 connect request struct
 ---------------------------------------------- */
struct redis_pool_conn_req {
    struct event ev;
    struct redis_pool *p;
    
    struct timeval tv;
    int retry_times;
};

/* ----------------------------------------------
 function defintion
 ---------------------------------------------- */
static void
redis_pool_schedule_reconnect(struct redis_pool_conn_req *);

redisAsyncContext *
redis_pool_connect(struct redis_pool_conn_req *req);


/* ----------------------------------------------
 new pool
 ---------------------------------------------- */
struct redis_pool *
redis_pool_new(struct event_base *base, struct redis_conf *conf, int count) {
    
    struct redis_pool *p = calloc(1, sizeof(struct redis_pool));
    
    p->count = count;
    
    p->ac = calloc(count, sizeof(redisAsyncContext*));
    int i;
    for(i=0;i<count;i++) p->ac[i] = NULL;
    
    p->connecting = 0;
    p->shutdown = 0;
    
    p->cfg = conf;
    p->base = base;
    
    return p;
}

/* ----------------------------------------------
 free redis context
 ---------------------------------------------- */
void
redis_pool_free_context(redisAsyncContext *ac) {
    if (ac)	{
        if(ac->data != NULL)
        {
            /* free connection request */
            free(ac->data);
        }
        
        redisAsyncDisconnect(ac);
    }
}

/* ----------------------------------------------
 free pool
 ---------------------------------------------- */
void
free_redis_pool(struct redis_pool *pool) {
    pool->shutdown = 1;
    int i;
    /* create connections */
    for(i = 0; i < pool->count; ++i) {
        redis_pool_free_context((redisAsyncContext*)pool->ac[i]);
    }
    free(pool->ac);
    free(pool);
    pool = NULL;
}

/* ----------------------------------------------
 connection callback
 ---------------------------------------------- */
static void
redis_pool_on_connect(const redisAsyncContext *ac, int status) {
    struct redis_pool_conn_req *req = ac->data;
    struct redis_pool *p = req->p;
    
    int i = 0;
    
    if(!p || status == REDIS_ERR || ac->err) {
        syslog(LOG_ERR, "Connection failed: %s", ac->errstr);
        if (p) {
            redis_pool_schedule_reconnect(req);
        }
        return;
    }
    
    /* connected to redis, reset connection request */
    req->retry_times = 0;
    
    /* add to redis_pool */
    int inserted = 0;
    for(i = 0; i < p->count; ++i) {
        if(p->ac[i] == NULL) {
            p->ac[i] = ac;
            inserted = 1;
            break;
        }
    }
    if(inserted == 0){
        redis_pool_free_context((redisAsyncContext *)ac);
    }
}

/* ----------------------------------------------
 trigger reconnect
 ---------------------------------------------- */
static void
redis_pool_can_connect(int fd, short event, void *ptr) {
    struct redis_pool_conn_req *req = ptr;
    
    (void)fd;
    (void)event;
    
    redis_pool_connect(req);
}

/* ----------------------------------------------
 schedule reconnect
 ---------------------------------------------- */
static void
redis_pool_schedule_reconnect(struct redis_pool_conn_req *req) {
    if(req==NULL || req->p == NULL)
    {
        return;
    }
    
    evtimer_set(&req->ev, redis_pool_can_connect, req);
    event_base_set(req->p->base, &req->ev);
    evtimer_add(&req->ev, &req->tv);
}

/* ----------------------------------------------
 disconnect callback
 ---------------------------------------------- */
static void
redis_pool_on_disconnect(const redisAsyncContext *ac, int status) {
    struct redis_pool_conn_req *req = ac->data;
    struct redis_pool *p = req->p;
    int i = 0;
    if (status != REDIS_OK) {
        fprintf(stderr, "Error: %s\n", ac->errstr);
    }
    
    if(p == NULL || p->shutdown) { /* no need to clean anything here. */
        return;
    }
    
    /* remove from the redis_pool */
    for(i = 0; i < p->count; ++i) {
        if(p->ac[i] == ac) {
            p->ac[i] = NULL;
            break;
        }
    }
    
    /* schedule reconnect */
    redis_pool_schedule_reconnect(req);
}


/* ----------------------------------------------
 redis connect
 ---------------------------------------------- */
redisAsyncContext *
redis_pool_connect(struct redis_pool_conn_req *req) {
    struct redis_pool *p = req->p;
    
    if(req->retry_times == 0){
        /* increase connecting count */
        p->connecting++;
    }
    
    /* increase retry times */
    req->retry_times++;
    
    if(p->cfg->max_conn_retry_times > 0 && req->retry_times > p->cfg->max_conn_retry_times)
    {
        syslog(LOG_ERR, "can't connect redis, alreay reach the max connection try times %d\n", p->cfg->max_conn_retry_times);
        
        /* decrease connecting count */
        p->connecting--;
        
        return NULL;
    }
    
    struct redisAsyncContext *ac;
    if(p->cfg->redis_host[0] == '/') { /* unix socket */
        ac = redisAsyncConnectUnix(p->cfg->redis_host);
    } else {
        ac = redisAsyncConnect(p->cfg->redis_host, p->cfg->redis_port);
    }
    
    ac->data = req;
    
    if(ac->err) {
        syslog(LOG_ERR, "Connection failed: %s", ac->errstr);
        
        redisAsyncFree(ac);
        redis_pool_schedule_reconnect(req);
        return NULL;
    }
    
    redisLibeventAttach(ac, p->base);
    redisAsyncSetConnectCallback(ac, redis_pool_on_connect);
    redisAsyncSetDisconnectCallback(ac, redis_pool_on_disconnect);
    
    if(p->cfg->redis_auth) { /* authenticate. */
        redisAsyncCommand((redisAsyncContext *)ac, NULL, NULL, "AUTH %s", p->cfg->redis_auth);
    }
    if(p->cfg->database) { /* change database. */
        redisAsyncCommand((redisAsyncContext *)ac, NULL, NULL, "SELECT %d", p->cfg->database);
    }
    return ac;
}

/* ----------------------------------------------
 new connection request
 ---------------------------------------------- */
struct redis_pool_conn_req *
redis_pool_conn_req_new(struct redis_pool *p)
{
    struct redis_pool_conn_req *req = malloc(sizeof(struct redis_pool_conn_req));
    req->p = p;
    req->retry_times = 0;
    
    req->tv.tv_sec = 1; /* 1 sec*/
    req->tv.tv_usec = 0;
    return req;
}

/* ----------------------------------------------
 get redis context
 ---------------------------------------------- */
const redisAsyncContext *
redis_pool_get_context(struct redis_pool *p) {
    int orig = p->current;
    
    do {
        p->current++;
        p->current %= p->count;
        if(p->ac[p->current] != NULL) {
            return p->ac[p->current];
        }else{
            if(p->connecting < p->count){
                struct redis_pool_conn_req *req = redis_pool_conn_req_new(p);
                redis_pool_connect(req);
            }
        }
    } while(p->current != orig);
    
    return NULL;
    
}

/* ----------------------------------------------
 pool init
 ---------------------------------------------- */
void
redis_pool_init(struct redis_pool *pool) {
    
    int i;
    /* create connections */
    for(i = 0; i < pool->count; ++i) {
        struct redis_pool_conn_req *req = redis_pool_conn_req_new(pool);
        redis_pool_connect(req);
    }
}

