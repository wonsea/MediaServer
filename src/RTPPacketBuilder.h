#ifndef _RTP_PACKET_BUILDER_H_
#define _RTP_PACKET_BUILDER_H_


#include "RTPPacket.h"


class RTPPacketBuilder
{
public:
	RTPPacketBuilder(uint8_t vpt, uint8_t apt, uint32_t ssrc, bool rtp_over_tcp = false);
	~RTPPacketBuilder();
	
	int32_t buildH264(std::vector<std::basic_string<uint8_t>> & rtp_packets, const uint8_t * nalu, int32_t nalu_size, int64_t pts, int64_t dts, bool last_nalu = false);
	
private:
	uint8_t m_vpt;
	uint8_t m_apt;
	uint16_t m_seq_no;
	uint32_t m_ssrc;
	bool m_rtp_over_tcp;
};


#endif