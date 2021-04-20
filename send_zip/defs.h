#define NMSGS 100			/* number of messages */
#define MSGSZ (1000)	/* sizeof each message MSGSZ*sizeof(unit64_t) */
// 127.0.0.1
#define SERV_ADDR "129.170.210.25"

#define TCP_ECHO_PORT 9000

#define LISTENQ (16)
#define errorExit(str) { printf(str); exit(EXIT_FAILURE); }
