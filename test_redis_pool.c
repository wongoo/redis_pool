
#include <stdio.h>
#include <stdlib.h>
#include <event.h>
#include <unistd.h>

#include <hiredis/async.h>
#include "redis_pool.h"

/* ----------------------------------------------
 ---------------------------------------------- */
void
redis_command_cb(redisAsyncContext *ac, void *r, void *privdata)
{
    if(ac->err) {
        printf("command error:%s\n", ac->errstr);
    }
    else
    {
        redisReply *reply = r;
        printf("command data:%s\n", reply->str);
    }
}

/* ----------------------------------------------
 ---------------------------------------------- */
static void
redis_pool_test(int fd, short event, void *ptr) {
    struct redis_pool *pool = ptr;
    
    struct redisAsyncContext *ac = (struct redisAsyncContext *) redis_pool_get_context(pool);
    if( ac == NULL){
        printf( "failed to get redis connection from pool!\n");
        return ;
    }
    
    redisAsyncCommand(ac, redis_command_cb, NULL, "set key1 %s", "test1");
    redisAsyncCommand(ac, redis_command_cb, NULL, "set key2 %s", "test2");
    
    redisAsyncCommand(ac, redis_command_cb, NULL, "get key2");
    redisAsyncCommand(ac, redis_command_cb, NULL, "get key2");
}

/* ----------------------------------------------
 ---------------------------------------------- */
int main(int argc, char *argv[])
{
    struct event_base *base = event_base_new();
    
    struct redis_conf *conf= malloc(sizeof(struct redis_conf));
    //conf->redis_host = "127.0.0.1";
    conf->redis_host = "/tmp/redis_6380.sock";
    conf->redis_port = 6380;
    conf->redis_auth = NULL;
    conf->database = -1;
    conf->max_conn_retry_times = 10;
    
    struct redis_pool *pool = redis_pool_new(base, conf, 5);
    redis_pool_init(pool);
    
    struct event ev;
    struct timeval tv;
    tv.tv_sec=2;
    tv.tv_usec=0;
    
    evtimer_set(&ev, redis_pool_test, (void*)pool);
    event_base_set(base, &ev);
    evtimer_add(&ev, &tv);
    
    event_base_dispatch(base);
    
    free_redis_pool(pool);
    free(conf);
    event_base_free(base);
    
    return 0;
    
}
