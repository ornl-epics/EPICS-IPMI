/** 
 * 
 * 
 * 
 */

#ifndef IPMIAPP_SRC_IPMIEXCEPTION_H_
#define IPMIAPP_SRC_IPMIEXCEPTION_H_

#include <string>

class IpmiException : public std::exception {

private:
    int m_errorCode;
    const std::string m_errorStr;
    const std::string m_msg;

public:
    IpmiException(int ecode, std::string &&estring);
    IpmiException(int ecode, std::string &&estring, std::string &&message);
    ~IpmiException();
    int getErrorCode() const;
    const std::string &getErrorString() const;
    const char * what() const noexcept override;
    
};

#endif 
