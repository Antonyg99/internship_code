/*
 * Simple TCP client/server -- tests network stack using loopback address
 * 
 */
#include <stdio.h>		/* printf */
#include <stdlib.h> 		/* EXIT_FAILURE & EXIT_SUCCESS */
#include <string.h>		/* memset */
#include <arpa/inet.h>		/* htons & inet_addr */
#include <sys/socket.h>		/* socket calls */
#include <unistd.h>		/* close */
#include <errno.h>

#include "defs.h"
#include "msg.c"
#define MAXBUF 1600
static uint8_t msgbuf[MAXBUF];

int main(int argc, char **argv) {
  int sock,client_sock;		/*  listening socket */
  struct sockaddr_in servaddr;  /*  socket address structure */
  uint64_t *bp,*bp1,*buffer,*buffer1;
  size_t i,j,msgsize,msgsizebytes;
  int res;
  char ans[2];
	
	uint16_t port = TCP_ECHO_PORT;
	if(argc>1)
		port=atoi(argv[1]);
	printf("Using port %d\n",(int)port);

  /* set up the server address */
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port   = htons(port);
  if(inet_aton(NEW_ADDR,&(servaddr.sin_addr)) <= 0)
    errorExit("CLIENT: Error on inet_pton\n");
  /* send and recieve NMSGS */
    
    while(1) {
        if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
			errorExit("CLIENT: Error creating listening socket.\n");
			/* connect to the server */
		if((res = connect(sock,(struct sockaddr *) &servaddr, sizeof(servaddr))) < 0) {
            printf("CLIENT: Error calling connect (%s)\n",strerror(errno));
            exit(EXIT_FAILURE);
		}

        /* send AOZ/EOZ message */
        char cmd[2];
        printf("\nCommand Target/AOZ (1/2): ");
		fflush(stdout);
		scanf("%s",cmd);
        size_t len;
        /* Target message */
        if (*cmd == '1'){
            // option1(sock);
            len = msgmake1(msgbuf);
        }
        /* AOZ/EZ */
        else if(*cmd == '2'){
            len = msgmake2(msgbuf);

        }
        printf("(len is %d)\n",(int)len);
        msgprint("sending",msgbuf,len);
        ssize_t nsent;
        /* send */
        if((nsent = send(sock,(void*)msgbuf,len,0))<0){
            errorExit("Error on send\n");
        }

        uint8_t resp[R_SIZE];
        /* receive response */
        ssize_t nrecv;
        if((nrecv = recv(sock,(void*)resp,R_SIZE,0))<0){
            errorExit("Error on send\n");
        }
        msgprint("recv h",resp,R_SIZE);

        


        printf("\nContinue? (y/n): ");
		fflush(stdout);
		scanf("%s",ans);
		if(*ans=='n')
			exit(EXIT_SUCCESS);
    }
}