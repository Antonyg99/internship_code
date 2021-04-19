/* 
 * sr.c -- send and recieve to hardware
 *
 * Description: This user-space Linuxprogram demonstrates how to send
 * data to the hardware interface and recieve data back from the
 * hardware.
 * 
 */
#include <stdio.h>							/* printf */
#include <stdlib.h>							/* exit codes */
#include <stdint.h>							/* uint8_t */
#include <sys/types.h>					    /* open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>							/* close */
#include "hw.h"									/* hwread, hwwrite */

/* largest message to send to hardware */
#define MAXBUF 1500
static uint8_t msgbuf[MAXBUF];			/* a message buffer */

typedef struct c1 {							/* target message */
	uint8_t type;
	int8_t lat_deg;
	uint8_t lat_min;
	uint8_t lat_sec;
	int16_t long_deg;
	uint8_t long_min;
	uint8_t long_sec;
	uint8_t weapon;
	uint8_t msgid;
} c1_t;

/*
 * msgmake() -- builds a message alternating between weapon 1 and 2
 * with message id starting at 1
 */
#define TARGET 0x20							/* command C3, target type */
static uint8_t id=0;						/* msgid rolls over at 255 */

int msgmake(uint8_t *bp) {				
	c1_t *p=(c1_t*)bp;						/* build a target message */
	p->type = TARGET;
	p->lat_deg = -64;							/* C0 */
	p->lat_min = 0x10;
	p->lat_sec = 0x20;
	p->long_deg = 180;						/* 180 = B4 00 (Intel=little endian) */
	p->long_min = 0x30;
	p->long_sec = 0x40;
	p->msgid = id++;	 /* every message has unique id */
	if((id%2)!=0)			 /* alternatve weapons based on msgid */
		p->weapon = 1;
	else
		p->weapon = 2;
	return sizeof(c1_t);
}

/*
 * printms() -- prints a message on the screen in hex
 */
void msgprint(char *tag,uint8_t *bp,int len) {
	int i;

	printf("%s: ",tag);
	for(i=0;i<len;i++) {
		printf("%02x ",*bp++);
		fflush(stdout);
	}
	printf("(len=%d)\n",len);
}

int main(void) {
	int fdout,fdin,i;
	size_t len;
	ssize_t cnt;

	fdout = open(DEVOUT,O_WRONLY);				/* open the hardware for read and write */
	fdin = open(DEVIN,O_RDONLY);				/* open the hardware for read and write */

	for(i=0; i<10; i++) {					/* send and recieve 10 messages */
		len = msgmake(msgbuf);			/* make ith c1 message */
		msgprint("send",msgbuf,len);	/* print recvd msg */
		hwwrite(fdout,(void*)msgbuf,len);				/* send ith message to hardware */
		while((cnt=hwread(fdin,(void*)msgbuf,MAXBUF))==0) { /* wait for a message */
			printf(".");											 /* print a dot for debug */
			fflush(stdout);
		}
		printf("\n");
		msgprint("recv",msgbuf,len);	/* print recvd msg */
	}
	close(fdout);
	close(fdin);
	exit(EXIT_SUCCESS);
}