/* 
 * hw.c --- simulates the fifo
 * 
 * Author: Stephen Taylor
 * Created: 12-21-2020
 * Version: 1.0
 * 
 * Description: grabs data from the input and sends to the output
 * NOTE: 10 class to read before the data is returned to simulate 
 * return without data
 * 
 */
#include <stdint.h>
#include <unistd.h>
#include <hw.h>

int cnt = 0;										/* number of returns without data */

#define MAX 2000								/* the fifo size */
uint8_t hw[MAX];								/* big enough hardware buffer */
size_t hwlen;
#define RES_S 2


ssize_t hwread(int fd,void *buf, size_t count) {
	uint8_t *bp;
	int i;
	
	cnt++;
	if((cnt%10)!=0) 							/* return with nothing 10 times */
		return 0;
	for(bp=(uint8_t*)buf,i=0; i<hwlen; i++) /* otherwise */
		*bp++ = hw[i];							/* return the data */
	return hwlen;			  						/* and its length */
}

ssize_t hwwrite(int fd,const void *buf, size_t count) {
	uint8_t *bp;
	int i;

	for(bp=(uint8_t*)buf,i=0; i<count; i++) /* copy data to hardware */
		hw[i] = *bp++;												
	hwlen = count;								/* record its length */
	return count;									/* say we took count bytes */
}


static uint8_t tar_type = 0x40;
ssize_t hwresponse(int fd,void *buf, size_t count) {
	uint8_t *bp;
	
	cnt++;
	if((cnt%10)!=0) 							/* return with nothing 10 times */
		return 0;

	hwlen =2;

	bp = (uint8_t*)buf;
					/* return iterative response*/
	if (hw[0] == 0x10 || hw[0] == 0x20 ){

		/* randomly set the response message 
		if aoz/ex respond with ACK/NAK */
		int n = tar_type / 0x10;
		if (n %0x2 != 0){
			*bp++ = 0x81;
		}
		else{
			*bp++ = 0x80;
		}
		*bp++ = hw[15];						/* get the message ID */

	}
	else if(hw[0] == 0x30){
		*bp++ = tar_type;
		*bp++ = hw[9];						/* get the message ID */
	}

	if (tar_type == 0x70) {
		tar_type = 0x40;
	}
	else{
		tar_type += 0x10;
	}
	return hwlen;			  						/* and its length */
}



