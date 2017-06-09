#ifndef _STREAM_SOURCE_H_
#define _STREAM_SOURCE_H_


#include "StreamSourceData.h"
#include "StreamSourceCallback.h"
	
class StreamSource
	: public Runnable
{
public:
	StreamSource();
	~StreamSource();
	
	int32_t start(StreamSourceCallback * callback);
	int32_t stop();
	
	//	--	for Runnable	--
	virtual void run();
	
private:
	int32_t doStreamSource();
	
private:
	Poco::Thread m_thread;
	bool m_stop;

	StreamSourceCallback * m_callback;
};


#endif