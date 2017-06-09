#ifndef _RTSP_CONNECTION_H_
#define _RTSP_CONNECTION_H_


#include "RTSPCommon.h"
#include "RTSPSession.h"


class RTSPServer;
class RTSPConnection
	: public Poco::TCPServerConnection
{
public:
	RTSPConnection(RTSPServer * own_server, const Poco::StreamSocket& socket, uint32_t ssid);
	virtual ~RTSPConnection();

	//	--	for TCPServerConnection/Runnable	--
	virtual void run() ;


protected:
	void runRTSP();
	int32_t handleSocketRead(Poco::StreamSocket & sock);
	int32_t handleData();
	int32_t handleRequest();

	//	--	handle request command	--
	string handleCmd_OPTIONS(const string & cseq);
	string handleCmd_DESCRIBE(const string & cseq, char const* url_prefix, char const* url_suffix, char const* full_request);
	string handleCmd_SETUP(const string & cseq, const string & req_prefix, const string & url_prefix, const string &  url_suffix, const string & full_request);
	string handleCmd_PLAY(const string & cseq);
	string handleCmd_TEARDOWN(const string & cseq);

	string handleCmd_Bad(const string & cseq);
	string handleCmd_NotSupported(const string & cseq);
	string handleCmd_NotFound(const string & cseq);
	string handleCmd_SessionNotFound(const string & cseq);
	string handleCmd_MethodNotValid(const string & cseq);
	string handleCmd_UnsupportedTransport(const string & cseq);
	string handleCmd_NotImplemented(const string & cseq);

	RtspSession * onSetup(const RtspSessInfo & sess_info);
	int32_t onPlay();
	int32_t onTeardown();

	string setRTSPResponse(char const* response_string, const string & cseq);
	int32_t sendRTSPResponse(const string & resp);

	bool parseRTSPRequestString(char const* reqStr,
		unsigned reqStrSize,
		char* resultCmdName,
		unsigned resultCmdNameMaxSize,
		char ** reqPrefix,
		char* resultURLPreSuffix,
		unsigned resultURLPreSuffixMaxSize,
		char* resultURLSuffix,
		unsigned resultURLSuffixMaxSize,
		char* resultCSeq,
		unsigned resultCSeqMaxSize,
		char* resultSessionIdStr,
		unsigned resultSessionIdStrMaxSize,
		unsigned& contentLength);
	void parseTransportHeader(char const * request,
		Poco::StreamingMode& streamingMode,
		char*& streamingModeString,
		char*& destinationAddressStr,
		uint8_t& destinationTTL,
		uint16_t& clientRTPPortNum, // if UDP
		uint16_t& clientRTCPPortNum, // if UDP
		unsigned char& rtpChannelId, // if TCP
		unsigned char& rtcpChannelId // if TCP
		) ;
	int32_t parsePrefix(string & par_line, string & ssid, string & type, string & stream_type, string & source_id, string & hist_start, string & hist_end, const string & prefix);
	string rawTimeFromString(const string & time_str);
	string dateHeader();
	void decodeURL(char* url);

protected:
	RTSPServer * m_own_server;
	uint32_t m_sess_id;
	std::auto_ptr<RtspSession> m_rtsp_sess;
	const uint32_t m_vpt_config;
	const uint32_t m_apt_config;

	string m_local_ip;
	string m_client_ip;

	auto_ptr<uint8_t> m_buffer_request;
	int32_t m_data_size_request;
	int32_t m_buffer_size_request;

	static const string VIDEO_PAYLOAD_TYPE_H264;
	static const string VIDEO_PAYLOAD_TYPE_M2TS;
//	static const string m_audio_payload_type;
};
/*
typedef map<uint32_t, RTSPConnection *> RTSPConnectionMap;
typedef RTSPConnectionMap::iterator RTSPConnectionMapIt;
*/



class RTSPConnectionFactory
	: public TCPServerConnectionFactory
{
public:
	RTSPConnectionFactory(RTSPServer * own_server)
		: m_own_server(own_server)
	{}
	virtual ~RTSPConnectionFactory(){}

	//	--	for TCPServerConnectionFactory	--
	virtual TCPServerConnection* createConnection(const StreamSocket & socket) 
	{
		return new RtspConnection(m_own_server, socket, (uint32_t)socket.impl());
	}

protected:
	RTSPServer * m_own_server;
};


#endif
