CC = g++
CFLAGS = -Wall -std=c++11

all: threadpool

threadpool: threadpool.o
	$(CC) $(CFLAGS) -o threadpool threadpool.o

threadpool.o:
	$(CC) $(CFLAGS) -c threadpool.cpp

clean:
	rm -rf threadpool *.o
