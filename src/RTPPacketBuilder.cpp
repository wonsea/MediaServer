#include "common.h"
#include "Environment.h"
#include "RTSPCommon.h"
#include "RTPPacketBuilder.h"


RTPPacketBuilder::RTPPacketBuilder(uint8_t vpt, uint8_t apt, uint32_t ssrc, bool rtp_over_tcp /*= false*/)
	: m_vpt(vpt)
	, m_apt(apt)
	, m_seq_no(0)
	, m_ssrc(ssrc)
	, m_rtp_over_tcp(rtp_over_tcp)
{}

RTPPacketBuilder::~RTPPacketBuilder()
{}

int32_t RTPPacketBuilder::buildH264(std::vector<std::basic_string<uint8_t>> & rtp_packets, const uint8_t * nalu, int32_t nalu_size, int64_t pts, int64_t dts, bool last_nalu /*= false*/)
{
	//	--	frequence of system clock is 90000	--
	uint32_t rtp_pts = (uint32_t)(pts * 90);
	RTPPacket rtp_packet;
	
	rtp_packets.clear();
	if (0x00 == nalu[0] && 0x00 == nalu[1] && (0x01 == nalu[2] || (0x00 == nalu[2] && 0x01 == nalu[3])))
	{
		int32_t ofst(0x01 == nalu[2] ? 3 : 4);
		const uint8_t * payload(nalu + ofst + 1);
		int32_t payload_size(nalu_size - ofst - 1);
		uint8_t nalu_type(nalu[ofst]);

		//	--	single rtp	--
		if (payload_start && payload_end)
		{
			rtp_packet.appendRtpHeader(last_nalu ? 1 : 0, m_vpt,  m_seq_no ++, (uint32_t)rtp_pts, m_ssrc, m_rtp_over_tcp);
			rtp_packet.appendRtpData(&nalu_type, sizeof(nalu_type));
			rtp_packet.appendRtpData(payload, payload_size);
			rtp_packet.updateInterleavedHeader();

			rtp_packets.push_back(std::basic_string<uint8_t>(rtp_packet.get(), rtp_packet.m_packet_size))
		}
		//	--	FU-A	--
		else
		{
			bool payload_start(true), payload_end(false);
			
			while (payload_size > 0)
			{
				int32_t rtp_payload_size(min(MAX_RTP_PACKET, payload_size));
				bool mark_end(last_nalu && rtp_payload_size == payload_size);
				bool payload_end(rtp_payload_size == payload_size);
				//	--	FU indicator	--
				//	--	+---------------+	--
				//	--	|0|1|2|3|4|5|6|7|	--
				//	--	+-+-+-+-+-+-+-+-+	--
				//	--	|F|NRI|  Type   |	--
				//	--	+---------------+	--
				uint8_t fu_indicator((0xE0 & nalu_type) | NALU_TYPE_FU_A);

				rtp_packet.resetPacket();
				
				m_rtp_packet.appendRtpHeader((mark_end ? 1 : 0), m_vpt, m_seq_no ++, (uint32_t)rtp_pts, m_ssrc, m_rtp_over_tcp);
				m_rtp_packet.appendUInt8(fu_indicator);
				//	--	FU header	--
				//	--	+---------------+	--
				//	--	|0|1|2|3|4|5|6|7|	--
				//	--	+-+-+-+-+-+-+-+-+	--
				//	--	|S|E|R|  Type   |	--
				//	--	+---------------+	--
				uint8_t fu_header(0x1F & nalu_type);

				if (payload_start)
					fu_header |= 0x80;
				else if (payload_end)
					fu_header |= 0x40;
				rtp_packet.appendUInt8(fu_header);
				rtp_packet.appendRtpData(payload, rtp_payload_size);
				rtp_packet.updateInterleavedHeader();
				
				payload += rtp_payload_size;
				payload_size -= rtp_payload_size;
				
				
				rtp_packets.push_back(std::basic_string<uint8_t>(rtp_packet.get(), rtp_packet.m_packet_size));
			}
		}
	}
	else
		return -1;
	
	return 0;
}