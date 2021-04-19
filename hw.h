/* 
 * hw.h --- the initial device driver for hardware
 * 
 * Author: Stephen Taylor
 * Created: 12-21-2020
 * Version: 1.0
 * 
 * Description: This file provides the prototypes for reading and
 * writing data into the hardware. It follows the convential read and
 * write interface but initially assumes there is just a single piece
 * of hardware to work with.
 * 
 */
#define DEVOUT "/dev/null"				/* currently unused */
#define DEVIN "/dev/null"				/* currently unused */

/* 
 * hwread() -- Non-blocking read: attempts to read upto count bytes
 *   from hardware file descriptor fd into the buffer buf.
 * 
 * returns: number of bytes read; 0 if no bytes read.
 */
ssize_t hwread(int fd,void *buf, size_t count);

/* 
 * hwwrite() -- Blocking write: writes count bytes from buf into
 * hardware file descriptor fd. 
 *
 * returns: number of bytes written; -1 on error.
 */
ssize_t hwwrite(int fd,const void *buf, size_t count);


/* 
 * hwresponse() -- Non-blocking response: attempts to response 
 *   from hardware file descriptor fd into the buffer buf.
 * 
 * returns: number of bytes read; 0 if no bytes read.
 */
ssize_t hwresponse(int fd,void *buf, size_t count);

