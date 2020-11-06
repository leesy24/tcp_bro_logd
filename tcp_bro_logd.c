/*****************************************************************************
Copyright (c) 2020 Dasan info tek All Rights Reserved.

Serial (1 port) to LAN (32 Sessions) Broadcast		2020, 11, 06 leesy

******************************************************************************/
#include <sys/statfs.h>

#include "sb_include.h"
#include "sb_define.h"
#include "sb_shared.h"
#include "sb_config.h"
#include "sb_extern.h"
#include "sb_ioctl.h"

#include "sb_misc.h"  ////...config_access...Eddy_2.5

#include <time.h>
#include <linux/rtc.h>

#define VERSION "1.0"
#define MAX_BUFFER_SIZE 		3000
#define MAX_SOCKET_READ_SIZE 	512
#define MAX_SERIAL_READ_SIZE 	512
#define MAX_CONNECT				32    //....5@org

struct sys_info  
{
	int				sfd;
	int				rtcfd;
	int				logfd;
	int				listen_fd;
	int				lfd[MAX_CONNECT];
	int				port_no;
	int				wait_msec;
	int				interface;
	int				socket;	
	char  			bps;
	char 			dps;
	char 			flow;
	char			rs_type;
	unsigned long 	alivetime;
	unsigned long 	rcv_timer[MAX_CONNECT];
	int				connect_cnt;
};	

struct tm	rtctime;

struct sys_info  SYS;

int port_no;

int portview;
int snmp;
struct SB_PORTVIEW_STRUCT *PSM;
struct SB_SNMP_STRUCT	  *SSM;
char WORK [MAX_BUFFER_SIZE];

int	SB_DEBUG	=	0;

void mainloop(void);
int receive_from_lan(int no);
int receive_from_port();

void close_init (int no);

/***
void get_snmp_memory ();
void get_portview_memory ();
int PA_login();

void SSM_write ();
void PSM_write (char sw, char *data, int len);
***/
void read_rtc();

int limit;

char rBUF[50][MAX_SERIAL_READ_SIZE+2+1];
int  rBUFcnt=0;
int  rBUFmax=10;

//===============================================================================	
int main (int argc, char *argv[])
{
int no;
int speed;
int socket_no;

	SB_SetPriority (5);						// Min 1 ~ Max 99 

	if (argc < 4)
	{
		printf("tcp_bro_logd - version %s\n", VERSION);
		printf("usage>\ntcp_bro_logd <serial port number> <serial speed> <TCP port number> <buffer max>\n");
		printf(" <serial port number> = 1 ~ 4\n");
		printf(" <serial speed> = 0 ~ 13\n");
		printf("\t0 : 150 BPS, 1 : 300 BPS, 2 : 600 BPS, 3 : 1200 BPS\n\t4 : 2400 BPS, 5 : 4800 BPS, 6 : 9600 BPS, 7 : 19200 BPS\n\t8 : 38400 BPS, 9 : 57600 BPS, 10 : 115200 BPS\n\t11 : 230400 BPS, 12 : 460800 BPS, 13 : 921600 BPS\n");
		printf(" <TCP port number> = 4001 or 4002 or 4003 or 4004 or etc.\n");
		printf(" <buffer max> = 0 ~ 50\n");
		exit(-1);
	}

	if (atoi(argv[1]) <= 0)
	{
		printf("need serial port number(1~4)\n");
		exit(-1);
	}
	port_no = atoi (argv[1]) - 1;			// port no 1 ~ 16

/***************
Speed 통신 속도
0 : 150 BPS 1 : 300 BPS 2 : 600 BPS 3 : 1200 BPS 4 : 2400 BPS 5 : 4800 BPS
6 : 9600 BPS 7 : 19200 BPS 8 : 38400 BPS 9 : 57600 BPS 10 : 115200 BPS
11 : 230400 BPS 12 : 460800 BPS 13 : 921600 BPS
***************/
	if (atoi(argv[2]) < 0)
	{
		printf("need serial speed(0~139)\n");
		exit(-1);
	}
	speed = atoi (argv[2]);

	if (atoi(argv[3]) < 0)
	{
		printf("need TCP port number\n");
		exit(-1);
	}
	socket_no = atoi (argv[3]);

	if (argc > 4)
	{
		rBUFmax = atoi(argv[4]);
		if(rBUFmax < 0)
		{
			printf("fixed..rBUFmax:0count\n");
			rBUFmax = 0;
		}
		if(rBUFmax > 50)
		{
			printf("fixed..rBUFmax:50count\n");
			rBUFmax = 50;
		}
	}

	signal(SIGPIPE, SIG_IGN);

	//get_portview_memory ();
	//get_snmp_memory ();

	struct statfs fsb;

	if(statfs("/tmp/usb", &fsb) == 0) {
		printf("USB drive capacity is %ld blocks, free %ld blocks, block size is %d bytes.\n", fsb.f_blocks, fsb.f_bfree, fsb.f_bsize);
		limit = fsb.f_blocks / 100;
	}

	SYS.sfd = 0;
	SYS.connect_cnt = 0;
	for (no=0; no<MAX_CONNECT; no++) SYS.lfd[no] = 0;

	read_rtc();
	/**/
	printf("RTC Time = %d-%d-%d, %02d:%02d:%02d\n",
			rtctime.tm_year + 1900, rtctime.tm_mon + 1, rtctime.tm_mday, 
			rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);
	/**/

	while (1) 
	{
		SYS.bps		 	= speed;
		SYS.dps		 	= 3;
		SYS.flow		= 0;
		SYS.interface	= SB_RS485_NONE_ECHO;
		SYS.wait_msec	= 0;
		SYS.alivetime	= 0; 	
		SYS.socket		= socket_no;
		
		/****
		if (SYS.wait_msec == SB_ENABLE)
			SYS.wait_msec = 0;
		else	
			SYS.wait_msec = SB_GetDelaySerial (SYS.bps);
		****/
		
		/****
		if (portview) 
			{
			PSM_PORT.reset_request = SB_DISABLE;	// Reset init
			PSM_PORT.flag		   = SB_ENABLE;		// Scope init
			PSM_PORT.status 	   = SB_ENABLE;		// Wait Stat
			}
		if (snmp) 
			{
			SSM_CONNECT_STAT = SB_DISABLE;
			SSM_PORTRESET = SB_DISABLE;
			}
		****/

		if (SB_DEBUG) 
			SB_LogMsgPrint ("Tcp_Broadcast port=%d, speed=%d, dps=%d, flow=%d, MRU=%d, Alive=%d, interface=%d, TCP port=%d, buf_max=%d\n", port_no+1, SYS.bps, SYS.dps, SYS.flow, SYS.wait_msec, SYS.alivetime, SYS.interface, SYS.socket, rBUFmax);
		printf("Tcp_Broadcast port=%d, speed=%d, dps=%d, flow=%d, MRU=%d, Alive=%ld, interface=%d, TCP port=%d, buf_max=%d\n", port_no+1, SYS.bps, SYS.dps, SYS.flow, SYS.wait_msec, SYS.alivetime, SYS.interface, SYS.socket, rBUFmax);
		
		mainloop();
	}
}

/****
//===============================================================================
void get_snmp_memory ()
{
void *shared_memory = (void *)0;

	snmp = SB_DISABLE;
	shared_memory = SB_GetSharedMemory (SB_SNMP_KEY, sizeof (struct SB_SNMP_STRUCT));
	if (shared_memory == (void *)-1)	return;
	SSM = (struct SB_SNMP_STRUCT *)shared_memory;
	snmp = SB_ENABLE;
}
//===============================================================================
void get_portview_memory (void)
{
void *shared_memory = (void *)0;

	portview = SB_DISABLE;
	shared_memory = SB_GetSharedMemory (SB_PORTVIEW_KEY, sizeof (struct SB_PORTVIEW_STRUCT));
	if (shared_memory == (void *)-1)	return;
	PSM = (struct SB_PORTVIEW_STRUCT *)shared_memory;
	portview = SB_ENABLE;
}
****/

void read_rtc(void)
{
	int ret;

	SYS.rtcfd = open("/dev/rtc0", O_RDONLY);
	if (SYS.rtcfd < 0) 
	{
		printf ("RTC --> Open Error\n");
		//return 0;
	}

	ret = ioctl(SYS.rtcfd, RTC_RD_TIME, &rtctime);
	if (ret < 0) 
	{
		printf ("RTC --> ioctl Error !\n");
		//return 0;
	}
	close (SYS.rtcfd);
}

//===============================================================================
void mainloop(void)
{
int no, ret;
unsigned long CTimer=0, BTimer;
unsigned long DTimer = 0;
unsigned long STimer = 0;

	SB_msleep (100);
	SYS.listen_fd = SB_ListenTcp (SYS.socket,4, 4);
	if (SYS.listen_fd <= 0) return;

	if (SYS.sfd <= 0)
	{
		SYS.sfd = SB_OpenSerial (port_no);
		if (SYS.sfd <= 0) return;
	}
	ioctl (SYS.sfd, INTERFACESEL, &SYS.interface);
	SB_InitSerial (SYS.sfd, SYS.bps, SYS.dps, SYS.flow);
	SB_ReadSerial (SYS.sfd, WORK, MAX_BUFFER_SIZE, 0);
	if (SYS.interface == SB_RS232)
	{
		SB_SetDtr (SYS.sfd, SB_ENABLE);
		SB_SetRts (SYS.sfd, SB_ENABLE);
	}

	char filename[100];
	sprintf(filename, "/tmp/usb/%02d%02d%02d.txt", rtctime.tm_year + 1900 - 2000, rtctime.tm_mon + 1, rtctime.tm_mday);
	SYS.logfd = open(filename, O_WRONLY | O_CREAT | O_APPEND);
	printf("Create log file \"%s\" : %d\n", filename, SYS.logfd);
	if (SYS.logfd <= 0)
	{
		return;
	}
	
	while (1)
	{	
		if (SYS.bps != 13) SB_msleep (1);
		
		BTimer = SB_GetTick ();
		if (BTimer < CTimer)						// Check over Tick Counter;  0xffffffff (4 bytes) 
		{
			DTimer = BTimer + 7000;
			STimer = BTimer + 1000;
			for (no=0; no<MAX_CONNECT; no++)
				SYS.rcv_timer[no] = BTimer + SYS.alivetime;
		}	
		CTimer = BTimer;
		
		if (DTimer < CTimer) 		
		{
			DTimer = CTimer + 7000;		// 7 sec
			sprintf (WORK, "/var/run/debug-%d", port_no);
    		if (access(WORK, F_OK) == 0)  SB_DEBUG = 1; else SB_DEBUG = 0;
    	}	
   		
		ret = SB_AcceptTcpMulti (SYS.listen_fd, 1);
		if (ret > 0)
		{
			///printf("SB_AcceptTcpMulti():ret=%d\n", ret); 
			if (++SYS.connect_cnt > MAX_CONNECT) 
			{
				--SYS.connect_cnt;
				close (ret);
			}
			else
			{
				for (no=0; no<MAX_CONNECT; no++)
				{
					if (SYS.lfd[no] == 0) 
					{
						SYS.lfd[no] = ret;
						SYS.rcv_timer[no] = CTimer + SYS.alivetime;
						
						///printf("Cnt=%02d,no=%d\n", SYS.connect_cnt, no); 
						
						/****
						if (portview) PSM_PORT.status = SB_ACTIVE;
						if (snmp)
						{
							SSM_CONNECT_STAT = SB_ENABLE;
							SSM_CONNECT_COUNT ++;
						}
						****/
						break;
					}		
				}
			}
		}

		for (no=0; no<MAX_CONNECT; no++)
		{
			if (SYS.lfd[no] <= 0) continue;
			if (SYS.alivetime) 
			{
				if (SYS.rcv_timer[no] < CTimer) 
				{
					close_init (no);
					continue;
				}
			}
			
			/****/
			if (receive_from_lan (no) < 0) 
			{
				close_init (no);
				continue;
			}
			/****/

			
			/****
			if (portview)
			{
				if (PSM_PORT.reset_request == SB_ENABLE) 		// Portview Reset request
				{
					shutdown (SYS.listen_fd, SHUT_RDWR);
					close (SYS.listen_fd);
					for (no=0; no<MAX_CONNECT; no++) close_init (no);
					return;
				}
			}
			****/
		}

		receive_from_port();
		
		/****
		if (snmp)
		{
			if (STimer > CTimer) continue;
			STimer = CTimer + 1000;
			SSM_write ();
			if (SSM_PORTRESET) return;			// SNMP set (reset)				
		}
		****/	
	}	 
}
//===============================================================================
void close_init (int no)
{
	if (SYS.lfd[no] > 0)
	{
		shutdown(SYS.lfd[no], SHUT_RDWR);	
		close (SYS.lfd[no]);
		SYS.lfd[no] = 0;
		if (--SYS.connect_cnt < 0)  SYS.connect_cnt = 0;
	}
	
	if (SYS.connect_cnt==0)
	{
		if (portview) PSM_PORT.status = SB_ENABLE;
		if (snmp) SSM_CONNECT_STAT = SB_DISABLE;	
	}		

	if (SB_DEBUG) SB_LogMsgPrint ("Close Socket no=%d\n", no);
}	
//===============================================================================
int receive_from_lan(int no)
{
int len, sio_len; 

	sio_len = ioctl (SYS.sfd, TIOTGTXCNT);
	if (sio_len < 32) return 0;	
	if (sio_len > MAX_SOCKET_READ_SIZE) sio_len = MAX_SOCKET_READ_SIZE;
	len = SB_ReadTcp (SYS.lfd[no], WORK, sio_len);
	if (len <= 0) return len;
	
	SB_SendSerial (SYS.sfd, WORK, len);		
	if (SB_DEBUG) SB_LogDataPrint ("L->S", WORK, len); 
	////if (portview && no==0)  PSM_write ('S', WORK, len);
	SYS.rcv_timer[no] = SB_GetTick()+SYS.alivetime;
	return 0;
} 
//===============================================================================
int receive_from_port()
{
int len, no;
char prefix[6+1+6+1+1];

	len = SB_ReadSerial (SYS.sfd, WORK, MAX_SERIAL_READ_SIZE, SYS.wait_msec);
	if (len <= 0) return 0;
	/***
	for (no=0; no<len; no++)
	{
		printf("%d:%d(0x%x)\n", no, WORK[no], WORK[no]);
	}
	***/
	for (no=0; no<MAX_CONNECT; no++)
	{
		if (SYS.lfd[no] <= 0) continue;	
		write (SYS.lfd[no], WORK, len);
		///if (portview && no==0) PSM_write ('R', WORK, len);
	}
	if (SB_DEBUG) SB_LogDataPrint ("S->L", WORK, len); 

	struct statfs fsb;

	if (statfs("/tmp/usb", &fsb) == 0) {
		//printf("device free is %ld blocks. %d\n", fsb.f_bfree, limit);
		if (fsb.f_bfree < limit) {
			DIR *d;
			struct dirent *dir;
			int min = 999999;
			char target[256];
			d = opendir("/tmp/usb");
			if (d)
			{
				while ((dir = readdir(d)) != NULL)
				{
					char f_name[256];
					int ymd;
					strncpy(f_name, dir->d_name, 8);
					f_name[6] = '\0';
					ymd = atoi(f_name);
					printf("file name is %s, %d\n", f_name, ymd);
					if (ymd != 0 && ymd < min) {
						min = ymd;
						strcpy(target, dir->d_name);
					}
				}
				closedir(d);
			}
			char target_full[256];
			sprintf(target_full, "/tmp/usb/%s", target);
			//printf("target full file is \"%s\".\n", target_full);
			if (min != 999999) {
				remove(target_full);
			}
		}
	}

	int old_y, old_m, old_d;
	old_y = rtctime.tm_year + 1900 - 2000;
	old_m = rtctime.tm_mon + 1;
	old_d = rtctime.tm_mday;
	//printf("old year = %d, month = %d, day = %d\n", old_y, old_m, old_d);

	read_rtc();

	if (old_y != rtctime.tm_year + 1900 - 2000 ||
		old_m != rtctime.tm_mon + 1 ||
		old_d != rtctime.tm_mday)
	{
		if (SYS.logfd > 0)
		{
			fsync(SYS.logfd);
			close(SYS.logfd);			receive_from_port();

		}
		char filename[100];
		sprintf(filename, "/tmp/usb/%02d%02d%02d.txt",
			rtctime.tm_year + 1900 - 2000, rtctime.tm_mon + 1, rtctime.tm_mday);
		SYS.logfd = open(filename, O_WRONLY | O_CREAT | O_APPEND);
		//printf("open(\"%s\"):return = %d\n", filename, SYS.sfd);
	}

	sprintf(prefix, "%02d%02d%02d %02d%02d%02d ",
		rtctime.tm_year + 1900, rtctime.tm_mon + 1, rtctime.tm_mday,
		rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);
	/***
	write(SYS.logfd, prefix, strlen(prefix));
	if(WORK[len-1] == 0x0d) // == '\r'
	{
		write(SYS.logfd, WORK, len);
		write(SYS.logfd, "\n", 1);
	}
	else
	{
		write(SYS.logfd, WORK, len);
		write(SYS.logfd, "\r\n", 2);
	}
	***/

	strncpy( rBUF[rBUFcnt], prefix, strlen(prefix) );
	if(WORK[len-1] == 0x0d) // == '\r'
	{
		strncpy( &rBUF[rBUFcnt][strlen(prefix)], WORK, len );  //Buf..LogRecords
		strncpy( &rBUF[rBUFcnt][strlen(prefix) + len], "\n", 1 );
		strncpy( &rBUF[rBUFcnt][strlen(prefix) + len + 1], "\0", 1 );
	}
	else
	{
		strncpy( &rBUF[rBUFcnt][strlen(prefix)], WORK, len );  //Buf..LogRecords
		strncpy( &rBUF[rBUFcnt][strlen(prefix) + len], "\r\n", 2 );
		strncpy( &rBUF[rBUFcnt][strlen(prefix) + len + 2], "\0", 1 );
	}
	rBUFcnt++;
	//
	if ( rBUFcnt >= rBUFmax )  //rBUF
	{
		for ( rBUFcnt=0; rBUFcnt<rBUFmax ; rBUFcnt++ )
		{
			write(SYS.logfd, rBUF[rBUFcnt], strlen(rBUF[rBUFcnt]) );
		}
		rBUFcnt=0;
	}

	fsync(SYS.logfd);

	return 0;
}
//===============================================================================
/****
void PSM_write (char sw, char *data, int len)
{
static int f_err=0, o_err=0, p_err=0;		//    get first error
struct serial_icounter_struct   err_cnt;
int cplen, f_len, l_len;

	if (sw == 'R')
		{
		switch ( PSM_PORT.flag) {
		case 0 : return;
		case 2 :
			if (RX_GET <= RX_PUT)
				cplen = SB_RING_BUFFER_SIZE - RX_PUT + RX_GET - 1;	
			else
				cplen = RX_GET - RX_PUT - 1;
			if (cplen == 0) return;
			if (cplen < len) len = cplen;	
			
			if ((RX_PUT+len) < SB_RING_BUFFER_SIZE) 
				{ f_len = len; l_len = 0; }
			else
				{ f_len = SB_RING_BUFFER_SIZE-RX_PUT;  l_len = len - f_len; }

			if (f_len) memcpy (&PSM->rx_buff[RX_PUT], data, f_len);
			if (l_len) 
				{
				memcpy (&PSM->rx_buff[0], &data[f_len], l_len);	
				RX_PUT = l_len; 
				}
			else
				RX_PUT += f_len;	
			
			PSM->rx_lastputtime = SB_GetTick();
		case 1 :		// error count
			ioctl (SYS.sfd, TIOCGICOUNT, &err_cnt);
			PSM_PORT.rx_bytes += len;
			PSM_PORT.frame_err += (err_cnt.frame - f_err);
			f_err = err_cnt.frame;
			PSM_PORT.parity_err += (err_cnt.parity - p_err);
			p_err = err_cnt.parity;
			PSM_PORT.overrun_err += (err_cnt.overrun - o_err);
			o_err = err_cnt.overrun;
			break;
			}	
		}
	else
		{			
		switch ( PSM_PORT.flag) {
		case 0 : return;
		case 2 :
			if (TX_GET <= TX_PUT)
				cplen = SB_RING_BUFFER_SIZE - TX_PUT + TX_GET - 1;	
			else
				cplen = TX_GET - TX_PUT - 1;
			if (cplen == 0) return;
			if (cplen < len) len = cplen;		
			
			if ((TX_PUT+len) < SB_RING_BUFFER_SIZE) 
				{ f_len = len; l_len = 0; }
			else
				{ f_len = SB_RING_BUFFER_SIZE-TX_PUT;  l_len = len - f_len; }

			if (f_len) memcpy (&PSM->tx_buff[TX_PUT], data, f_len);
			if (l_len) 
				{
				memcpy (&PSM->tx_buff[0], &data[f_len], l_len);	
				TX_PUT = l_len; 
				}
			else
				TX_PUT += f_len;	
			
			PSM->tx_lastputtime = SB_GetTick();
		case 1 :		// error count
			PSM_PORT.tx_bytes += len;
			break;			
			}	
		}
}
****/
//===============================================================================
/****
void SSM_write ()
{
unsigned char msr;
struct serial_icounter_struct   err_cnt;
static int f_err=0, o_err=0, p_err=0;		//    get first error
static unsigned char msr_shadow = 0;

	msr = ioctl(SYS.sfd, TIOTSMSR) & 0xf0;
	if (msr != msr_shadow) 
		{
		if ((msr&0x10) != (msr_shadow&0x10)) SSM_CTS_CHANGE ++;
		if ((msr&0x20) != (msr_shadow&0x20)) SSM_DSR_CHANGE ++;
		if ((msr&0x80) != (msr_shadow&0x80)) SSM_DCD_CHANGE ++;
		if (msr&0x10) SSM_CTS_STAT = 1; else SSM_CTS_STAT = 0;
		if (msr&0x20) SSM_DSR_STAT = 1; else SSM_DSR_STAT = 0;
		if (msr&0x80) SSM_DCD_STAT = 1; else SSM_DCD_STAT = 0;		
		msr_shadow = msr;
		}

	ioctl (SYS.sfd, TIOCGICOUNT, &err_cnt);
	SSM_FRAMING_ERRS += (err_cnt.frame   - f_err);  f_err = err_cnt.frame;
	SSM_PARITY_ERRS  += (err_cnt.parity  - p_err);  p_err = err_cnt.parity;
	SSM_OVERRUN_ERRS += (err_cnt.overrun - o_err);  o_err = err_cnt.overrun;
}		
****/

