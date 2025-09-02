CC = gcc
CFLAGS = -O2 -Wall -fPIC
LIB_OBJS = mbmalloc.o
STATIC_LIB = libmbmalloc.a
SHARED_LIB = libmbmalloc.so
TEST_OBJS = test_mbmalloc.o
TEST = test_mbmalloc

all: $(STATIC_LIB) $(SHARED_LIB) $(TEST)

$(STATIC_LIB): $(LIB_OBJS)
	ar rcs $(STATIC_LIB) $(LIB_OBJS)

$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared -o $(SHARED_LIB) $(LIB_OBJS)

$(TEST): $(TEST_OBJS) $(SHARED_LIB)
	$(CC) $(CFLAGS) -o $(TEST) $(TEST_OBJS) -L. -lmbmalloc -lnuma

mbmalloc.o: mbmalloc.c mbmalloc.h
	$(CC) $(CFLAGS) -c mbmalloc.c -o mbmalloc.o

test_mbmalloc.o: test_mbmalloc.c mbmalloc.h
	$(CC) $(CFLAGS) -c test_mbmalloc.c -o test_mbmalloc.o

clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB) $(TEST)

.PHONY: all clean
