COMPILER = gcc
CFLAGS = -Wall -o
all: tftpserv
tftpserv: server.o
server.o:
	$(COMPILER) server.c $(CFLAGS) server
clean:
	rm -rf *.o tftpserv tftpserv.o
