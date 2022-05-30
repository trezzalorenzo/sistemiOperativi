CC		=	gcc
CFLAGS		+=	-std=gnu99 -Wall -Werror -g -pedantic
INCLUDES	=	-I. -I ./include
LDFLAGS		=	-L.
OPTFLAGS	=
T_LIBS		= 	-lpthread

TARGETS		=	server	\
			client

.PHONY: all clean cleanall server client test1

all		: $(TARGETS)

server: server.c libs/libhash.so
	$(CC) $(CFLAGS) $(INCLUDES) $(T_LIBS) server.c -o server -Wl,-rpath,./libs -L ./libs -lhash

client: client.c libs/libapi.so
	$(CC) $(CFLAGS) $(INCLUDES) $(T_LIBS) client.c -o client -Wl,-rpath,./libs -L ./libs -lapi

libs/libhash.so: hash.o
	$(CC) -shared -o libs/libhash.so $^

hash.o:
	$(CC) $(CFLAGS) $(INCLUDES) HashLFU.c -c -fpic -o $@

libs/libapi.so: api.o
	$(CC) -shared -o libs/libapi.so $^	

api.o:
	$(CC) $(CFLAGS) $(INCLUDES) api.c -c -fpic -o $@
	
clean		:
	rm -f $(TARGETS)
	
cleanall	: clean
	\rm -f *.o *~ *.a *.sk ./storage_sock ./log_file.txt ./valgrind-out.txt ./libs/*.* ./letti/* ./espulsi/*

test1: cleanall all
	./scripts/test1.sh

test2: cleanall all
	./scripts/test2.sh

test3: cleanall all
	./scripts/test3.sh
