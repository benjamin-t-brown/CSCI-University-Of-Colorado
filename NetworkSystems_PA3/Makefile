CC = gcc
CFLAGS = -g -I./
LFLAGS = -std=gnu99 -lm -lpthread

.PHONY: all routed_LS tostring

all: clean routed_LS

routed_LS: 
	$(CC) -o $@ routed_LS.c $(CFLAGS) $(LFLAGS)  

tostring:
	$(CC) -o $@ tostring.c $(CFLAGS) $(LFLAGS)  

clean:
	rm -f routed_LS
	rm -f tostring
	rm -f *.log
	rm -f *.o
	rm -f *~
