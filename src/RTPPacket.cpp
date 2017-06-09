#include "RTPPacket.h"

RTPPacket::RTPPacket(int32_t buffer_size = 2048)
	: PacketBuf(buffer_size)
{
}
~RTPPacket(){}

int32_t RTPPacket::appendRtpHeader(uint8_t mark, uint8_t vpt, uint16_t seq_no, uint32_t time_stamp, uint32_t ssrc, bool rtp_over_tcp = false)
{
	clear();

	if (rtp_over_tcp)
	{
		appendUInt8(0x24);
		//	--	we only send one stream channel:0-1;	--
		appendUInt8(0x00);
		appendUInt16(0x0000);
	}
	//	--	rtp header, see rfc	--
	//	--	0                   1                   2                   3			--
	//	--	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1			--
	//	--	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+		--
	//	--	|V=2|P|X|  CC   |M|     PT      |       sequence number         |		--
	//	--	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+		--
	//	--	|                           timestamp                           |		--
	//	--	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+		--
	//	--	|           synchronization source (SSRC) identifier            |		--
	//	--	+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+		--
	//	--	|            contributing source (CSRC) identifiers             |		--
	//	--	|                             ....                              |		--
	//	--	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+		--
	appendUInt8(0x80);
	appendUInt8((mark << 7 & 0x80) | vpt);
	appendUInt16(seq_no);
	appendUInt32(time_stamp);
	appendUInt32(ssrc);

	return 0;
}

int32_t RTPPacket::appendRtpData(const uint8_t * data, uint16_t len)
{
	AppendData(data, len);

	return 0;
}

int32_t RTPPacket::updateInterleavedHeader()
{
	if (0x24 == *get())
	{
		uint16_t rtp_packet_size = m_packet_size - 4;
			*(get() + 2) = (uint8_t)(0xff & (rtp_packet_size >> 8));
		*(get() + 3) = (uint8_t)(0xff & rtp_packet_size);
	}

	return 0;
}