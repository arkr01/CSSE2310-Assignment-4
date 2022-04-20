CFLAGS = -Wall -pedantic -g -pthread -std=gnu99
.PHONY: all clean
.DEFAULT_GOAL := all

all: mapper2310 control2310 roc2310

roc2310: roc2310.o general.o errors.o
	gcc $(CFLAGS) -o roc2310 roc2310.o general.o errors.o

roc2310.o: roc2310.c roc2310.h
	gcc $(CFLAGS) -c roc2310.c

control2310: control2310.o general.o errors.o
	gcc $(CFLAGS) -o control2310 control2310.o general.o errors.o

control2310.o: control2310.c control2310.h
	gcc $(CFLAGS) -c control2310.c

mapper2310: mapper2310.o general.o errors.o
	gcc $(CFLAGS) -o mapper2310 mapper2310.o general.o errors.o

mapper2310.o: mapper2310.c mapper2310.h
	gcc $(CFLAGS) -c mapper2310.c

general.o: general.c general.h
	gcc $(CFLAGS) -c general.c

errors.o: errors.c errors.h
	gcc $(CFLAGS) -c errors.c

clean:
	rm *.o mapper2310 control2310 roc2310
