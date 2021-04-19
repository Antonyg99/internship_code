/* 
 * msg.h -- 
 * 
 * Author: Antony Guzman
 * Created: 01-2-2020
 * Version: 1.0
 * 
 */
#include <stdio.h>							/* printf */
#include <stdlib.h>							/* exit codes */
#include <stdint.h>							/* uint8_t */
#include <sys/types.h>					/* open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>							/* close */
#include "hw.h"	

#define TARGET 0x30							/* command C3, target type */
#define AOZ 0x10							/* command C3, target type */
#define EZ 0x20							/* command C3, target type */
#define R_SIZE 2 
static uint8_t id=0;						/* msgid rolls over at 255 */

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



int msgmake1(uint8_t *bp) {				
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

int msgmake2(uint8_t *bp) {				
    if((id%2)!=0)
        *bp++ = AOZ;
    else						/* build a target message */
        *bp++= EZ;
	*bp++ = -64;							/* C0 */
	*bp++ = 0x10;
	*bp++= 0x20;
	*bp++ = 180;						/* 180 = B4 00 (Intel=little endian) */
	*bp++ = 0x00;
	*bp++ = 0x30;
	*bp++ = 0x40;
    *bp++ = -64;							/* C0 */
	*bp++ = 0x10;
	*bp++ = 0x20;
	*bp++ = 180;						/* 180 = B4 00 (Intel=little endian) */
	*bp++ = 0x00;
	*bp++ = 0x30;
	*bp++ = 0x40;
	*bp++ = id++;	 /* every message has unique id */

	return 16;
}

void msgprint(char *tag,uint8_t *bp,int len) {
	int i;

	printf("%s: ",tag);
	for(i=0;i<len;i++) {
		printf("%02x ",*bp++);
		fflush(stdout);
	}
	printf("(len=%d)\n",len);
}
