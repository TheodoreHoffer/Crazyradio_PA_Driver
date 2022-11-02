#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <getopt.h>
#include <errno.h>
#include "brequest.h"
#include "receivePacket.h"
#include "connecttonordic.h"

void print_usage() {
	printf("Usage: nordic_configurator\n"
		      " [--radiochannel] \"0..125\"\n" 
		      " [--datarate] \"0,1,2\"\n"
		      " [--radiopower] \"0..3\"\n"
		      " [--radioard] \"0x80..0xA0\"\n"
		      " [--radioarc] \"0x00..0x0F\"\n"
		      " |--ackenable] \"0 or >0\"\n"
		      " [--continuouscarrier] \"0 or >0\"\n"
		      " ------------------------------------\n"
		      "For options explanations, read, in Documentation directory:\n"
		      "doc_crazyradio_usb_index [Bitcraze Wiki].pdf\n\n") ;
}

int main (int argc, char *argv[]){
	unsigned char data[24] = {0} ;
	unsigned char *pdata ;
	pdata = data ;
	int nordicfd ;
	int retval = 0 ;
	unsigned char trxbuffer[32] = {0} , rcxbuffer[40] = {0};
	unsigned char *ptrxbuffer , *prcxbuffer ;
	ptrxbuffer = trxbuffer ;
	prcxbuffer = rcxbuffer ;
	int opt= 0 ; /*option number */
	uint16_t rchannel = 0 ;
	uint16_t rdatarate = 4 ;
	uint16_t rradiopower = 4 ;
	uint16_t rradioard = 0x79 ;
	uint16_t rradioarc = 0xEF ;
	uint16_t rackenable = 1 ;
	uint16_t rcontinuouscarrier = 0 ;
	
	int i  ;



	static struct option long_options[] = {
		{"radiochannel",	required_argument,	0,	'z'},
		{"datarate",		required_argument,	0,	'r'},
		{"radiopower",		required_argument,	0,	'p'},
		{"radioard",		required_argument,	0,	'd'},
		{"radioarc",		required_argument,	0,	'c'},
		{"ackenable",		required_argument,	0,	'e'},
		{"continuouscarrier",	required_argument,	0,	'u'},
		{0,			0,			0}
	};

	int long_index = 0 ;
	
	if (argc == 1) {
		print_usage() ;
		goto exit ;
	}

	for (i = 1 ; i < argc ; i=i+2){
		if (strcmp(argv[i], "--radiochannel") &&
	 	    strcmp(argv[i], "--datarate") &&
		    strcmp(argv[i], "--radiopower") &&
		    strcmp(argv[i], "--radioard") &&
		    strcmp(argv[i], "--radioarc") &&
		    strcmp(argv[i], "--ackenable") &&
		    strcmp(argv[i], "--continuouscarrier")
		   ){
			print_usage() ;
			goto exit ;
		}
	}


	if ((nordicfd = connectN()) < 0)
	       goto exit ;
	
	while((opt = getopt_long(argc,
				 argv,
				 "z:r:p:d:c:e:u:",
				 long_options,
				 &long_index )) != -1)
	{
		switch (opt) {
			case 'z':
				rchannel = atoi(optarg) ;
				setRadioChannel(rchannel, pdata, ptrxbuffer) ;
				break ;
			case 'r':
				rdatarate = atoi(optarg) ;
				setDataRate(rdatarate, pdata, ptrxbuffer);
				break ;
			case 'p':
				rradiopower = atoi(optarg) ;
				setRadioPower(rradiopower, pdata, ptrxbuffer);
				break ;
			case 'd':
				rradioard = strtol(optarg,NULL,16) ;
				setRadioArd(rradioard, pdata, ptrxbuffer);
				break ;
			case 'c':
				rradioarc = strtol(optarg, NULL, 16) ;
				setRadioArc(rradioarc, pdata, ptrxbuffer);
				break ;
			case 'e':
				rackenable = atoi(optarg) ;
				setAckEnable(rackenable, pdata, ptrxbuffer);
				break ;
			case 'u':
				rcontinuouscarrier = atoi(optarg) ;
				setContinuousCarrier(rcontinuouscarrier, pdata, ptrxbuffer);
				break ;
			default:
				print_usage();
				goto exit ;
		}
	}
//	setRadioChannel(rchannel, pdata, ptrxbuffer) ;
	printf("main> Prepared buffer size: %lu - Content: 0x%x 0x%x %d %d %d %d \n",sizeof(ptrxbuffer),
							 ptrxbuffer[0],
						         ptrxbuffer[1],
						         ptrxbuffer[2],
						         ptrxbuffer[4],
						         ptrxbuffer[6],
						         ptrxbuffer[8]) ;

	retval = write(nordicfd, ptrxbuffer, sizeof(ptrxbuffer)) ;
	if (retval < 0)
	{
		printf("Error writing to device\n") ;
		goto exit ;
	} else
	{
		printf("Sizeof ptrxbuffer: %ld\n", sizeof(ptrxbuffer)) ;
		printf("Successfully wrote to /dev/Nordic0\n") ;
	}

	retval = read(nordicfd, prcxbuffer, sizeof(prcxbuffer)) ;
	if (retval < 0)
	{
		printf("Error reading from device\n") ;
		goto exit ;
	} else
	{
		printf("Successfully read from /dev/Nordic0\n") ;
		printf("main> Read buffer size: %lu - Content: 0x%x 0x%x %d %d %d %d \n",sizeof(prcxbuffer),
								 prcxbuffer[0],
							         prcxbuffer[1],
							         prcxbuffer[2],
							         prcxbuffer[4],
							         prcxbuffer[6],
							         prcxbuffer[8]) ;
	}

	fsync(nordicfd) ;
	retval = disconnectN(nordicfd) ; 
exit:
	return 0 ;
}
