#include "common.h"
#include "RTSPServer.h"
#include "RTSPConnection.h"
#include "RTSPSession.h"


RTSPServer::RTSPServer(uint16_t serv_port)
	: TCPServer(new RTSPConnectionFactory(this), ServerSocket(serv_port))
{}

RTSPServer::~RTSPServer()
{}

void RTSPServer::start() 
{
	TCPServer::start();
}

RTSPSession * RTSPServer::onSessionSetup(const RtspSessInfo & sess_info) 
{}

int32_t RTSPServer::onSessionPlay() 
{}

int32_t RTSPServer::onSessionTeardown() 
{}