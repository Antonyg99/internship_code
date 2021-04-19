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

static uint8_t msgbuf[MAXBUF];			/* a message buffer */

static uint8_t AOZtable[MAXBUF][4];         /* table of AOZ */

static uint8_t EZtable[MAXBUF][4];         /* table of EZ */

static int AOZ_index;
static int EZ_index;



/* If message AOZ, add to AOZ table
   if message EZ, add to EZ table
   if message Target, check that it's wihtin 
   AOZ and outside of EZ
   Output:
   if AOZ, return 2
   if EZ, return 2
   if valid Target, return 0
   else -1
*/
int checkTables(int size){
    int ret = 0;
    int in_aoz = 1;                 /* assumes that no aoz contains target */
    int out_ez = 0;                 /* assumes that all ez dont contain target */
    if (size == A_ESIZE){
        /* add aoz to table */
        uint8_t  first_long[4];
        uint8_t  first_lat[3];
        uint8_t  second_long[4];
        uint8_t  second_lat[3];
        if (msgbuf[0]==AOZ || msgbuf[0]==EZ) {
            /* get first long coord */
            first_long[0] = msgbuf[1];
            first_long[1] = msgbuf[2];
            first_long[2] = msgbuf[3];
            first_long[3] = msgbuf[4];

            /* get first lat coord */
            first_lat[0] = msgbuf[5];
            first_lat[1] = msgbuf[6];
            first_lat[2] = msgbuf[7];

            /* get second long coord */
            second_long[0] = msgbuf[8];
            second_long[1] = msgbuf[9];
            second_long[2] = msgbuf[10];
            second_long[3] = msgbuf[11];

            /* get second lat coord */
            second_lat[0] = msgbuf[12];
            second_lat[1] = msgbuf[13];
            second_lat[2] = msgbuf[14];            

            /* insert into appropiate table */
            if (msgbuf[0]==AOZ){

                
                AOZtable[AOZ_index][0] = first_long;
                AOZtable[AOZ_index][1] = first_lat;
                AOZtable[AOZ_index][2] = second_long;
                AOZtable[AOZ_index][3] = second_lat;
                AOZ_index ++;
            }
            else{
                EZtable[AOZ_index][0] = first_long;
                EZtable[AOZ_index][1] = first_lat;
                EZtable[AOZ_index][2] = second_long;
                EZtable[AOZ_index][3] = second_lat;
                EZ_index ++;
            }
        }
        ret = 2;
    }
    else{
        /* go through AZ and check if in 1 of them at least */
        for (int i =0; i <AOZ_index; i++){
            /* check if within aoz */
            if (AOZtable[i][0] % 2== 0) in_aoz = 0;
        }
        /* check EZ and if not within */
        for(int i = 0; i <EZ_index;i++){
            if (EZtable[i][0] % 2== 0) out_ez = 1;
        }

    }
    if (in_aoz == 0 && out_ez == 0) ret = 0;
    else  ret = -1;

    return ret;


}

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

    AOZ_index = 0;
    EZ_index = 0;
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

        int ret= checkTables(size);
        if (ret == 0) continue;
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
