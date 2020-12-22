#pragma once
#include <stdexcept>

class DbWrapperException : public std::runtime_error
{
public:
	DbWrapperException(const char* msg) :
		runtime_error(msg) {}
};

