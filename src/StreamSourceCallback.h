#ifndef _STREAM_SOURCE_CALLBACK_H_
#define _STREAM_SOURCE_CALLBACK_H_


class StreamSourceCallback
{
public:
	StreamSourceCallback(){}
	virtural ~StreamSourceCallback(){}
	
	virtural int32_t StreamOut(const uint8_t * data, int32_t data_size) = 0;
};


#endif