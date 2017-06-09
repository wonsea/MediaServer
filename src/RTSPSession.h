#ifndef _RTSP_SESSION_H_
#define _RTSP_SESSION_H_


#include "RTPPacketBuilder.h"
#include "StreamSourceCallback.h"


struct RTSPSessInfo
{
	//	--	RTP_UDP/RTP_TCP/RAW_UDP	--
	uint8_t transport;
	Poco::StreamSocket rtsp_sock;

	std::string resource_id;
	
	uint16_t server_ip;
	uint16_t server_rtp_port;
	uint32_t client_ip;
	uint16_t client_rtp_port;
	uint8_t vpt;
	uint8_t apt;
	uint32_t ssrc;
};


class StreamSource;
class RTSPSession
	: public Poco::Runnable
	, public StreamSourceCallback
{
public:
	RtspSession(AbstractRtspServer * owner);
	~RtspSession();

	const RTSPSessInfo & getSessInfo();
	int32_t start(const RtspSessInfo & info);
	//	--	send data to client	--
	int32_t sendData(uint8_t * data, int32_t len);
	//	--	have received data from client	--
	int32_t onReceivedData(uint8_t channel_id, uint8_t * data, uint16_t len);
	int32_t stop();

	//	--	for Runnable	--
	virtual void run();
	//	--	for StreamSourceCallback	--
	virtural int32_t StreamOut(const uint8_t * data, int32_t data_size) ;

private:
	void doRun();
	
	
private:
	RTSPServer * m_owner;
	RTSPSessInfo m_sess_info;
	
	Poco::Thread m_thread_sess;
	bool m_thread_stop;
	
	Poco::Mutex m_mutex_stream_data_queue;
	std::queue<std::basic_string<uint8_t>> m_stream_data_queue;
	
	RTPPacketBuilder m_rtp_packet_builder;
};


#endif
