COMPILER = gcc
CFLAGS = -Wall -o
all: tftpserv
tftpserv: tftpserver.o
tftpserver.o:
	$(COMPILER) tftpserver.c $(CFLAGS) tftpserver
clean:
	rm -rf *.o tftpserv tftpserv.o
