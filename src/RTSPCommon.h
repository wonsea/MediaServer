#ifndef _RTSP_COMMON_H_
#define _RTSP_COMMON_H_


//	--	RTSP	--
#define LENGTH_RTSP_PARAMETER_MAX		(256)

typedef uint8_t StreamingMode;
#define RTP_UDP							(0)
#define RTP_TCP							(1)
#define RAW_UDP							(2)

#define RTP_UDP_STR						("RTP/AVP")
#define RTP_TCP_STR						("RTP/AVP/TCP")
#define RAW_UDP_STR						("RAW/RAW/UDP")


//	--	RTP	--
#define MAX_RTP_PACKET					(1300)

#define RTP_INTERLEAVE_MAGIC			(0x24)


#endif