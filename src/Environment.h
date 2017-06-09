#ifndef _ENVIRONMENT_H_
#define _ENVIRONMENT_H_


class Environment
{
public:
	static Environment & instance();

	Environment();
	~Environment();
	
	Poco::Logger & logger();
private:
	Poco::Logger m_logger;
};


#endif