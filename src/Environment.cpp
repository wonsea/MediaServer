#include "common.h"
#include "Environment.h"


Environment & Environment::instance()
{
	static Environment env;
	
	return env;
}

Environment::Environment()
{
	m_logger = Logger::get("MediaServer");
}

Environment::~Environment()
{}

Poco::Logger & Environment::logger()
{
	return m_logger;
}