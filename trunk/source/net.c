/************************************************
* Download code by nicksasa                     *
* Licensed under the GNU 2                      *
* Free to use, but give some credit             *
*************************************************/




#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <network.h>
#include <sdcard/wiisd_io.h>


#define PROCENT              (u32)( (double)cnt/len*100)
#define NETWORK_PORT         80   
#define BLOCKSIZE            2048

static char hostip[16] ATTRIBUTE_ALIGN(32);
static u8 titleBuf[2048] ATTRIBUTE_ALIGN(32);
//char NETWORK_PATH[1024];


/* Network variables */
static s32 sockfd = -1;


char *network_getip(void)
{
	
	return hostip;
}
s32 network_init(void)
{
	s32 ret;

	
	ret = if_config(hostip, NULL, NULL, true);
	if (ret < 0)
		return ret;

	return 0;
}
s32 network_connect(char NETWORK_HOSTNAME[1024])
{
	struct hostent *he;
	struct sockaddr_in sa;

	s32 ret;


	if (sockfd >= 0)
		net_close(sockfd);

	sockfd = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (sockfd < 0)
		return sockfd;


	he = net_gethostbyname(NETWORK_HOSTNAME);
	if (!he)
		return -1;


	memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(NETWORK_PORT);

	ret = net_connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0)
		return ret;

	return 0;
}
s32 network_request(const char *filepath, char NETWORK_PATH[1024], char NETWORK_HOSTNAME2[1024])
{
	char buf[1024], request[256];
	char *ptr = NULL;

	u32 cnt, size;
	s32 ret;



	char *r = request;
	r += sprintf (r, "GET %s HTTP/1.1\r\n", NETWORK_PATH);
	r += sprintf (r, "Host: %s\r\n", NETWORK_HOSTNAME2);
	r += sprintf (r, "Cache-Control: no-cache\r\n\r\n");
	
	printf("%s\n", request);


	ret = network_connect(NETWORK_HOSTNAME2);
	if (ret < 0)
		return ret;

	ret = net_send(sockfd, request, strlen(request), 0);
	if (ret < 0)
		return ret;


	memset(buf, 0, sizeof(buf));


	for (cnt = 0; !strstr(buf, "\r\n\r\n"); cnt++)
		if (net_recv(sockfd, buf + cnt, 1, 0) <= 0)
			return -1;


	if (!strstr(buf, "HTTP/1.1 200 OK"))
		return -1;


	ptr = strstr(buf, "Content-Length:");
	if (!ptr)
		return -1;

	sscanf(ptr, "Content-Length: %u", &size);
	printf("Conent-Length: %d\n", size);

	return size;
}

s32 network_read(void *buf, u32 len)
{
	s32 read = 0, ret;


	while (read < len) {

		ret = net_read(sockfd, buf + read, len - read);
		if (ret < 0)
			return ret;

	
		if (!ret)
			break;

		
		read += ret;
	}

	return read;
}
s32 ReadNetwork(char filename[1024], FILE *file, u32 *length, char lolo[1024], char NETWORK_HOSTNAME3[1024])
{
	char netpath[ISFS_MAXPATH];

	u32 cnt, len;
	s32 ret;


	sprintf(netpath, "%s", filename);


	len = network_request(netpath, lolo, NETWORK_HOSTNAME3);
	if (len < 0)
		return len;


	printf("\n");
time_t nop;
char TLine3[1024];
char speed2[1024];
time_t lol;
lol = time(0);
struct tm to;

	for (cnt = 0; cnt < len; cnt += BLOCKSIZE) {
		u32 blksize;

		blksize = (len - cnt);
		if (blksize > BLOCKSIZE)
			blksize = BLOCKSIZE;
			
		if(blksize < BLOCKSIZE)
            cnt += blksize;	
			
		
		nop = time(0);
to = *localtime(&nop);
sprintf(TLine3,"[%i:%i:%i]",to.tm_hour,to.tm_min,to.tm_sec);
sprintf(speed2, "%ld", (cnt / 1000)/(nop - lol));	

printf("Procent %d , bytes = %d, downloading @ %s KB/s\r", PROCENT, cnt, speed2);
fflush(stdout);



		ret = network_read(titleBuf, blksize);
		if (ret != blksize) {
			ret = -1;
			goto out;
		}


		ret = fwrite(titleBuf, 1, blksize, file);
		if (ret != blksize) {
			ret = -1;
			printf("\n");
			
			
		}
		*length = len;
if(cnt == len) {
break;
}
	}


	ret = len;

out:

	if (file >= 0)
		fclose(file);

	return ret;
}


