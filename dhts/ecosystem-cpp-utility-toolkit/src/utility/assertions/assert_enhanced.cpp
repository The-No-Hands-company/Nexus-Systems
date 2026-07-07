#include "nexus/utility/assertions/assert_enhanced.h"
#include <iostream>
namespace nexus::utility::assertions {
static AssertionHandler g_h;
void EnhancedAssert::assertFailed(const char* e,const char* m,std::source_location l){auto t=nexus::utility::stacktrace::current();if(g_h){g_h(e,m,l,t);return;}std::cerr<<"\nASSERT FAILED: "<<e<<" - "<<m<<" at "<<l.file_name()<<":"<<l.line()<<"\n"<<t<<"\n";std::abort();}
void EnhancedAssert::setHandler(AssertionHandler h){g_h=std::move(h);}
void EnhancedAssert::resetHandler(){g_h=nullptr;}
} // namespace nexus::utility::assertions
