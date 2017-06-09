#include "common.h"
#include "Environment.h"
#include "RTSPConnectioin.h"
#include "RTSPSession.h"
#include "RTSPServer.h"


//	--	see rfc2327	--
const string RTSPConnection::VIDEO_PAYLOAD_TYPE_H264("96");
const string RTSPConnection::VIDEO_PAYLOAD_TYPE_M2TS("33");

RTSPConnection::RtspConnection(RTSPServer * own_server, const StreamSocket & sock, uint32_t ssid)
	: TCPServerConnection(sock)
	, m_own_server(own_server)
	, m_sess_id(ssid)
	, m_rtsp_sess()
	, m_vpt_config(RtspEnvironment::Instance().ConfigInst().VPTConfig())
	, m_apt_config(0)
{
	m_client_ip = socket().peerAddress().host().toString();

	m_buffer_size_request = 1024 * 1024 * 2;
	m_data_size_request = 0;
	m_buffer_request.reset(new uint8_t[m_buffer_size_request]);
}

RTSPConnection::~RTSPConnection()
{}

void RTSPConnection::run()
{
	try
	{
		runRtsp();
	}
	catch (Exception e)
	{
		Environment::instance().logInst().error(Poco::format("Run rtsp server exception: %s[%?d]\r\n", e.message(), e.code()));
	}
	catch (...)
	{
		Environment::instance().logInst().error("Run rtsp server exception: unknown exception\r\n");
	}
	if (m_own_server && m_rtsp_sess.get())
	{
		string sess_id = m_rtsp_sess->sessInfo().sess_id;

		onTeardown(sess_id);
	}
}

void RTSPConnection::runRtsp()
{
	StreamSocket sock = socket();
	Timespan timeout(1, 0);

	Environment::instance().logInst().information(Poco::format("New RTSP connection coming[%s]", sock.peerAddress().toString()));
	while (true)
	{
		bool can_read = sock.poll(timeout, Socket::SELECT_READ | Socket::SELECT_ERROR);

		if (!can_read)
			continue;

		if (handleSocketRead(sock) < 0)
			break;
	}
	Environment::instance().logInst().information(Poco::format("RTSP connection disconnected[%s]", sock.peerAddress().toString()));
}

int32_t RTSPConnection::handleSocketRead(StreamSocket & sock)
{
	int ret = 0;

	m_local_ip = sock.address().host().toString();
	//	--	receive data	--
	ret = sock.receiveBytes(m_buffer_request.get() + m_data_size_request, m_buffer_size_request - m_data_size_request, 0);
	if (ret < 0)
		return 0;
	else if (ret == 0)
		//	--	socket graceful close	--
		return -1;
	m_data_size_request += ret;


	//	--	handle data	--
	int32_t size_handled = 0;

	do 
	{
		if (RTP_INTERLEAVE_MAGIC == m_buffer_request.get()[0])
			size_handled = handleData();
		else
			size_handled = handleRequest();

		if (size_handled > 0)
		{
			memmove(m_buffer_request.get(), m_buffer_request.get() + size_handled, m_data_size_request - size_handled);
			m_data_size_request -= size_handled;
		}
	} while (size_handled > 0);

	return 0;
}

int32_t RTSPConnection::handleData()
{
	static const int32_t data_head_len = 4;

	if (RTP_INTERLEAVE_MAGIC != m_buffer_request.get()[0] || m_data_size_request < data_head_len)
		return 0;


	uint8_t channel_id = m_buffer_request.get()[1];
	uint16_t data_len = ntohs((u_short)*((uint16_t*)&m_buffer_request.get()[2]));

	if (data_head_len + data_len > m_data_size_request)
		return 0;

	return data_head_len + data_len;
}

int32_t RTSPConnection::handleRequest()
{
	char * req = (char *)m_buffer_request.get();
	string resp;
	char * crlf = 0;
	char term_temp = 0;

	req[m_data_size_request] = '\0';
	crlf = strstr((char*)req, "\r\n\r\n");
	if (0 == crlf)
	{
		return -1;
	}
	term_temp = crlf[4];
	crlf[4] = 0;


	char cmd_name[LENGTH_RTSP_PARAMETER_MAX];
	string req_prefix_str;
	char * req_prefix = 0;
	char url_prefix[LENGTH_RTSP_PARAMETER_MAX];
	char url_suffix[LENGTH_RTSP_PARAMETER_MAX];
	char cseq_no[LENGTH_RTSP_PARAMETER_MAX];
	char session_id[LENGTH_RTSP_PARAMETER_MAX];
	unsigned content_length = 0;
	string cseq_no_str;

	if (!parseRTSPRequestString(
		req, 
		crlf - req + 4,
		cmd_name, sizeof(cmd_name),
		&req_prefix,
		url_prefix, sizeof(url_prefix),
		url_suffix, sizeof(url_suffix),
		cseq_no, sizeof(cseq_no),
		session_id, sizeof(session_id),
		content_length
		))
	{
		return -1;
	}
	if (req_prefix)
	{
		req_prefix_str = req_prefix;
		delete req_prefix;
		req_prefix = 0;
	}
	cseq_no_str = cseq_no;
	if (0 == _stricmp(cmd_name, "OPTIONS"))
	{
		resp = handleCmd_OPTIONS(cseq_no_str);
	}
	else if (0 == _stricmp(cmd_name, "DESCRIBE"))
	{
		resp = handleCmd_DESCRIBE(cseq_no_str, url_prefix, url_suffix, (char const*)req);
	}
	else if (0 == _stricmp(cmd_name, "SETUP"))
	{
		resp = handleCmd_SETUP(cseq_no_str, req_prefix_str, url_prefix, url_suffix, req);
	}
	else if (0 == _stricmp(cmd_name, "PLAY"))
	{
		resp = handleCmd_PLAY(cseq_no_str);
	}
	else if (0 == _stricmp(cmd_name, "TEARDOWN"))
	{
		resp = handleCmd_TEARDOWN(cseq_no_str);
	}
	else
	{
		//	--	unknown request command	--
		resp = handleCmd_NotSupported(cseq_no_str);
	}

	sendRtspResponse(resp);

	crlf[4] = term_temp;

	return crlf - req + 4;;
}

string RTSPConnection::handleCmd_OPTIONS(const string & cseq)
{
	string resp;

	Poco::Poco::format(resp,
		"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",
		cseq, dateHeader(), AbstractRtspServer::AllowedCommandNames());

	return resp;
}

string RTSPConnection::handleCmd_DESCRIBE(const string & cseq, char const* url_prefix, char const* url_suffix, char const* full_request)
{
	bool h264_payload(true);
	string sdp_desc;
	string resp;

	if (Config::VPT_MPEG2TS == m_vpt_config)
	{
		
		static const string sdp_desc_fmt = 
		"m=video 0 RTP/AVP %s\r\n"
		;

		Poco::format(sdp_desc, sdp_desc_fmt, VIDEO_PAYLOAD_TYPE_M2TS, string("0.0.0.0"));
	}
	else
	{
		static const string sdp_desc_fmt = 
		"m=video 0 RTP/AVP %s\r\n"
		"a=rtpmap:%s H264/90000\r\n"
		"a=framerate:25\r\n"
		"c=IN IP4 0.0.0.0\r\n"
		;
		Poco::format(sdp_desc, sdp_desc_fmt, VIDEO_PAYLOAD_TYPE_H264, VIDEO_PAYLOAD_TYPE_H264, string("0.0.0.0"));
	}
	Poco::format(resp,
		"RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
		"%s"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %?d\r\n\r\n"

		"%s"
		"\r\n"
		,
		cseq,
		dateHeader(),
		sdp_desc.length(),
		sdp_desc
		);

	return resp;
}

string RTSPConnection::handleCmd_SETUP(const string & cseq, const string & req_prefix, const string & url_prefix, const string & url_suffix, const string & full_request)
{
	char * stream_mode_string = 0; // set when RAW_UDP streaming is specified
	char * client_address_string = 0;
	uint8_t clientsDestinationTTL;
	
	string resp;
	StreamingMode stream_mode;
	RTSPSessInfo sess_info;
	RTSPSession * new_sess(0);

	parseTransportHeader(
		full_request.c_str(), 
		sess_info.stream_mode, 
		stream_mode_string,
		client_address_string, 
		clientsDestinationTTL,
		sess_info.client_rtp_port, 
		sess_info.client_rtcp_port,
		sess_info.rtpChannelId, 
		sess_info.rtcpChannelId);
	if (stream_mode_string)
		sess_info.transport = stream_mode_string;
	else
		sess_info.transport = RTP_UDP_STR;
	if (client_address_string)
		m_client_ip = client_address_string;

	
	int32_t code = 0;
	string prefix = url_prefix;

	sess_info.rtsp_sock = socket();
	if (prefix.empty())
		prefix = url_suffix;
	parsePrefix(sess_info.resourceid, prefix);
	switch (stream_mode)
	{
	case RTP_UDP: 
		{
			sess_info.server_ip = "0.0.0.0";
			sess_info.server_rtp_port = format("%?d", local_rtp_port);
			sess_info.client_ip = m_client_ip;
			sess_info.client_rtp_port = format("%?d", client_rtp_port);
			sess_info.vpt = (Config::VPT_MPEG2TS == m_vpt_config) ? VIDEO_PAYLOAD_TYPE_M2TS : VIDEO_PAYLOAD_TYPE_H264;
			sess_info.apt = "";
			if (0 == (new_sess = onSetup(sess_info))
				resp = handleCmd_Bad(cseq);
			else
			{
				m_rtsp_sess.reset(new_sess);
				sess_info = m_rtsp_sess->sessionInfo();
				local_rtcp_port = local_rtp_port + 1;
				
				
				//	--	response 200 OK	--
				string transport;
				string source_dest_addr;

				source_dest_addr = Poco::format("source=%s", sess_info.server_ip);
				Poco::format(transport,"Transport: RTP/AVP;%s;unicast;client_port=%?d-%?d;server_port=%?d-%?d;\r\n",
					sess_info.server_ip,
					sess_info.client_rtp_port, 
					sess_info.client_rtcp_port, 
					sess_info.server_rtp_port, 
					sess_info.server_rtcp_port);
				Poco::format(resp,
					"RTSP/1.0 200 OK\r\n"
					"CSeq: %s\r\n"
					"%s"
					"%s"
					"Session: %?d;%s\r\n"
					"\r\n"
					,
					cseq,
					dateHeader(),
					transport,
					m_sess_id,
					string("timeout=65")
					);
			}
		}
		break;

	case RTP_TCP:
		{
			sess_info.server_ip = "0.0.0.0";
			sess_info.server_rtp_port = format("%?d", local_rtp_port);
			//	--	different from RTP_UDP mode	--
			sess_info.vpt = (Config::VPT_MPEG2TS == m_vpt_config) ? VIDEO_PAYLOAD_TYPE_M2TS : VIDEO_PAYLOAD_TYPE_H264;
			sess_info.apt = "";
			if (0 == (new_sess = onSetup(sess_info))
				resp = handleCmd_Bad(cseq);
			else
			{
				m_rtsp_sess.reset(new_sess);
				sess_info = m_rtsp->sessionInfo();
				
				
				//	--	response 200 OK	--
				string transport;
				string source_dest_addr;
				
				Poco::format(transport,"Transport: RTP/AVP/TCP;interleaved=0-1;\r\n",
					(uint32_t)(_atoi64(ssrc.c_str()))
					);
				Poco::format(resp,
					"RTSP/1.0 200 OK\r\n"
					"CSeq: %s\r\n"
					"%s"
					"%s"
					"Session: %?d;%s\r\n"
					"\r\n"
					,
					cseq,
					dateHeader(),
					transport,
					m_sess_id,
					string("timeout=65")
					);
			}
		}
		break;

	default:
		resp = handleCmd_UnsupportedTransport(cseq);
		break;
	}

	if (stream_mode_string)
		delete []stream_mode_string;
	if (client_address_string)
		delete []client_address_string;

	return resp;
}

string RTSPConnection::handleCmd_PLAY(const string & cseq)
{
	onPlay();

	
	string range = "Range: npt=0.000-\r\n";
	string rtp_info = "RTP-Info: seq=0;rtptime=0\r\n";
	string resp;

	Poco::format(resp,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"%s"
		"%s"
		"\r\n",

		cseq,
		dateHeader(),
		range
		, rtp_info
		);

	return resp;
}

string RTSPConnection::handleCmd_TEARDOWN(const string & cseq)
{
	onTeardown();


	string resp;

	Poco::format(resp,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"\r\n",

		cseq,
		dateHeader()
		);

	return resp;
}

string RTSPConnection::handleCmd_Bad(const string & cseq) 
{
	string resp;

	Poco::format(resp,
		"RTSP/1.0 400 Bad Request\r\n%sAllow: %s\r\n\r\n",
		dateHeader(), 
		RTSPServer::allowedCommandNames());

	return resp;
}

string RTSPConnection::HandleCmd_NotSupported(const string & cseq) 
{
	string resp;

	Poco::format(resp,
		"RTSP/1.0 405 Method Not Allowed\r\nCSeq: %s\r\n%sAllow: %s\r\n\r\n",
		cseq, dateHeader(), RtspServer::allowedCommandNames());

	return resp;
}

string RTSPConnection::handleCmd_NotFound(const string & cseq) 
{
	return setRtspResponse("404 Stream Not Found", cseq);
}

string RTSPConnection::handleCmd_SessionNotFound(const string & cseq) 
{
	return setRtspResponse("454 Session Not Found", cseq);
}

string RTSPConnection::handleCmd_MethodNotValid(const string & cseq)
{
	return setRtspResponse("455 Method Not Valid in This State", cseq);
}

string RTSPConnection::handleCmd_UnsupportedTransport(const string & cseq) 
{
	return setRtspResponse("461 Unsupported Transport", cseq);
}

string RTSPConnection::handleCmd_NotImplemented(const string & cseq)
{
	return setRtspResponse("501 Not Implemented", cseq);
}

int32_t RTSPConnection::onSetup(RTSPSession *& sess, string & server_ip, uint16_t & rtp_port, string & ssrc, const RTSPSessInfo & sess_info)
{
	if (m_own_server)
		return m_own_server->onSetup(sess, server_ip, rtp_port, ssrc, sess_info);

	return -1;
}

int32_t RTSPConnection::onPlay(const string & sess_id)
{
	return 0;
}

int32_t RTSPConnection::onTeardown(const string & sess_id)
{
	if (m_own_server)
	{
		m_own_server->onTeardown(sess_id);
		m_own_server = 0;
	}
	m_rtsp_sess.reset();

	return 0;
}

string RTSPConnection::setRtspResponse(char const* response_string, const string & cseq)
{
	string resp;

	Poco::format(resp,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"\r\n"
		,
		string(response_string),
		string(cseq),
		dateHeader());

	return resp;
}

int32_t RTSPConnection::sendRtspResponse(const string & resp)
{
	if (resp.empty())
		return -1;

	const uint8_t * resp_ptr = (const uint8_t *)resp.c_str();
	int32_t resp_len = resp.length();

	do 
	{
		int bytes_sent = socket().sendBytes(resp_ptr, resp_len, 0);

		if (bytes_sent < 0)
			break;
		else if (0 == bytes_sent)
			Poco::Thread::sleep(20);

		resp_ptr += bytes_sent;
		resp_len -= bytes_sent;
	} while (resp_len > 0);

	return 0;
}

bool RTSPConnection::parseRTSPRequestString(char const* reqStr,
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
	unsigned& contentLength) 
{
	// This parser is currently rather dumb; it should be made smarter #####

	// "Be liberal in what you accept": Skip over any whitespace at the start of the request:
	unsigned i;
	for (i = 0; i < reqStrSize; ++i) {
		char c = reqStr[i];
		if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0')) break;
	}
	if (i == reqStrSize) return false; // The request consisted of nothing but whitespace!

	// Then read everything up to the next space (or tab) as the command name:
	bool parseSucceeded = false;
	unsigned i1 = 0;
	for (; i1 < resultCmdNameMaxSize-1 && i < reqStrSize; ++i,++i1) {
		char c = reqStr[i];
		if (c == ' ' || c == '\t') {
			parseSucceeded = true;
			break;
		}

		resultCmdName[i1] = c;
	}
	resultCmdName[i1] = '\0';
	if (!parseSucceeded) return false;

	// Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
	unsigned j = i+1;
	while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) ++j; // skip over any additional white space
	for (; (int)j < (int)(reqStrSize-8); ++j) {
		if ((reqStr[j] == 'r' || reqStr[j] == 'R')
			&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
			&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
			&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
			&& reqStr[j+4] == ':' && reqStr[j+5] == '/') {
				j += 6;
				if (reqStr[j] == '/') {
					// This is a "rtsp://" URL; skip over the host:port part that follows:
					++j;
					while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') ++j;
				} else {
					// This is a "rtsp:/" URL; back up to the "/":
					--j;
				}
				//	--	copy host&port	--
				if (reqPrefix)
				{
					unsigned req_prefix_size = j - i + 1;

					*reqPrefix = new char[req_prefix_size + 1];
					memcpy(*reqPrefix, &reqStr[i], req_prefix_size);
					(*reqPrefix)[req_prefix_size] = 0;
				}

				i = j;
				break;
		}
	}

	// Look for the URL suffix (before the following "RTSP/"):
	parseSucceeded = false;
	for (unsigned k = i+1; (int)k < (int)(reqStrSize-5); ++k) {
		if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
			reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/') {
				while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"
				unsigned k1 = k;
				while (k1 > i && reqStr[k1] != '/') --k1;

				// ASSERT: At this point
				//   i: first space or slash after "host" or "host:port"
				//   k: last non-space before "RTSP/"
				//   k1: last slash in the range [i,k]

				// The URL suffix comes from [k1+1,k]
				// Copy "resultURLSuffix":
				unsigned n = 0, k2 = k1+1;
				if (k2 <= k) {
					if (k - k1 + 1 > resultURLSuffixMaxSize) return false; // there's no room
					while (k2 <= k) resultURLSuffix[n++] = reqStr[k2++];
				}
				resultURLSuffix[n] = '\0';

				// The URL 'pre-suffix' comes from [i+1,k1-1]
				// Copy "resultURLPreSuffix":
				n = 0; k2 = i+1;
				if (k2+1 <= k1) {
					if (k1 - i > resultURLPreSuffixMaxSize) return false; // there's no room
					while (k2 <= k1-1) resultURLPreSuffix[n++] = reqStr[k2++];
				}
				resultURLPreSuffix[n] = '\0';
				decodeURL(resultURLPreSuffix);

				i = k + 7; // to go past " RTSP/"
				parseSucceeded = true;
				break;
		}
	}
	if (!parseSucceeded) return false;

	// Look for "CSeq:" (mandatory, case insensitive), skip whitespace,
	// then read everything up to the next \r or \n as 'CSeq':
	parseSucceeded = false;
	for (j = i; (int)j < (int)(reqStrSize-5); ++j) {
		if (_strnicmp("CSeq:", &reqStr[j], 5) == 0) {
			j += 5;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned n;
			for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j) {
				char c = reqStr[j];
				if (c == '\r' || c == '\n') {
					parseSucceeded = true;
					break;
				}

				resultCSeq[n] = c;
			}
			resultCSeq[n] = '\0';
			break;
		}
	}
	if (!parseSucceeded) return false;

	// Look for "Session:" (optional, case insensitive), skip whitespace,
	// then read everything up to the next \r or \n as 'Session':
	resultSessionIdStr[0] = '\0'; // default value (empty string)
	for (j = i; (int)j < (int)(reqStrSize-8); ++j) {
		if (_strnicmp("Session:", &reqStr[j], 8) == 0) {
			j += 8;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned n;
			for (n = 0; n < resultSessionIdStrMaxSize-1 && j < reqStrSize; ++n,++j) {
				char c = reqStr[j];
				if (c == '\r' || c == '\n') {
					break;
				}

				resultSessionIdStr[n] = c;
			}
			resultSessionIdStr[n] = '\0';
			break;
		}
	}

	// Also: Look for "Content-Length:" (optional, case insensitive)
	contentLength = 0; // default value
	for (j = i; (int)j < (int)(reqStrSize-15); ++j) {
		if (_strnicmp("Content-Length:", &(reqStr[j]), 15) == 0) {
			j += 15;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned num;
			if (sscanf(&reqStr[j], "%u", &num) == 1) {
				contentLength = num;
			}
		}
	}

	return true;
}

void RTSPConnection::parseTransportHeader(char const* request,
	StreamingMode& streamingMode,
	char*& streamingModeString,
	char*& destinationAddressStr,
	uint8_t & destinationTTL,
	uint16_t & clientRTPPortNum, // if UDP
	uint16_t & clientRTCPPortNum, // if UDP
	uint8_t & rtpChannelId, // if TCP
	uint8_t & rtcpChannelId // if TCP
	) 
{
		// Initialize the result parameters to default values:
		streamingMode = RTP_UDP;
		streamingModeString = NULL;
		destinationAddressStr = NULL;
		destinationTTL = 255;
		clientRTPPortNum = 0;
		clientRTCPPortNum = 1;
		rtpChannelId = rtcpChannelId = 0xFF;

		uint16_t p1, p2;
		uint32_t ttl, rtpCid, rtcpCid;

		// First, find "Transport:"
		while (1) 
		{
			if (*request == '\0') return; // not found
			if (*request == '\r' && *(request + 1) == '\n' && *(request+2) == '\r') return; // end of the headers => not found
			if (_strnicmp(request, "Transport:", 10) == 0) break;
			++ request;
		}

		// Then, run through each of the fields, looking for ones we handle:
		char const* fields = request + 10;
		while (*fields == ' ') ++fields;
		char* field = strdup(fields);

		while (sscanf(fields, "%[^;\r\n]", field) == 1) 
		{
			if (strcmp(field, "RTP/AVP") == 0)
			{
				streamingMode = RTP_UDP;
				streamingModeString = strdup(field);
			}
			else if (strcmp(field, "RTP/AVP/TCP") == 0) 
			{
				streamingMode = RTP_TCP;
				streamingModeString = strdup(field);
			} 
			else if (strcmp(field, "RAW/RAW/UDP") == 0 || strcmp(field, "MP2T/H2221/UDP") == 0) 
			{
				streamingMode = RAW_UDP;
				streamingModeString = strdup(field);
			} 
			else if (_strnicmp(field, "destination=", 12) == 0) 
			{
				delete[] destinationAddressStr;
				destinationAddressStr = strdup(field+12);
			}
			else if (sscanf(field, "ttl%u", &ttl) == 1) 
			{
				destinationTTL = (UInt8)ttl;
			} 
			else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) 
			{
				clientRTPPortNum = p1;
				clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p2; // ignore the second port number if the client asked for raw UDP
			} 
			else if (sscanf(field, "client_port=%hu", &p1) == 1) 
			{
				clientRTPPortNum = p1;
				clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p1 + 1;
			} 
			else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) 
			{
				rtpChannelId = (unsigned char)rtpCid;
				rtcpChannelId = (unsigned char)rtcpCid;
			}

			fields += strlen(field);
			while (*fields == ';' || *fields == ' ' || *fields == '\t') ++fields; // skip over separating ';' chars or whitespace
			if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
		}
		delete[] field;
}

int32_t RTSPConnection::parsePrefix(string & resourceid, const string & prefix)
{
	//	--	
	//	--	/video?resourceid=762cd502-0350-40e2-8f15-e6ea72aa0033
	static const std::string mark_resouceid("video?resourceid=");
	size_t pos = prefix.find(mark_resourceid);
	size_t start(0);
	
	if (std::string::npos == pos)
		return -1;
	start = pos + mark_resourceid.length();
	resourceid = prefix.substr(start, prefix.length() - start);

	return 0;
}

string RTSPConnection::dateHeader() 
{
	static char buf[200];

#if !defined(_WIN32_WCE)
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
#else
	// WinCE apparently doesn't have "time()", "strftime()", or "gmtime()",
	// so generate the "Date:" header a different, WinCE-specific way.
	// (Thanks to Pierre l'Hussiez for this code)
	// RSF: But where is the "Date: " string?  This code doesn't look quite right...
	SYSTEMTIME SystemTime;
	GetSystemTime(&SystemTime);
	WCHAR dateFormat[] = L"ddd, MMM dd yyyy";
	WCHAR timeFormat[] = L"HH:mm:ss GMT\r\n";
	WCHAR inBuf[200];
	DWORD locale = LOCALE_NEUTRAL;

	int ret = GetDateFormat(locale, 0, &SystemTime,
		(LPTSTR)dateFormat, (LPTSTR)inBuf, sizeof inBuf);
	inBuf[ret - 1] = ' ';
	ret = GetTimeFormat(locale, 0, &SystemTime,
		(LPTSTR)timeFormat,
		(LPTSTR)inBuf + ret, (sizeof inBuf) - ret);
	wcstombs(buf, inBuf, wcslen(inBuf));
#endif
	return string(buf);
}

void RTSPConnection::decodeURL(char* url) 
{
	// Replace (in place) any %<hex><hex> sequences with the appropriate 8-bit character.
	char* cursor = url;
	while (*cursor) 
	{
		if ((cursor[0] == '%') &&
			cursor[1] && isxdigit(cursor[1]) &&
			cursor[2] && isxdigit(cursor[2])) 
		{
			// We saw a % followed by 2 hex digits, so we copy the literal hex value into the URL, then advance the cursor past it:
			char hex[3];
			hex[0] = cursor[1];
			hex[1] = cursor[2];
			hex[2] = '\0';
			*url++ = (char)strtol(hex, NULL, 16);
			cursor += 3;
		} else 
		{
			// Common case: This is a normal character or a bogus % expression, so just copy it
			*url++ = *cursor++;
		}
	}

	*url = '\0';
}
