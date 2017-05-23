#ifndef REDIS_POOL_H
#define REDIS_POOL_H

#include <hiredis/async.h>
#include "redis_conf.h"

struct redis_pool {
    
    struct redis_conf *cfg;
    
    const redisAsyncContext **ac;
    int connecting;
    int count;
    int cur;
    
    struct event_base *base;
    
};


struct redis_pool *
redis_pool_new(struct event_base *base, struct redis_conf *conf, int count);

void
redis_pool_init(struct redis_pool *pool);

const redisAsyncContext *
redis_pool_get_context(struct redis_pool *p);

void
free_redis_pool(struct redis_pool *pool);

#endif
