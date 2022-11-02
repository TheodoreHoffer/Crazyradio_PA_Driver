#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int errno ;

int connectN(){
	int retval = 0 ;
	int fd ;
	if ((retval = open("/dev/Nordic0", O_RDWR))<0)
	{
		printf("Error opening device. Return value: %d\n Quitting...\n", retval) ;
		goto exit ;
	} else
	{
		fd = retval ;
		printf("Device opened\nFile descriptor: %d\n", fd) ;
	}

exit:
	return retval ;
}

int disconnectN(int fd){
	int errnum ;
	int retval = 0 ;

	errnum = errno ;
	if ((retval = close(fd)) < 0)
	{
		perror("Perror print") ;
		fprintf(stderr, "Error closing device %d\n: %s", fd, strerror(errnum));
		goto exit ;
	}
	else
		printf("Device %d closed\n", fd) ;
exit:
	return retval ;
} ;

