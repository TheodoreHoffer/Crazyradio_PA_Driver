#include "transmitPacket.h"

enum RequestList {
        SET_RADIO_CHANNEL   = 0x01,
        SET_RADIO_ADDRESS   = 0x02,
        SET_DATA_RATE       = 0x03,
        SET_RADIO_POWER     = 0x04,
        SET_RADIO_ARD       = 0x05,
        SET_RADIO_ARC       = 0x06,
        ACK_ENABLE          = 0x10,
        SET_CONT_CARRIER    = 0x20,
        SCANN_CHANNELS      = 0x21,
        LAUNCH_BOOTLOADER   = 0xFF,
}; 

#define MIN_CHANNEL 0
#define MAX_CHANNEL 125
#define MIN_DATARATE 0
#define MAX_DATARATE 2
#define MIN_RADIOPOWER 0
#define MAX_RADIOPOWER 3
#define MIN_RADIOARD 0x80
#define MAX_RADIOARD 0xA0
#define MIN_RADIOARC 0x00
#define MAX_RADIOARC 0x0F
#define ACK_DISABLE 0
#define NORMAL_CARRIER 0

//enum RequestList bRequest ;

void setRadioChannel(uint16_t RadioChannel, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = SET_RADIO_CHANNEL ;
	if (RadioChannel < MIN_CHANNEL || RadioChannel > MAX_CHANNEL)
		printf("\t[ERROR --radiochannel has not been set]\n\t Channel must be in [0..125] range\n\n") ;
	else
	{
		printf("setRadioChannel> RadioChannel %d has been requested\n", RadioChannel) ; 
		TransmitPacket(0x40, bRequest, RadioChannel, 0, 0, data, trxbuffer) ;
	}
}


void setDataRate(uint16_t DataRate, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = SET_DATA_RATE ;
	if (DataRate < MIN_DATARATE || DataRate > MAX_DATARATE)
		printf("\t[ERROR --datarate has not been set]\n\t Datarate must be in [O,1,2] range\n\n") ;
	else
	{
		printf("setDataRate %d has been requested\n", DataRate) ;
		TransmitPacket(0x40, bRequest, DataRate, 0, 0, data, trxbuffer) ;
	}
}


void setRadioPower(uint16_t RadioPower, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = SET_RADIO_POWER ;
	if (RadioPower < MIN_RADIOPOWER || RadioPower > MAX_RADIOPOWER)
		printf("\t[ERROR --radiopower has not been set]\n\t RadioPower must be in [0..3] range\n\n") ;
	else
	{
		printf("setRadioPower %d has been requested\n", RadioPower) ;
		TransmitPacket(0x40, bRequest, RadioPower, 0, 0, data, trxbuffer) ;
	}
}

void setRadioArd(uint16_t RadioArd, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = SET_RADIO_ARD ;
	//RadioArd = (RadioArd << 8) & 0x00FFFF ;
	if (RadioArd < MIN_RADIOARD || RadioArd > MAX_RADIOARD)
		printf("\t[ERROR --radioard has not been set]\n\t RadioArd must be in [Ox80..0xA0]\n\n") ;
	else
	{
		printf("setRadioArd(uint16_t 0x%x has been requested\n", RadioArd) ;
		TransmitPacket(0x40, bRequest, RadioArd, 0, 0, data, trxbuffer) ;
	}
}

void setRadioArc(uint16_t RadioArc, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = SET_RADIO_ARC ;
	if (RadioArc < MIN_RADIOARC || RadioArc > MAX_RADIOARC)
		printf("\t[ERROR --radioarc has not been set]\n\t RadioArc must be in [Ox00..0x0F]\n\n") ;
	else
	{
		printf("setRadioArc 0x%x has been requested\n", RadioArc) ; 
		TransmitPacket(0x40, bRequest, RadioArc, 0, 0, data, trxbuffer) ;
	}
}

void setAckEnable(uint16_t AckEnable, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = ACK_ENABLE ;
	if (AckEnable < ACK_DISABLE)
		printf("\t[ERROR --ackenable has not been set]\n\t AckEnable must be 0 or superior to 0\n\n") ;
	else
	{
		printf("setAckEnable 0x%x has been requested\n", AckEnable) ;
		TransmitPacket(0x40, bRequest, AckEnable, 0, 0, data, trxbuffer) ;
	}
}

void setContinuousCarrier(uint16_t ContinuousCarrier, unsigned char *data, unsigned char *trxbuffer) {
	enum RequestList bRequest = SET_CONT_CARRIER ;
	if (ContinuousCarrier < NORMAL_CARRIER)
		printf("\t[ERROR --continuouscarrier has not been set]\n\t ContinousCarrier must be 0 or superior to 0\n") ;
	else
	{
		printf("setContinuousCarrier 0x%x has been requested\n", ContinuousCarrier) ;
		TransmitPacket(0x40, bRequest, ContinuousCarrier, 0, 0, data, trxbuffer) ;
	}
	
}

