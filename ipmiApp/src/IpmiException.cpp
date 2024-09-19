/** 
 * 
 * 
 * 
 * 
*/

#include "IpmiException.h"

IpmiException::IpmiException(int ecode, std::string &&estring)
:m_errorCode(ecode), m_errorStr(std::move(estring)), m_msg()
{
    
}

IpmiException::IpmiException(int ecode, std::string &&estring, std::string &&message)
:m_errorCode(ecode), m_errorStr(std::move(estring)), m_msg(std::move(message))
{
    
}

IpmiException::~IpmiException() {
    
}

int IpmiException::getErrorCode() const {
    return this->m_errorCode;
}

const std::string &IpmiException::getErrorString() const {
    return this->m_errorStr;
}

const char *IpmiException::what() const noexcept {
    return this->m_msg.c_str();
}

