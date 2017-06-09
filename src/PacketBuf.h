#ifndef _PACKET_BUF_H_
#define _PACKET_BUF_H_


class PacketBuf
	: public auto_ptr<uint8_t>
{
public:
	PacketBuf(int32_t buf_size = 2048)
		: m_buffer_size(buf_size)
		, m_packet_size(0)
	{
		reset(new uint8_t[m_buffer_size]);
	}

	void resetPacket()
	{
		m_packet_size = 0;
	}

	int32_t copyData(const uint8_t * data, int32_t data_size)
	{
		resetPacket();

		return AppendData(data, data_size);
	}

	int32_t appendData(const uint8_t * data, int32_t data_size)
	{
		if (data_size <= 0)
		{
			return -1;
		}
		if (m_buffer_size < m_packet_size + data_size)
		{
			int32_t new_buffer_size = (m_packet_size + data_size) * 2;
			uint8_t * new_buffer = new uint8_t[new_buffer_size];

			if (0 == new_buffer)
				return -1;
			if (m_packet_size > 0)
				memcpy(new_buffer, get(), m_packet_size);

			reset(new_buffer);
			m_buffer_size = new_buffer_size;
		}

		memcpy(get() + m_packet_size, data, data_size);
		m_packet_size += data_size;

		return data_size;
	}
	bool appendUInt8(uint8_t ui8)
	{
		//		*(get() + m_packet_size ++) = ui8;
		return (sizeof(ui8) == appendData(&ui8, sizeof(ui8)));
	}
	bool appendUInt16(uint16_t ui16)
	{
		return (appendUInt8((uint8_t)(ui16 >> 8)) &&
			appendUInt8((uint8_t)(ui16 & 0xff)));
	}
	bool appendUInt32(uint32_t ui32)
	{
		return (appendUInt16((uint16_t)(ui32 >> 16)) &&
			appendUInt16((uint16_t)ui32 & 0xffff));
	}

	int32_t m_packet_size;
	int32_t m_buffer_size;

private:
	PacketBuf(const PacketBuf & src);
	PacketBuf & operator=(PacketBuf & src);
};


#endif