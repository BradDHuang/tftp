#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <inttypes.h>
#include <signal.h>

#define MAXBUFLEN 100

/*
 * Main function
 */
int main(int argc, char *argv[]) {
  struct addrinfo hints, *servinfo, *p;  // store server socket information
  int rv, servfd, newservfd;  // server socket descriptor
  int numbytes;  // the length of sent or received messages in bytes
  char *buf, *err;  // pointer to the messages
  char bufr[MAXBUFLEN];  // messages that are received
  struct sockaddr_storage their_addr;  // store the address information of clients' socket
  socklen_t addr_len; 
  uint16_t opcode, opcode2, blockno, errcode;  // header fields of tftp
  uint8_t split;
  split = htons(0);
  int count = 0;

  if (argc != 3) {
    printf("usage: <serverIP> <port>\n");// checking for input arguments.
    exit(1);
  }
  memset(&hints, 0, sizeof hints);// setting zeroes to struct hints
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  if ((rv = getaddrinfo((const char *)argv[1], (const char *)argv[2], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((servfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
    perror("tftpserver1: socket");
    continue;
    }
    if (bind(servfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(servfd);
      perror("tftpserver1: bind");
      continue;
    }
    break;
  }
  if (p == NULL) {
    fprintf(stderr, "tftpserver1: failed to bind socket\n");
    return 2;
  }
  // infinite loop to create stand server
  while(1) {
    printf("tftpserver1: waiting to recvfrom clients...\n");
    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(servfd, bufr, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("tftpserver: fail to receive packets from client-1");
      exit(2);
    }
    if ( *(bufr+1) == 1) { //then its an RRQ hence fork. else dont.
      count = count + 1;
      FILE *fp ;
      char *mode = "rb";
      fp = fopen ( bufr+2, mode);
      err = malloc(200);
      char errmsg[14] = "file not found";
      if ( fp == NULL ) {
        // File not found Error
        if (errno == ENOENT) {
          printf("File not found\n");
          // send an error message
          opcode2 = htons(5);
          errcode = htons(1);
          memcpy(err, (const char *)&opcode2, sizeof(uint16_t));
          memcpy(err+sizeof(uint16_t), (const char *)&errcode, sizeof(uint16_t));
          memcpy(err+(2*sizeof(uint16_t)), (const char *)errmsg, sizeof(errmsg));
          memcpy(err+(2*sizeof(uint16_t))+sizeof(errmsg), (const char *)&split, sizeof(uint8_t));
          if ((numbytes = sendto(servfd, err, 19, 0, (struct sockaddr *)&their_addr, p->ai_addrlen)) == -1) {
            perror("tftpserver: fail to send ERROR packet");
          } else {
            printf("tftpserver1: ERROR packet sent successfully\n");
          }
        }
      } else {
        // if file exists, fork to accept multiple client request concurrently
        pid_t pid = fork();
        if (pid == 0) {  // in child process
          int a,b,c;
          char z[5] = "";
          a = atoi(argv[2]);
          c = a/1000;
          b = a%1000;
          sprintf(z,"%d%d", c,b+count);  // random port
          printf("New port no for the client is: %s\n", z);
          // create a new UDP socket with a new port number and bind it to the server
          if ((rv = getaddrinfo((const char *)argv[1], (const char *)z , &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
          }

          for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((newservfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
              perror("tftpserver1: socket");
              continue;
            }
            if (bind(newservfd, p->ai_addr, p->ai_addrlen) == -1) {
              close(newservfd);
              perror("tftpserver1: bind");
              continue;
            }
            break;
          }
          if (p == NULL) {
            fprintf(stderr, "tftpserver1: failed to bind socket\n");
            return 2;
          }
          // read the file and send data
          opcode =htons(3);
          buf = malloc(516);
          memcpy(buf, (const char *)&opcode, sizeof(uint16_t));
          int i,j;
          int data = 0;
          j = 0;
          long fileSize;
          fseek(fp, 0L, SEEK_END);  // go to end of file
          fileSize = ftell(fp);  // obtain current position is the file length
          rewind(fp);  // rewind to beginning
          clearerr(fp);  // reset the flags
          printf("file size is : %ld\n", fileSize);
          if (fileSize > 33554432) {
            //send error message -- exceed maximum file size
            opcode2 =htons(5);
            errcode =htons(4);
            char errmsg2[26] = "maximum file size exceeded";
            printf("size of errmsg is:%d\n", strlen(errmsg2));
            memset(err, 0, 200);
            memcpy(err, (const char *)&opcode2, sizeof(uint16_t));
            memcpy(err+sizeof(uint16_t), (const char *)&errcode, sizeof(uint16_t));
            memcpy(err+(2*sizeof(uint16_t)), (const char *)errmsg2, sizeof(errmsg));
            memcpy(err+(2*sizeof(uint16_t))+sizeof(errmsg2), (const char *)errmsg2, sizeof(errmsg));
            if ((numbytes = sendto(newservfd, err, 30, 0, (struct sockaddr *)&their_addr, p->ai_addrlen)) == -1) {
              perror("tftpserver1: send ERROR packet");
            } else {
              printf("tftpserver1: ERROR packet sent successfully\n");
              printf("size of error packet is %d\n", numbytes);
            }
            break;
          }
          // split the file in blocks and send each block
          while( !feof(fp) && !ferror(fp)) {
            unsigned char msg[512] = "";
            j++;
            for( i=0; i<=511; i++) {
              if (( data = fgetc(fp) ) == EOF) {
                break;
              } else {
                msg[i] =(unsigned char) data;
              }
            }
            blockno = htons(j); 
            memcpy(buf+sizeof(uint16_t), (const char *)&blockno, sizeof(uint16_t));
            memcpy(buf+(2*sizeof(uint16_t)), (const unsigned char *)msg, sizeof(msg));
            if ((numbytes = sendto(newservfd, buf, i+4, 0, (struct sockaddr *)&their_addr, p->ai_addrlen)) == -1) {
              perror("tftpserver1: sendto");
              exit(4);
            }
            printf("talker: sent %d bytes\n", numbytes);
            printf(" waiting for ack\n");
            // wait for ACK.....
            while(1) {
              struct timeval tv;  // implementing timeout
              fd_set readfds;
              tv.tv_sec = 10;
              tv.tv_usec = 1000000;
              FD_ZERO(&readfds);  // clear the file descriptor list
              FD_SET(newservfd, &readfds);  // add the new server file descriptor to the list
              select(newservfd+1, &readfds, NULL, NULL, &tv);  // select to start the timing
              if (FD_ISSET(newservfd, &readfds)) {  // did not time out
                if ((numbytes = recvfrom(newservfd, bufr, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                  perror("recvfrom");
                  exit(5);
                }
                uint16_t bl;  // block number
                memcpy(&bl,bufr+2, sizeof(uint16_t));
                printf("opcode: %d\nblockno: %d\nj: %d\n", *(bufr+1), ntohs(bl), j);
                if( *(bufr+1) == 4 && ntohs(bl)==j) {
                  break;
                } else {
                  // if get the incorrect ack, then retransmission
                  if ((numbytes = sendto(newservfd, buf, i+4, 0, (struct sockaddr *)&their_addr, p->ai_addrlen)) == -1) {
                    perror("tftpserver1: sendto");
                    exit(6);
                  }
                }
              } else {  // time out
                // retransmission
                if ((numbytes = sendto(newservfd, buf, i+4, 0, (struct sockaddr *)&their_addr, p->ai_addrlen)) == -1) {
                  perror("tftpserver1: sendto");
                  exit(7);
                }
              }
            } 
          }
          close(newservfd);  // when done with the transmission, close the file descriptor immediately
          if(feof(fp)){
            printf("reached the end of file\n");
          }
          if(ferror(fp))
            printf("read error occured\n");
        }
        fclose(fp);
      }  // done with the file manipulation
    }  // done with the transmission
  }  // end of while loop
return 0;
}
