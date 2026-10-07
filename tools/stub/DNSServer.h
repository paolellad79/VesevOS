#pragma once
#include <WiFi.h>
enum class DNSReplyCode{NoError=0,ServerFailure=2,NonExistentDomain=3};
class DNSServer{public: bool start(uint16_t,const String&,const IPAddress&){return true;} void stop(){} void processNextRequest(){} void setErrorReplyCode(DNSReplyCode){} };
