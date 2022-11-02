/* From firmware:
   //USB controll structure

	struct controllStruct {
		unsigned char requestType;
		unsigned char request;
		unsigned short value;
		unsigned short index;
		unsigned short length;
	};
 */
#define TESTTHAT 0

struct usb_control_packet {
        uint8_t  bmRequestType;
        uint8_t  bRequest;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
	unsigned char* datafield ;
} ;

typedef struct {unsigned char bytes24[24];} ubytes24_t ;
typedef struct {unsigned char bytes32[32];} ubytes32_t ;

void TransmitPacket(uint8_t RequestType, /* 1 byte */
	     	    uint8_t Request, /* 1 byte */
	     	    uint16_t Value, /* 2 bytes */
	     	    uint16_t Index, /* 2 bytes */
	       	    uint16_t Length, /* 2 bytes ; 8 Bytes consumed before data */
	       	    unsigned char* datafield, /* 24 Bytes remaining for data*/
		    unsigned char* trxbuffer) /* TrxBuffer to send to device */
{
	int i ;
	struct usb_control_packet trxpacket = {RequestType,
					       Request,
					       Value,
					       Index,
					       Length,
					       datafield
	} ;


	printf("TRANSMITPACKET> Sizeof(trxpacket): %lu\n", sizeof(trxpacket)) ;
	memcpy(trxbuffer, (const unsigned char*)&trxpacket, sizeof(trxpacket)) ;


	printf("TRANSMITPACKET>Copied byte array is:\n") ;
	for(i=0;i<sizeof(trxbuffer);i++)
		printf("%02X ",trxbuffer[i]);
	printf("\n");
}
