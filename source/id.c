/*-------------------------------------------------------------
 
id.c -- ES Identification code
 
Copyright (C) 2008 tona
Unless other credit specified
 
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.
 
Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:
 
1.The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.
 
2.Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.
 
3.This notice may not be removed or altered from any source
distribution.
 
-------------------------------------------------------------*/

#include <stdio.h>
#include <gccore.h>

#include "../build/certs_dat.h"
#include "../build/fake_su_tmd_dat.h"
#include "../build/fake_su_ticket_dat.h"


/* Debug functions adapted from libogc's es.c */
//#define DEBUG_ES
//#define DEBUG_IDENT
#define ISALIGNED(x) ((((u32)x)&0x1F)==0)



/* Reads a file from ISFS to an array in memory */
/*
s32 ISFS_ReadFileToArray (char *filepath, u8 *filearray, u32 max_size, u32 *file_size) {
	s32 ret, fd;
	static fstats filestats ATTRIBUTE_ALIGN(32);
	ret = ISFS_Open(filepath, ISFS_OPEN_READ);
	if (ret <= 0)
	{
		printf("Error! ISFS_Open (ret = %d)\n", ret);
		return -1;
	}
	
	fd = ret;
	
	ret = ISFS_GetFileStats(fd, &filestats);
	if (ret < 0)
	{
		printf("Error! ISFS_GetFileStats (ret = %d)\n", ret);
		return -1;
	}
	
	*file_size = filestats.file_length;
	
	if (*file_size > max_size)
	{
		printf("File is too large! Size: %u", *file_size);
		return -1;
	}
	
	ret = ISFS_Read(fd, filearray, *file_size);
	if (ret < 0)
	{
		printf("Error! ISFS_Read (ret = %d)\n", ret);
		return -1;
	}
	
	ret = ISFS_Close(fd);
	if (ret < 0)
	{
		printf("Error! ISFS_Close (ret = %d)\n", ret);
		return -1;
	}
	return 0;
}
*/
s32 Identify(const u8 *certs, u32 certs_size, const u8 *idtmd, u32 idtmd_size, const u8 *idticket, u32 idticket_size) {
	s32 ret;
	u32 keyid = 0;
	ret = ES_Identify((signed_blob*)certs, certs_size, (signed_blob*)idtmd, idtmd_size, (signed_blob*)idticket, idticket_size, &keyid);
	if (ret < 0)
	{
		printf("\n");
		switch(ret){
			case ES_EINVAL:
				printf("Error! ES_Identify (ret = %d;) Data invalid!\n", ret);
				break;
			case ES_EALIGN:
				printf("Error! ES_Identify (ret = %d;) Data not aligned!\n", ret);
				break;
			case ES_ENOTINIT:
				printf("Error! ES_Identify (ret = %d;) ES not initialized!\n", ret);
				break;
			case ES_ENOMEM:
				printf("Error! ES_Identify (ret = %d;) No memory!\n", ret);
				break;
			default:
				printf("Error! ES_Identify (ret = %d)\n", ret);
				break;
		}
		if (!ISALIGNED(certs)) printf("\tCertificate data is not aligned!\n");
		if (!ISALIGNED(idtmd)) printf("\tTMD data is not aligned!\n");
		if (!ISALIGNED(idticket)) printf("\tTicket data is not aligned!\n");
	}
	return ret;
}



s32 Identify_SU(void)
{
	return Identify(certs_dat, certs_dat_size, fake_su_tmd_dat, fake_su_tmd_dat_size, fake_su_ticket_dat, fake_su_ticket_dat_size);
}
/*
s32 Identify_SysMenu(void)
{
	s32 ret;
	u32 sysmenu_tmd_size, sysmenu_ticket_size;
	static u8 sysmenu_tmd[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
	static u8 sysmenu_ticket[STD_SIGNED_TIK_SIZE] ATTRIBUTE_ALIGN(32);
	
	printf("\n\tPulling Sysmenu TMD...");
	ret = ISFS_ReadFileToArray ("/title/00000001/00000002/content/title.tmd", sysmenu_tmd, MAX_SIGNED_TMD_SIZE, &sysmenu_tmd_size);
	if (ret < 0) {
		printf("\tReading TMD failed!\n");
		return -1;
	}
	
	printf("\n\tPulling Sysmenu Ticket...");
	ret = ISFS_ReadFileToArray ("/ticket/00000001/00000002.tik", sysmenu_ticket, STD_SIGNED_TIK_SIZE, &sysmenu_ticket_size);
	if (ret < 0) {
		printf("\tReading TMD failed!\n");
		return -1;
	}
	
	printf("\n\tInforming the Wii that I am its daddy...");
	fflush(stdout);
	return Identify(certs_dat, certs_dat_size, sysmenu_tmd, sysmenu_tmd_size, sysmenu_ticket, sysmenu_ticket_size);
}
*/
