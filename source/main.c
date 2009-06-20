#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/isfs.h>
#include <ogc/ipc.h>
#include <ogc/ios.h>
#include <wiiuse/wpad.h>
#include <ogc/wiilaunch.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <fat.h>
#include <dirent.h>
#include <sdcard/wiisd_io.h>

#include "id.h"
#include "sha1.h"
#include "net.h"

#define BLOCKSIZE 2048

#define DIRENT_T_FILE 0
#define DIRENT_T_DIR 1

#define INIT_FIRST 1
#define INIT_RESET 0
//Network Stuff

#define NETWORK_HOSTNAME	"nicksasa.x10hosting.com"
#define link                "/FileFlasher.dol"
//The filename of the sd file thats gonna be written


//Types.

typedef struct _dirent
{
	char name[ISFS_MAXPATH + 1];
	int type;
	u32 ownerID;
	u16 groupID;
	u8 ownerperm;
	u8 groupperm;
	u8 otherperm;
} dirent_t;

typedef struct _dir
{
	char name[ISFS_MAXPATH + 1];
} dir_t;

typedef struct _list
{
	char name[ISFS_MAXPATH + 1];

} list_t;

/* Video pointers */
static GXRModeObj *rmode = NULL;
u32 *xfb;
bool Power_Flag;
bool Reset_Flag;

// Prevent IOS36 loading at startup
s32 __IOS_LoadStartupIOS()
{
	return 0;
}

static void power_cb () 
{
	Power_Flag = true;
}

static void reset_cb () 
{
	Reset_Flag = true;
}

void Verify_Flags()
{
	if (Power_Flag)
	{
		STM_ShutdownToStandby();
	}
	if (Reset_Flag)
	{
		STM_RebootSystem();
	}
}

void Reboot()
{
	if (*(u32*)0x80001800) exit(0);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

s32 waitforbuttonpress()
{
	s32 pressed;
	while (true)
	{
		Verify_Flags();
		WPAD_ScanPads();
		pressed = WPAD_ButtonsDown(0);
		if(pressed) 
		{
			return pressed;
		}
	}
}

static void sys_init(void)
{
	// Initialise the video system
	VIDEO_Init();
	
	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);
	
	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);
	
	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	// Set console parameters
    int x = 24, y = 32, w, h;
    w = rmode->fbWidth - (32);
    h = rmode->xfbHeight - (48);

    // Initialize the console - CON_InitEx works after VIDEO_ calls
	CON_InitEx(rmode, x, y, w, h);

	// Clear the garbage around the edges of the console
    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
}

void resetscreen()
{
	printf("\x1b[J");
}

void flash(char* source, char* destination)
{
	u8 *buffer3 = (u8 *)memalign(32, BLOCKSIZE);
	s32 ret;
	fstats *stats = memalign(32, sizeof(fstats));
	s32 a;
	FILE *file;
	file = fopen(source, "rb");
	if(!file) 
	{
		printf("fopen error\n");
		sleep(20);
		Reboot();
	}
	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);
	printf("Flashing to %s\n", destination);
	printf("SD file is %u bytes\n", filesize);	

	ISFS_Delete(destination);
	ISFS_CreateFile(destination, 0, 3, 3, 3);
	a = ISFS_Open(destination, ISFS_OPEN_RW);
	if(!a)
	{
		printf("isfs_open_write error %d\n", a);
	}
	printf("Writing file to nand...\n\n");
		
	u32 size;
	u32 restsize = filesize;
	while (restsize > 0)
	{
		if (restsize >= BLOCKSIZE)
		{
			size = BLOCKSIZE;
		} else
		{
			size = restsize;
		}
		ret = fread(buffer3, 1, size, file);
		if(!ret) 
		{
			printf(" fread error %d\n", ret);
		}
		ret = ISFS_Write(a, buffer3, size);
		if(!ret) 
		{
			printf("isfs_write error %d\n", ret);
		}
		restsize -= size;
	}
	
	ISFS_Close(a);
	s32 b = ISFS_Open(destination, ISFS_OPEN_RW);
	ret = ISFS_GetFileStats(b, stats);
	printf("Flashing file to nand successful!\n");
	printf("New file is %u bytes\n", stats->file_length);
	ISFS_Close(b);
	fclose(file);
	free(stats);
	free(buffer3);
}

void dumpfile(char source[1024], char destination[1024])
{
	// TODO Implement a way to abort the dumping to process without exiting the program
	Verify_Flags();
	int buttonsdown = 0;
	WPAD_ScanPads();
	buttonsdown = WPAD_ButtonsDown(0);
	if (buttonsdown & WPAD_BUTTON_B) 
	{
		printf("Exiting...\n");
		sleep(5);
		Reboot();
	}

	u8 *buffer;
	fstats *status;

	FILE *file;
	int fd;
	s32 ret;
	u32 size;

	fd = ISFS_Open(source, ISFS_OPEN_READ);
	if (fd < 0) 
	{
		printf("Error: ISFS_OpenFile(%s) returned %d\n", source, fd);
		return;
	}
	
	file = fopen(destination, "wb");
	if (!file)
	{
		printf("Error: fopen(%s) returned 0\n", destination);
		sleep(20);
	}

	status = memalign(32, sizeof(fstats) );
	ret = ISFS_GetFileStats(fd, status);
	if (ret < 0)
	{
		printf("ISFS_GetFileStats(fd) returned %d\n", ret);
		free(status);
		return;
	}
	printf("File: %s, filesize: %uKB\nDumping...", source, (status->file_length / 1024)+1);

	buffer = (u8 *)memalign(32, BLOCKSIZE);
	u32 restsize = status->file_length;
	while (restsize > 0)
	{
		if (restsize >= BLOCKSIZE)
		{
			size = BLOCKSIZE;
		} else
		{
			size = restsize;
		}
		ret = ISFS_Read(fd, buffer, size);
		if (ret < 0)
		{
			printf("ISFS_Read(%d, %p, %d) returned %d\n", fd, buffer, size, ret);
			free(status);
			free(buffer);
			return;
		}
		ret = fwrite(buffer, 1, size, file);
		if(ret < 0) 
		{
			printf("fwrite error%d\n", ret);
			sleep(10);
			Reboot();
		}
		restsize -= size;
	}
	ISFS_Close(fd);
	fclose(file);
	free(status);
	free(buffer);
	printf("complete\n");
}

void zero_sig(signed_blob *sig) 
{
	u8 *sig_ptr = (u8 *)sig;
	memset(sig_ptr + 4, 0, SIGNATURE_SIZE(sig)-4);
}

void brute_tmd(tmd *p_tmd) 
{
	u16 fill;
	for(fill=0; fill<65535; fill++) 
	{
		p_tmd->fill3=fill;
		sha1 hash;
		//printf("SHA1(%p, %x, %p)\n", p_tmd, TMD_SIZE(p_tmd), hash);
		SHA1((u8 *)p_tmd, TMD_SIZE(p_tmd), hash);;
		  
		if (hash[0]==0) 
		{
			//printf("setting fill3 to %04hx\n", fill);
			return;
		}
	}
	printf("Unable to fix tmd :(\n");
}

			
void sort(u8 *a, int cnt)
{
	u8 tmp;
	int i, j, m;
	
	for(i = 0; i < cnt; i++)
	{
		m = i;
		for(j = i; j < cnt; j++)
			if(a[j] < a[m])
				m = j;
		tmp = a[i];
		a[i] = a[m];
		a[m] = tmp;
	}
}

u8 *get_ioslist(u32 *cnt)
{
	u64 *buf;
	s32 i, res;
	u32 tcnt = 0, icnt = 0;
	u8 *ioses = NULL;
	
	//Get stored IOS versions.
	res = ES_GetNumTitles(&tcnt);
	if(res < 0)
	{
		printf("ES_GetNumTitles: Error! (result = %d)\n", res);
		Reboot();
	}
	buf = memalign(32, sizeof(u64) * tcnt);
	res = ES_GetTitles(buf, tcnt);
	if(res < 0)
	{
		printf("ES_GetTitles: Error! (result = %d)\n", res);
		Reboot();
	}
	//Ugly.
	for(i = 0; i < tcnt; i++)
	{
		if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
		{
			icnt++;
			ioses = (u8 *)realloc(ioses, sizeof(u8) * icnt);
			ioses[icnt - 1] = (u8)buf[i];
		}
	}
	free(buf);
	sort(ioses, icnt);
	
	*cnt = icnt;
	return ioses;
}

int ios_selection(int default_ios)
{
	s32 pressed;
	int selection = 0;
	u32 ioscount;
	u8 *list = get_ioslist(&ioscount);
	
	int i;
	for (i=0;i<ioscount;i++)
	{
		if (list[i] == default_ios)
		{
			selection = i;
		}
	}	
	
	while (true)
	{
		printf("\x1B[%d;%dH",0,0);	// move console cursor to x/y
		printf("Select the IOS you want your channel to use: %3d", list[selection]);
		printf("\nThis will not patch it right away so you can cancel it\n");
		pressed = WPAD_ButtonsDown(0);
		WPAD_ScanPads();
		if (pressed == WPAD_BUTTON_LEFT)
		{	
			if (selection > 0)
			{
				selection--;
			} else
			{
				selection = ioscount - 1;
			}
		}
		if (pressed == WPAD_BUTTON_RIGHT)
		{
			if (selection < ioscount -1	)
			{
				selection++;
			} else
			{
				selection = 0;
			}
		}
		if (pressed == WPAD_BUTTON_A) break;
	}
	return list[selection];
}


bool patch(char tmd[500])
{
	int ios;
	s32 ret;
	s32 nandfile;
	nandfile = ISFS_Open(tmd, ISFS_OPEN_RW);
	if (nandfile < 0) 
	{
		printf("Error: ISFS_OpenFile returned %d\n", nandfile);
		return false;
	}	

	fstats *status = memalign( 32, sizeof(fstats) );

	ret = ISFS_GetFileStats(nandfile, status);
	if (ret < 0)
	{
		printf("ISFS_GetFileStats(fd) returned %d\n", ret);
		return false;
	}

	printf("Filesize: %u\n", status->file_length);

	u8 *buffer2 = (u8 *)memalign(32, status->file_length);

	ret = ISFS_Read(nandfile, buffer2, status->file_length);
	if (ret < 0)
	{
		printf("ISFS_Read(%d, %p, %d) returned %d\n", nandfile, buffer2, status->file_length, ret);
		return false;
	}
	ISFS_Close(nandfile);

	resetscreen();
	ios = ios_selection(36);
	printf("\n\nThis channel is using IOS %d\n", buffer2[0x18B]);
	printf("\nPress A to patch TMD to use IOS %d or B to exit\n", ios);

	while (true)
	{
		s32 pressed;
		pressed = WPAD_ButtonsDown(0);
		WPAD_ScanPads();
			
		if (pressed == WPAD_BUTTON_A)
		{
			break;
		}
		if (pressed == WPAD_BUTTON_B)
		{
			Reboot();
		}
	}
						
	printf("\nPatching TMD to use IOS %d\n", ios);

	zero_sig((signed_blob *)buffer2);
	brute_tmd(SIGNATURE_PAYLOAD((signed_blob *)buffer2));
	//buffer[0x18B] = 0x24;
	buffer2[0x18B] = ios;
	ISFS_Delete(tmd);
	ISFS_CreateFile(tmd, 0, 3, 3, 1);

	nandfile = ISFS_Open(tmd, ISFS_OPEN_RW);
	if (nandfile < 0) 
	{
		printf("Error: ISFS_OpenFile returned %d\n", nandfile);
		return false;
	}	

	//ISFS_Seek(nandfile, 0x0, SEEK_SET);
	ret = ISFS_Write(nandfile, buffer2, status->file_length);
	if(ret < 0) 
	{
		printf("isfs_write error %d\n", ret);
	} else
	{
		printf("Patched succesfully to IOS %d!\n", ios);
	}
	ISFS_Close(nandfile);
	free(status);
	free(buffer2);
	
	return true;
}

int isdir(char *path)
{
	s32 res;
	u32 num = 0;

	res = ISFS_ReadDir(path, NULL, &num);
	if(res < 0)
		return 0;
		
	return 1;
}

void get_attribs(char *path, u32 *ownerID, u16 *groupID, u8 *ownerperm, u8 *groupperm, u8 *otherperm)
{
	s32 res;
	u8 attributes;
	res = ISFS_GetAttr(path, ownerID, groupID, &attributes, ownerperm, groupperm, otherperm);
	if(res != ISFS_OK)
		printf("Error getting attributes of %s! (result = %d)\n", path, res);
}

s32 __FileCmp(const void *a, const void *b)
{
	dirent_t *hdr1 = (dirent_t *)a;
	dirent_t *hdr2 = (dirent_t *)b;
	
	if (hdr1->type == hdr2->type)
	{
		return strcmp(hdr1->name, hdr2->name);
	} else
	{
		if (hdr1->type == DIRENT_T_DIR)
		{
			return -1;
		} else
		{
			return 1;
		}
	}
}

void getdir(char *path, dirent_t **ent, int *cnt)
{
	s32 res;
	u32 num = 0;

	int i, j, k;

	//Get the entry count.
	res = ISFS_ReadDir(path, NULL, &num);
	if(res != ISFS_OK)
	{
		printf("Error: could not get dir entry count! (result: %d)\n", res);
		return;
	}

	//Allocate aligned buffer.
	char *nbuf = (char *)memalign(32, (ISFS_MAXPATH + 1) * num);
	char ebuf[ISFS_MAXPATH + 1];
	char pbuf[ISFS_MAXPATH + 1];

	if(nbuf == NULL)
	{
		printf("Error: could not allocate buffer for name list!\n");
		return;
	}

	//Get the name list.
	res = ISFS_ReadDir(path, nbuf, &num);
	if(res != ISFS_OK)
	{
		printf("Error: could not get name list! (result: %d)\n", res);
		return;
	}
	
	//Set entry cnt.
	*cnt = num;

	if (*ent)
	{
		free(*ent);
	}
	*ent = malloc(sizeof(dirent_t) * num);

	//Split up the name list.
	for(i = 0, k = 0; i < num; i++)
	{
		//The names are seperated by zeroes.
		for(j = 0; nbuf[k] != 0; j++, k++)
			ebuf[j] = nbuf[k];
		ebuf[j] = 0;
		k++;

		//Fill in the dir name.
		strcpy((*ent)[i].name, ebuf);
		//Build full path.
		if(strcmp(path, "/") != 0)
			sprintf(pbuf, "%s/%s", path, ebuf);
		else
			sprintf(pbuf, "/%s", ebuf);
		//Dir or file?
		(*ent)[i].type = ((isdir(pbuf) == 1) ? DIRENT_T_DIR : DIRENT_T_FILE);
		
	}
	qsort(*ent, *cnt, sizeof(dirent_t), __FileCmp);
	
	free(nbuf);
}

		
void browser(char cpath[ISFS_MAXPATH + 1], dirent_t* ent, int cline, int lcnt)
{
	int i;
	resetscreen();
	printf("Press - to dump file to SD, press + to write file from SD to NAND\n");
	printf("Press 1 to dump the dir you're currently in, including all sub dirs\n");
	printf("press 2 to patch ios version in a TMD and fakesign it\n\n");
	printf("Path: %s\n\n", cpath);
	printf("  NAME          TYPE    \n");
		
	for(i = (cline / 15)*15; i < lcnt && i < (cline / 15)*15+15; i++) 
	{
		printf("%s %-12s  %s\n", 
				(i == cline ? ">" : " "),
				ent[i].name,
				(ent[i].type == DIRENT_T_DIR ? "[DIR] " : "[FILE]")				
			);
		
				
			/*if(cline > (i + 15)) {
			i = cline;
			};*/
			/*printf("%s %-12s  %s\n",
				(i == cline ? ">" : " "), ent[i].name, (ent[i].type == DIRENT_T_DIR ? "[DIR] " : "[FILE]"));
			
			
			if(ent[i].type == DIRENT_T_FILE) {
			printf("cpath : %s\n", cpath);
			sprintf(lol, "%s/%s", cpath, ent[i].name);
			sprintf(lol2, "sd:/FSTOOLBOX%s/%s", cpath, ent[i].name);
			printf("lol : %s\n", lol);
			printf("sd path : %s\n", lol2);
			sleep(5);
			dumpfile(lol, lol2);
			printf("going to the next file\n");
			}*/
	}
	printf("\n");
}


bool dumpfolder(char source[1024], char destination[1024])
{
	printf("Entering folder: %s\n", source);
	
	s32 tcnt;
	int ret;
	int i;
	char path[1024];
	char path2[1024];
	dirent_t *test = NULL;
	getdir(source, &test, &tcnt);

	if (strlen(source) == 1)
	{
		sprintf(path, "%s", destination);
	} else
	{
		sprintf(path, "%s%s", destination, source);
	}

	ret = (u32)opendir(path);
	if (ret == 0)
	{
		ret = mkdir(path, 0777);
		if (ret < 0)
		{
			printf("Error making directory %d...\n", ret);
			sleep(10);
			Reboot();
		}
	}
	
	for(i = 0; i < tcnt; i++) 
	{				
		if (strlen(source) == 1)
		{
			sprintf(path, "%s%s", source, test[i].name);
		} else
		{
			sprintf(path, "%s/%s", source, test[i].name);
		}
		
		if(test[i].type == DIRENT_T_FILE) 
		{
			sprintf(path2, "%s%s", destination, path);

			//printf("Dumping file: %s\n", path);
			//printf("To: %s\n", path2);

			//sleep(5);
			dumpfile(path, path2);
		} else
		{
			if(test[i].type == DIRENT_T_DIR) 
			{
				dumpfolder(path, destination);
			}	
		}
	}
	free(test);
	printf("Dumping folder %s complete\n\n", source);
	return true;
}	

int ios_selectionmenu(int default_ios)
{
	s32 pressed;
	int selection = 0;
	u32 ioscount;
	u8 *list = get_ioslist(&ioscount);
	
	int i;
	for (i=0;i<ioscount;i++)
	{
		// Default to default_ios if found, else the loaded IOS
		if (list[i] == default_ios)
		{
			selection = i;
			break;
		}
		if (list[i] == IOS_GetVersion())
		{
			selection = i;
		}
	}	
	
	while (true)
	{
		printf("\x1B[%d;%dH",0,0);	// move console cursor to x/y
		printf("Currently using IOS%u (Rev %u)\n", IOS_GetVersion(), IOS_GetRevision());
		printf("Select the IOS to load for FSToolbox : %3d", list[selection]);
		printf("\nIf you want to acces savedata load IOS 249 or a cIOS\n");
		printf("Otherwise IOS 36 is the best choice\n");
		printf("Press B to continue without IOS Reload\n");
		pressed = WPAD_ButtonsDown(0);
		WPAD_ScanPads();
		if (pressed == WPAD_BUTTON_LEFT)
		{	
			if (selection > 0)
			{
				selection--;
			} else
			{
				selection = ioscount - 1;
			}
		}
		if (pressed == WPAD_BUTTON_RIGHT)
		{
			if (selection < ioscount -1	)
			{
				selection++;
			} else
			{
				selection = 0;
			}
		}
		if (pressed == WPAD_BUTTON_A) break;
		if (pressed == WPAD_BUTTON_B)
		{
			return 0;
		}
		
	}
	return list[selection];
}

/*void config(char filepath[255], char *dat)
{
FILE *f;
f = fopen(filepath, "rb");
char line[100];
char i = fgets(line, 100, f);
while(i != '\n')i++;

*dat = strstr(line, "=") 
					
					;
dat+=2;
while(*dat==' ')dat++;

}*/


bool update_fstoolbox(int argc, char **argv)
{
	resetscreen();

	if(argc == 0 || argv == 0 || *argv == 0) 
	{
		printf("Sorry, your launch path is empty\n");
		sleep(10);
		Reboot();
	}

	s32 ret;

	char NETWORK_PATH[1024];
	char sd[1024];

	sprintf(NETWORK_PATH, "%s", link);
	sprintf(sd, "%s", *argv);

	printf("Initializing network\n");
	ret = network_init();

	if(ret < 0) 
	{
		printf("INIT failed %d\n", ret);
		sleep(5);
		Reboot();
	}
	printf(" %s\n", network_getip());
	printf("\n\n");

	ret = remove(sd);
	if (ret < 0) 
	{
		printf("ERROR could not delete the old file, exitting ...\n");
		sleep(10);
		Reboot();
	} else 
	{
		printf("Deleted old file\n");
	}
	FILE *file;
	file = fopen(sd, "wb");
	if (file < 0)
	{
		printf("Error: fopen\n");
		return false;
	}

	u32 length;
	ReadNetwork(NETWORK_PATH, file, &length, NETWORK_PATH, NETWORK_HOSTNAME);
	//printf("DOWNLOADED\n");
	printf("\n\nDownloaded %s to %s\n\n", link, sd);
	return true;
}		

int main(int argc, char **argv)
{
	Power_Flag = false;
	Reset_Flag = false;
	SYS_SetPowerCallback (power_cb);
    SYS_SetResetCallback (reset_cb);

	char path2[500];
	char path3[500];
	char tmp[ISFS_MAXPATH + 1];

	s32 ret;
	int i;

	char cpath[ISFS_MAXPATH + 1];	
	dirent_t* ent = NULL;
	int lcnt;
	s32 cline = 0;

	sys_init();
	// char *dat;
	// config("sd:/test.txt", &dat);
	// printf("%s", dat);
	// sleep(10);
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);					

	int ios;
	ios = ios_selectionmenu(249);
	WPAD_Shutdown();
	if (ios != 0)
	{
		IOS_ReloadIOS(ios);
	}
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);					

	sprintf(cpath, "/");
//	strcpy(cpath, "/");
	if (Identify_SU() != 0)
	{
		printf("Identify as SU failed, press any button to continue\n");
		waitforbuttonpress();
	}
	
	ret = ISFS_Initialize();
	if (ret < 0) 
	{
		printf("Error: ISFS_Initialize returned %d\n", ret);
		sleep(5);
		Reboot();
	}

	__io_wiisd.startup();
	fatMount("sd",&__io_wiisd,0,4,512);
	//Background_Show();
	/* FILE *fp;
	mxml_node_t *tree;

	fp = fopen("sd:/flash.xml", "r");
	tree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
	fclose(fp);

	mxml_node_t *branch = mxmlFindElement(tree, tree, "filepath", NULL, NULL, MXML_DESCEND);
	mxml_node_t *node = mxmlFindElement(branch, branch, "src", NULL, NULL, MXML_DESCEND);
		
		 


	//Name's
	mxml_node_t *branchname = mxmlFindElement(tree, tree, "destinationpath", NULL, NULL, MXML_DESCEND);
		
	mxml_node_t *nodedst = mxmlFindElement(branchname, branchname, "dst", NULL, NULL, MXML_DESCEND);
		 


	char source[256];


	char destination[256];
		
			
	get_value(node, source, 256);	
	get_value(nodedst, destination, 256);
			


	printf("\n");
	//printf("File :'%s' copy to %s\n\n", source, destination);



	// printf("Channel 2 :'%s' with title ID %s\n", name2, titleid2);
	// printf("Channel 3 :'%s' with title ID %s\n", name3, titleid3);
	//sscanf(titleid, "%s", id);
	*/
	//ASND_Init();	
	//MP3Player_Init();

	//if (!MP3Player_IsPlaying()) MP3Player_PlayBuffer(background, background_size, NULL);

	resetscreen();

	printf("FS Toolbox 0.3 by Nicksasa & WiiPower\n\n"); 

	printf("ALL files will be stored in sd:/FSTOOLBOX !\n\n");
	printf("Press - to dump file to SD\n");
	printf("Press + to write file to nand\n");
	printf("Press 1 to dump the dir you're currently in, including all sub dirs\n");
	printf("Press 2 on a TMD to change the IOS it uses and fakesign it\n");
	printf("Press Home to exit\n\n");

	printf("Press A to continue\n");
	if (argc == 0 || argv == NULL || *argv == NULL)
	{
		printf("Update function not available\n\n");

	} else
	{
		printf("Press 1 to UPDATE FS Toolbox to the latest version!\n");
		printf("The update would be downloaded to: %s\n\n", *argv);
	}

	while (1) 
	{
		WPAD_ScanPads();
		
		int buttonsdown = WPAD_ButtonsDown(0);
		if (buttonsdown & WPAD_BUTTON_A) 
		{
			break;
		}
		if (buttonsdown & WPAD_BUTTON_1) 
		{
			update_fstoolbox(argc, argv);
		}
		if (buttonsdown & WPAD_BUTTON_HOME) 
		{
			Reboot();
		}
	}

	ret = (u32)opendir("sd:/FSTOOLBOX");
	if (ret == 0)
	{
		printf("FSTOOLBOX folder doesnt exist, making it...\n");
		ret = mkdir("sd:/FSTOOLBOX", 0777);
		if (ret < 0)
		{
			printf("Error making directory %d...\n", ret);
			sleep(10);
			Reboot();
		}
	}	  
	
	resetscreen();
	printf(" This software is in beta stadium\n");
	printf(" A brick prevention methood like BootMii boot2 installation is mandatory\n");
	sleep(1);
	resetscreen();
	sleep(1);
	printf(" This software is in beta stadium\n");
	printf(" A brick prevention methood like BootMii boot2 installation is mandatory\n");
	sleep(1);
	resetscreen();
	sleep(1);
	printf(" This software is in beta stadium\n");
	printf(" A brick prevention methood like BootMii boot2 installation is mandatory\n");
	sleep(1);
	resetscreen();

	getdir(cpath, &ent, &lcnt);
	cline = 0;
	browser(cpath, ent, cline, lcnt);
	
	while (1) 
	{
		Verify_Flags();
		WPAD_ScanPads();
	
		int buttonsdown = WPAD_ButtonsDown(0);

		//Navigate up.
		if(buttonsdown & WPAD_BUTTON_UP)
		{			
			if(cline > 0) 
			{
				cline--;
			} else
			{
				cline = lcnt - 1;
			}
			browser(cpath, ent, cline, lcnt);
		}

		//Navigate down.
		if(buttonsdown & WPAD_BUTTON_DOWN)
		{
			if(cline < (lcnt - 1))
			{
				cline++;
			} else
			{
				cline = 0;
			}
			browser(cpath, ent, cline, lcnt);
		}

		//Enter parent dir.
		if(buttonsdown & WPAD_BUTTON_B)
		{
			int len = strlen(cpath);
			for(i = len; cpath[i] != '/'; i--);
			if(i == 0)
				strcpy(cpath, "/");
			else
				cpath[i] = 0;
				
			getdir(cpath, &ent, &lcnt);
			cline = 0;
			browser(cpath, ent, cline, lcnt);
		}

		//Enter dir.
		if(buttonsdown & WPAD_BUTTON_A)
		{
			//Is the current entry a dir?
			if(ent[cline].type == DIRENT_T_DIR)
			{
				strcpy(tmp, cpath);
				if(strcmp(cpath, "/") != 0)
				{
					sprintf(cpath, "%s/%s", tmp, ent[cline].name);
				} else
				{				
					sprintf(cpath, "/%s", ent[cline].name);
				}
				getdir(cpath, &ent, &lcnt);
				cline = 0;
				printf("cline :%s\n", cpath);
				sprintf(path3, "sd:/FSTOOLBOX%s", cpath);
				ret = (u32)opendir(path3);
				if (ret == 0)
				{
					printf("Folder %s does not exist, making it...\n", path3);
					ret = mkdir(path3, 0777);
					if (ret < 0)
					{
						printf("Error making directory %d...\n", ret);
						sleep(10);
						Reboot();
					}
				}
			}
			browser(cpath, ent, cline, lcnt);
		}
		
		//Dump folder
		if(buttonsdown & WPAD_BUTTON_1)
		{
			dumpfolder(cpath, "sd:/FSTOOLBOX");
			printf("Dumping complete. Press any button to continue...\n");
			waitforbuttonpress();
			browser(cpath, ent, cline, lcnt);
		}

		//Dump file
		if (buttonsdown & WPAD_BUTTON_MINUS) 
		{
			if(ent[cline].type == DIRENT_T_FILE)
			{
				sprintf(tmp, "%s/%s", cpath, ent[cline].name);
				sprintf(path2, "sd:/FSTOOLBOX%s/%s", cpath, ent[cline].name);
			} else
			{
				sprintf(tmp, "/%s", ent[cline].name);
				sprintf(path2, "sd:/FSTOOLBOX%s/%s", cpath, ent[cline].name);
			}
			dumpfile(tmp, path2);
		}
		
		//Flash file
		if (buttonsdown & WPAD_BUTTON_PLUS) 
		{
			if(ent[cline].type == DIRENT_T_FILE)
			{
				sprintf(tmp, "%s/%s", cpath, ent[cline].name);
				sprintf(path2, "sd:/FSTOOLBOX%s/%s", cpath, ent[cline].name);
			} else
			{
				sprintf(tmp, "/%s", ent[cline].name);
				sprintf(path2, "sd:/FSTOOLBOX%s/%s", cpath, ent[cline].name);
			}
			flash(path2, tmp);
		}
		
		if (buttonsdown & WPAD_BUTTON_2) 
		{
			if(ent[cline].type == DIRENT_T_FILE)
			{
				sprintf(tmp, "%s/%s", cpath, ent[cline].name);
			} else
			{
				sprintf(tmp, "/%s", ent[cline].name);
			}
			resetscreen();
			patch(tmp);
			sleep(5);
			browser(cpath, ent, cline, lcnt);
		}

		if(buttonsdown & WPAD_BUTTON_HOME)
		{
			Reboot();
		}
	
	}		
	
}
