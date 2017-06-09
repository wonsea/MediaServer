#include "common.h"
#include "StreamSource.h"

StreamSource()
	: m_stop(true)
{}

StreamSource::~StreamSource()
{}
	
int32_t StreamSource::start(StreamSourceCallback * callback)
{
	m_callback = callback;
	m_stop = false;
	m_thread.start(*this);
	
	return 0;
}

int32_t StreamSource::stop()
{
	m_stop = true;
	m_thread.join();
	
	return 0;
}

void StreamSource::run()
{
	doStreamSource();
	m_stop = true;
}

int32_t StreamSource::doStreamSource()
{
	FILE * fp_h264(fopen("./source.264", "rb"));
	const int32_t buf_size(10240);
	uint8_t buf[buf_size];
	uint8_t data_size(0);
	
	if (!fp_h264)
		return -1;
	while (0 == feof(fp_h264))
	{
		size_t bytes_read = fread(buf + data_size, 1, buf_size - data_size, fp_h264);
		
		if (bytes_read <= 0)
			break;
		data_size += bytes_read;
		
		
		const uint8_t * data(buf);
		int32_t last_prefix_pos(-1);
		
		for (int32_t i = 0; i < data_size - 4;)
		{
			if (0x00 == data[i] && 0x00 == data[i + 1] &&
				(0x01 == data[i + 2] ||
				(0x00 == data[i + 2] && 0x01 == data[i + 3])))
			{
				int32_t ofst(0x01 == data[i + 2] ? 3 : 4);
				uint8_t nalu_type(0x1f & data[ofst]);
				
				if (last_prefix_pos >= 0 && m_callback)
					m_callback->StreamOut(data + last_prefix_pos, i - last_prefix_pos);
				
				last_prefix_pos = i;
				i += ofst;
			}
			else
				++ i;
		}
	}
	
	fclose(fp_h264);
	
	return 0;
}