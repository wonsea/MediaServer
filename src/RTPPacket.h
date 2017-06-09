#ifndef _RTP_PACKET_H_
#define _RTP_PACKET_H_


#include "PacketBuf.h"


class RTPPacket
	: public PacketBuf
{
public:
	RTPPacket(int32_t buffer_size = 2048);
	~RTPPacket();

	int32_t appendRtpHeader(uint8_t mark, uint8_t vpt, uint16_t seq_no, uint32_t time_stamp, uint32_t ssrc, bool rtp_over_tcp = false);
	int32_t appendRtpData(const uint8_t * data, uint16_t len);
	int32_t updateInterleavedHeader();
};

#endif

