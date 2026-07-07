#include "nexus/utility/error/error_reporter.h"
#include <iostream>
namespace nexus::utility::error {
ErrorReporter& ErrorReporter::instance(){static ErrorReporter r;return r;}
void ErrorReporter::reportError(const std::string& m,std::source_location l){std::shared_lock lk(mutex_);if(error_callback_)error_callback_(m,l);else std::cerr<<"[ERROR] "<<l.file_name()<<":"<<l.line()<<" "<<m<<"\n";}
void ErrorReporter::reportWarning(const std::string& m,std::source_location l){std::shared_lock lk(mutex_);if(warning_callback_)warning_callback_(m,l);else std::cerr<<"[WARN] "<<l.file_name()<<":"<<l.line()<<" "<<m<<"\n";}
void ErrorReporter::setErrorCallback(ErrorCallback cb){std::unique_lock lk(mutex_);error_callback_=std::move(cb);}
void ErrorReporter::setWarningCallback(ErrorCallback cb){std::unique_lock lk(mutex_);warning_callback_=std::move(cb);}
} // namespace nexus::utility::error
