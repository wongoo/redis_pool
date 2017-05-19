CC=gcc
INCLUDES=-I/usr/local/include
CFLAGS= -g -Wall ${INCLUDES}
LIBS = -L/usr/local/lib -lhiredis -levent
DEPS = redis_conf.h redis_pool.h
OBJ = redis_pool.o
PROG = test_redis_pool

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(PROG): $(OBJ) $(PROG).o
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

redis_pool.a: $(OBJ)
	ar -rcs redis_pool.a redis_pool.o

redis_pool.so: $(OBJ)
	gcc -o redis_pool.so redis_pool.o $(CFLAGS) $(LIBS) -shared

install: redis_pool.so redis_pool.a
	cp redis_pool.a /usr/local/lib
	cp redis_pool.so /usr/local/lib
	cp *.h /usr/local/include

clean:;         $(RM) $(PROG) *.o *.a *.so

