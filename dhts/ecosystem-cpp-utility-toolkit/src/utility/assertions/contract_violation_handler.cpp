#include "nexus/utility/assertions/contract_violation_handler.h"
#include <iostream>
namespace nexus::utility::assertions {
ViolationPolicy ContractViolationHandler::policy_=ViolationPolicy::Abort;
ContractViolationHandler::ViolationCallback ContractViolationHandler::callback_;
void ContractViolationHandler::setPolicy(ViolationPolicy p)noexcept{policy_=p;}
ViolationPolicy ContractViolationHandler::getPolicy()noexcept{return policy_;}
void ContractViolationHandler::setCallback(ViolationCallback cb){callback_=std::move(cb);}
void ContractViolationHandler::handleViolation(const char* t,const char* e,const char* m,std::source_location l,nexus::utility::stacktrace tr){if(callback_){callback_(t,e,m,l,tr);return;}std::cerr<<"\nCONTRACT VIOLATION: "<<t<<" - "<<m<<"\n"<<tr<<"\n";switch(policy_){case ViolationPolicy::Abort:std::abort();break;case ViolationPolicy::Throw:throw ContractViolationException(std::string(t)+": "+m);case ViolationPolicy::LogAndContinue:default:break;}}
} // namespace nexus::utility::assertions
