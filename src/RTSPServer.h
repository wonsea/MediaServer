#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_


class RTSPServer
	: public TCPServer
{
public:
	RTSPServer(uint16_t serv_port);
	~RTSPServer();

	void start() ;
	
	RTSPSession * onSessionSetup(const RtspSessInfo & sess_info) ;
	int32_t onSessionPlay() ;
	int32_t onSessionTeardown() ;
};

#endif