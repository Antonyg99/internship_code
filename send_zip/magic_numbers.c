/* 
 * Server with hardware and connection to the server
 * 
*/

#include <stdio.h>		/* printf */
#include <stdlib.h> 		/* EXIT_FAILURE & EXIT_SUCCESS */
#include <string.h>		/* memset */
#include <arpa/inet.h>		/* htons & inet_addr */
#include <sys/socket.h>		/* socket calls */
#include <unistd.h>		/* close */
#include <errno.h>
#include <sys/types.h>					/* open */
#include <sys/stat.h>
#include <fcntl.h>
#include "hw.h"
#include "defs.h"
#include "msg.c"

/* largest message to send to hardware */
#define MAXBUF  1500
#define RSIZE   2
#define TSIZE   10
#define A_ESIZE 16
#define TARGET  0x30
#define AOZ     0x10
#define EZ      0x20

static uint8_t msgbuf[MAXBUF];			/* a message buffer */     /* table of EZ */




int main(int argc, char **argv){
    int sock,client_sock;

	int yes =1;
	
	struct sockaddr_in servaddr;

	uint16_t port = TCP_ECHO_PORT;

	/* Create a TCP socket */
	if ((sock = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP)) < 0){
		errorExit("SERVER: Error creating listening socket.\n");
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    	errorExit("SERVER: Setsockopt\n");
	}

	/* set up the server address */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ){
		errorExit("SERVER: Error calling bind\n");
	}

	printf("[Listening...]\n");

	if (listen(sock, LISTENQ) < 0 ){
   		errorExit("SERVER: Error calling listen\n");
	}

    int fdout = open(DEVOUT,O_WRONLY);				/* open the hardware for read and write */
	int fdin = open(DEVIN,O_RDONLY);				/* open the hardware for read and write */

    while (1){
        client_sock = accept(sock,NULL,NULL) ;
        if(client_sock < 0 ) {
				errorExit("SERVER: Error calling accept\n");
        }

        ssize_t nrecv;
        if ((nrecv = recv(client_sock,(void*)msgbuf,A_ESIZE,0)) < 0){
            errorExit("Error on recv\n");
        }
        int size = A_ESIZE;
        if (msgbuf[0] == TARGET){
            size = TSIZE;
        }
        msgprint("send hw",msgbuf,size);

        hwwrite(fdout,(void*)msgbuf,TSIZE);
        ssize_t cnt;
        
        
        while((cnt=hwresponse(fdin,(void*)msgbuf,MAXBUF))==0) { /* wait for a message */
            printf(".");														 /* print a dot for debug */
            fflush(stdout);
        }				/* send ith message to hardware */
        printf("\n");
        msgprint("recv h",msgbuf,cnt);
        

        ssize_t nsent;
        if ((nsent = send(client_sock,(void*)msgbuf,RSIZE,0)) < 0){
            errorExit("Error on recv\n");
            msgprint("sending again",msgbuf,cnt);
        }


    }
    if(close(sock) <0){
		errorExit("Error closing socket\n");
	}
}
