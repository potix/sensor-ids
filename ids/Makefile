CC = gcc
CFLAGS = -O2 -Wall -g -ggdb3 -pipe
CFLAGS += -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wnested-externs -Wlong-long -Wuninitialized
#CFLAGS += -Wconversion
LIBS = -levent -lusb
OBJS = ids.o alert.o sensor.o rpc.o tcpsock.o config.o string_util.o
PROG = ids

$(PROG): Makefile $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $(OBJS) 
.c.o:
	$(CC) $(CFLAGS) -o $(<:.c=.o) -c $<

ids.o: macro.h ids.h alert.h sensor.h rpc.h
alert.o: macro.h alert.h
sensor.o: macro.h sensor.h alert.h
rpc.o: macro.h rpc.h alert.h sensor.h
tcpsock.o: macro.h tcpsock.h
config.o: macro.h string_util.h config.h
string_util.o: macro.h string_util.h

install:
	install -D -m 755 $(PROG) /var/ids/$(PROG)
clean:
	rm -rf *.o $(PROG)
