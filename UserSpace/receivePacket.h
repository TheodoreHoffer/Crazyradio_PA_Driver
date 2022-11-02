#include "status.h"

typedef struct __inpacket {
	unsigned char status[SZ_EP1_IN_STATUS] ;
	unsigned char ackPayload[32] ;
} inpacket ;

//struct inpacket *ep1_in_packet ;
void ReceivePacket(inpacket ep1_in_packet){
	int bufferindex = 0 ;
	unsigned char recStatus = ep1_in_packet.status[0] ;
	unsigned char *recPayload[32];
	memset(recPayload, 0, sizeof(recPayload)) ;
	printf("sizeof(recPayload) = %lu \n", sizeof(recPayload)/ sizeof(recPayload[0])) ;
	memcpy(recPayload, ep1_in_packet.ackPayload, strlen((const char*) ep1_in_packet.ackPayload)) ;
	
	printf("ReceivePacket>...\n") ;
	if (recStatus & (1 << ACK_RECEIVED)) printf("Request acknowledged\n") ;	
	if (recStatus & (1 << POWER_DETECTOR)) printf("Device is powered\n") ;
	if (recStatus & (1 << RESERVED_2)) printf("RESERVED_2 is set\n") ;
	if (recStatus & (1 << RESERVED_3)) printf("RESERVED_3 is set\n") ;

	/* Number of retransmissions are set in Nb_RETRX_4 to 7 */	
	for (bufferindex = 0 ; bufferindex < ((sizeof(recPayload) / sizeof(recPayload[0])) -1) ;bufferindex++)
	{
		printf("0x%hhn-", recPayload[bufferindex]) ;
	}
	printf("\n") ;	
} ;
