#include "common.h"
#include "Environment.h"
#include "RTSPSession.h"
#include "RTSPServer.h"


RTSPSession::RTSPSession(RTSPServer * owner)
	: m_owner(owner)
	
	, m_thread_stop(true)
{}

RTSPSession::~RtspSession()
{}

const RTSPSessInfo & RTSPSession::getSessInfo()
{
	return m_rtsp_sess_info;
}

int32_t RTSPSession::start(const RtspSessInfo & info)
{

	m_sess_info = info;
	m_thread_stop = false;
	m_thread_sess.start(*this);
	
	return 0;
}

//	--	send data to client	--
int32_t RTSPSession::sendData(uint8_t * data, int32_t len)
{
	return 0;
}

//	--	have received data from client	--
int32_t RTSPSession::onReceivedData(uint8_t channel_id, uint8_t * data, uint16_t len)
{
	return 0;
}

int32_t RTSPSession::stop()
{
	m_thread_stop = true;
	m_thread_sess.join();
	
	return 0;
}

void RTSPSession::run()
{
	try
	{
		doRun();
	}
	catch(Poco::Exception e)
	{}
	catch(...)
	{}

	m_thread_stop = true;
}
int32_t RTSPSession::StreamOut(const uint8_t * data, int32_t data_size) 
{
	Poco::ScopedLock<Poco::Mutex> lock(m_mutex_stream_data_queue);
	std::basic_string<uint8_t> new_data;

	if (0 != data && data_size > 0)
		new_data.assign(data, data_size);
	m_stream_data_queue.push(new_data);

	return 0;
}

void RTSPSession::doRun()
{
	RTPPacketBuilder rtp_builder(m_sess_info.vpt, m_sess_info.apt, m_sess_info.ssrc, RTP_TCP == m_sess_info.transport);
	std::basic_string<uint8_t> nalu;
	int64_t pts(0), dts(0);
	
	while (!m_thread_stop)
	{
		std::basic_string<uint8_t> stream_data;
		
		//	--	pop stream data	--
		{
			Poco::ScopedLock<Poco::Mutex> lock(m_mutex_stream_data_queue);
			
			stream_data = m_stream_data_queue.front();
			m_stream_data_queue.pop();
		}
	
		
		std::vector<std::basic_string<uint8_t>> rtp_packets;
		
		if (!nalu.empty())
		{
			rtp_builder->buildH264(rtp_packets, nalu.c_str(), nalu.length(), pts, dts, stream_data.empty());

			for(auto it = rtp_packets.begin(); it != rtp_packets.end(); ++ it)
			{
				m_sess_info.rtsp_sock.sendBytes(it->c_str(), it->length());
			}
		}
		nalu = stream_data;
	}
}
