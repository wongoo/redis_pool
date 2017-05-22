CC=gcc
INCLUDES=-I/usr/local/include
CFLAGS= -g -Wall -fPIC ${INCLUDES}
LIBS = -L/usr/local/lib -lhiredis -levent
DEPS = redis_conf.h redis_pool.h
OBJ = redis_pool.o
PROG = test_redis_pool

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(PROG): $(OBJ) $(PROG).o
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

libredis_pool.a: $(OBJ)
	ar -rcs libredis_pool.a redis_pool.o

libredis_pool.so: $(OBJ)
	gcc -o libredis_pool.so redis_pool.o $(CFLAGS) $(LIBS) -shared

install: libredis_pool.so libredis_pool.a
	cp libredis_pool.a /usr/local/lib
	cp libredis_pool.so /usr/local/lib
	cp *.h /usr/local/include

clean:;         $(RM) $(PROG) *.o *.a *.so

