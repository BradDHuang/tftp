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

#define BUFFER_LENGTH 100

/*
 * Main function
 */
int timeout_count = 0;

int main(int argc, char *argv[]) {
  struct addrinfo addr_index, *server_info, *p;
  int rv, sockfd, clientfd;  // server socket descriptor
  int numbytes;  // the length of sent or received messages in bytes
  char *sending_buffer, *error_buffer;  // pointer to the messages
  char received_buffer[BUFFER_LENGTH];  // messages that are received
  struct sockaddr_storage client_addr;  // store the address information of clients' socket
  socklen_t addr_len; 
  uint16_t opcode, opcode_FileError, blockno, errorcode_FileError;  // header fields of tftp
  uint8_t split;
  split = htons(0);
  int count = 0;

  if (argc != 3) {
    printf("argument number wrong\n");// checking for input arguments.
    exit(1);
  }
  memset(&addr_index, 0, sizeof addr_index);// setting zeroes to struct addr_index
  addr_index.ai_family = AF_UNSPEC;
  addr_index.ai_socktype = SOCK_DGRAM;
  if ((rv = getaddrinfo((const char *)argv[1], (const char *)argv[2], &addr_index, &server_info)) != 0) {
    //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  // loop through all the results and make a socket
  for(p = server_info; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
    //perror("tftpserver1: socket");
    continue;
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      //perror("tftpserver1: bind");
      continue;
    }
    break;
  }
  if (p == NULL) {
    //fprintf(stderr, "tftpserver1: failed to bind socket\n");
    return 2;
  }
  // infinite loop to create stand server
  while(1) {
      printf("The server is waiting for the client request\n");
      addr_len = sizeof client_addr;
      if ((numbytes = recvfrom(sockfd, received_buffer, BUFFER_LENGTH-1 , 0, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
      //perror("tftpserver: fail to receive packets from client-1");
          exit(2);
      }
      if (numbytes > 4){
    //if ( *(received_buffer+1) == 1) { //then its an RRQ hence fork. else dont.
          count = count + 1;
          //printf("The block number is %d \n", count);
          FILE *fp ;
          char *mode = "rb";
          fp = fopen ( received_buffer+2, mode);
          error_buffer = malloc(200);
          char errmsg[14] = "file not found";
          if ( fp == NULL ) {
        // File not found Error
              if (errno == ENOENT) {
                  printf("Error!! File not found\n");
          // send an error message
                  opcode_FileError = htons(5);
                  errorcode_FileError = htons(1);
                  memcpy(error_buffer, (const char *)&opcode_FileError, sizeof(uint16_t));
                  memcpy(error_buffer+sizeof(uint16_t), (const char *)&errorcode_FileError, sizeof(uint16_t));
                  memcpy(error_buffer+(2*sizeof(uint16_t)), (const char *)errmsg, sizeof(errmsg));
                  memcpy(error_buffer+(2*sizeof(uint16_t))+sizeof(errmsg), (const char *)&split, sizeof(uint8_t));
                  if ((numbytes = sendto(sockfd, error_buffer, 19, 0, (struct sockaddr *)&client_addr, p->ai_addrlen)) == -1) {
            //perror("tftpserver: fail to send ERROR packet");
                  } else {
                      printf("The server has sent FILE NOT FOUND message\n");
                  }
              }
          } else {
        // if file exists, fork to accept multiple client request concurrently
              pid_t pid = fork();
              if (pid == 0) {  // in child process
                  printf("This is child process\n");
                  int a,b,c;
                  char z[5] = "";
                  a = atoi(argv[2]);
                  c = a/1000;
                  b = a%1000;
                  sprintf(z,"%d%d", c,b+count);  // random port
                  printf("New port number for the client is: %s\n", z);
          // create a new UDP socket with a new port number and bind it to the server
                  if ((rv = getaddrinfo((const char *)argv[1], (const char *)z , &addr_index, &server_info)) != 0) {
                      //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                      return 1;
                  }

                  for(p = server_info; p != NULL; p = p->ai_next) {
                      if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                          //perror("tftpserver1: socket");
                          continue;
                      }
                      if (bind(clientfd, p->ai_addr, p->ai_addrlen) == -1) {
                          close(clientfd);
                          //perror("tftpserver1: bind");
                          continue;
                      }
                      break;
                  }
                  if (p == NULL) {
                      //fprintf(stderr, "tftpserver1: failed to bind socket\n");
                      return 2;
                  }
                  // read the file and send data
                  opcode =htons(3);
                  sending_buffer = malloc(516);
                  memcpy(sending_buffer, (const char *)&opcode, sizeof(uint16_t));
                  int i,j;
                  int data = 0;
                  j = 0;
                  //long fileSize;
                  //fseek(fp, 0L, SEEK_END);  // go to end of file
                  //fileSize = ftell(fp);  // obtain current position is the file length
                  //rewind(fp);  // rewind to beginning
                  //clearerr(fp);  // reset the flags
                  //printf("file size is : %ld\n", fileSize);

                  // split the file in blocks and send each block
                  while( !feof(fp) && !ferror(fp)) {
                      unsigned char msg[512] = "";
                      j++;//block number
                      for( i=0; i<=511; i++) {
                          if (( data = fgetc(fp) ) == EOF) {
                              break;
                          } else {
                              msg[i] =(unsigned char) data;
                          }
                      }
                      blockno = htons(j);
                      memcpy(sending_buffer+sizeof(uint16_t), (const char *)&blockno, sizeof(uint16_t));
                      memcpy(sending_buffer+(2*sizeof(uint16_t)), (const unsigned char *)msg, sizeof(msg));
                      if ((numbytes = sendto(clientfd, sending_buffer, i+4, 0, (struct sockaddr *)&client_addr, p->ai_addrlen)) == -1) {
                          //perror("tftpserver1: sendto");
                          exit(4);
                      }
                      //printf("talker: sent %d bytes\n", numbytes);
                      printf(" waiting for ACK message from clients\n");
                      // wait for ACK.....
                      while(1) {
                          struct timeval tv;  // implementing timeout
                          fd_set readfds;
                          tv.tv_sec = 1;
                          tv.tv_usec = 0;
                          FD_ZERO(&readfds);  // clear the file descriptor list
                          FD_SET(clientfd, &readfds);  // add the new server file descriptor to the list
                          select(clientfd+1, &readfds, NULL, NULL, &tv);  // select to start the timing
                          if (FD_ISSET(clientfd, &readfds)) {  // did not time out
                              if ((numbytes = recvfrom(clientfd, received_buffer, BUFFER_LENGTH-1 , 0, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
                  //perror("recvfrom");
                                  exit(5);
                              }
                              uint16_t bl;  // block number
                              memcpy(&bl,received_buffer+2, sizeof(uint16_t));
                              printf("opcode: %d\nblockno: %d\nj: %d\n", *(received_buffer+1), ntohs(bl), j);
                              //if(feof(fp)){
                              if(*(received_buffer+1)==4){
                              //if( *(received_buffer+1) == 4 && ntohs(bl)==j) {
                                  break;
                              }
                          } else {  // time out
                              // retransmission
                              timeout_count++;
                              printf("The count of timeout is %d\n",timeout_count);
                              if (timeout_count>10){
                                  printf("The timeout count is over 10 times, the server close the file stream and child socket.\n");
                                  printf("The server is waiting for next client\n");
                                  exit(7);
                              }
                              if ((numbytes = sendto(clientfd, sending_buffer, i+4, 0, (struct sockaddr *)&client_addr, p->ai_addrlen)) == -1) {
                                  //perror("tftpserver1: sendto");
                                  exit(7);
                              }
                          }
                      }
                  }//end of ack while loop
          close(clientfd);  //
          if(feof(fp)){
            printf("tranfser complete \n");
          }
          //if(ferror(fp))
            //printf("read error occured\n");
        }//end of child process
        fclose(fp);
      }  // done with the file manipulation
    }  // done with the transmission
  }  // end of while loop
return 0;
}
