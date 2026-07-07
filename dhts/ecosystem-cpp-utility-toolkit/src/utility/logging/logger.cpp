#include "nexus/utility/logging/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
namespace nexus::utility::logging {
Logger::Logger(){default_console_sink_=std::make_shared<ConsoleSink>();sinks_.push_back(default_console_sink_);}
Logger::~Logger(){flush();}
Logger& Logger::instance(){static Logger l;return l;}
void Logger::log(LogLevel lv,const std::string& m,std::source_location loc){if(lv<min_level_)return;std::lock_guard<std::mutex> lk(mutex_);auto fm=formatMessage(lv,m,loc);LogRecord r{m,lv,fm};for(auto& s:sinks_)if(s)s->write(r);}
void Logger::addSink(std::shared_ptr<LogSink> s){if(!s)return;std::lock_guard<std::mutex> lk(mutex_);sinks_.push_back(std::move(s));}
void Logger::clearSinks(){std::lock_guard<std::mutex> lk(mutex_);sinks_.clear();}
void Logger::addConsoleSink(){std::lock_guard<std::mutex> lk(mutex_);for(auto& s:sinks_)if(dynamic_cast<ConsoleSink*>(s.get()))return;sinks_.push_back(std::make_shared<ConsoleSink>());}
void Logger::addFileSink(const std::string& fn){std::lock_guard<std::mutex> lk(mutex_);sinks_.push_back(std::make_shared<FileSink>(fn));}
void Logger::addRotatingFileSink(const std::string& fn,size_t ms,int mf){std::lock_guard<std::mutex> lk(mutex_);sinks_.push_back(std::make_shared<RotatingFileSink>(fn,ms,mf));}
void Logger::setLogLevel(LogLevel lv)noexcept{std::lock_guard<std::mutex> lk(mutex_);min_level_=lv;}
LogLevel Logger::getLogLevel()const noexcept{std::lock_guard<std::mutex> lk(mutex_);return min_level_;}
void Logger::setConsoleOutput(bool e){std::lock_guard<std::mutex> lk(mutex_);console_output_=e;if(e){for(auto& s:sinks_)if(dynamic_cast<ConsoleSink*>(s.get()))return;sinks_.push_back(std::make_shared<ConsoleSink>());}}
bool Logger::isConsoleOutput()const noexcept{std::lock_guard<std::mutex> lk(mutex_);return console_output_;}
void Logger::setTimestamps(bool e){std::lock_guard<std::mutex> lk(mutex_);timestamps_=e;}
bool Logger::isTimestamps()const noexcept{std::lock_guard<std::mutex> lk(mutex_);return timestamps_;}
void Logger::flush(){std::lock_guard<std::mutex> lk(mutex_);for(auto& s:sinks_)if(s)s->flush();}
std::string Logger::formatMessage(LogLevel lv,const std::string& m,const std::source_location& loc)const{std::ostringstream os;if(timestamps_){auto n=std::chrono::system_clock::now();auto t=std::chrono::system_clock::to_time_t(n);auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(n.time_since_epoch())%1000;os<<std::put_time(std::localtime(&t),"%Y-%m-%d %H:%M:%S")<<'.'<<std::setfill('0')<<std::setw(3)<<ms.count()<<' ';}os<<'['<<levelToString(lv)<<"] ";if(loc.file_name())os<<loc.file_name()<<':'<<loc.line()<<' ';os<<m;return os.str();}
const char* Logger::levelToString(LogLevel lv)noexcept{switch(lv){case LogLevel::Trace:return"TRACE";case LogLevel::Debug:return"DEBUG";case LogLevel::Info:return"INFO";case LogLevel::Warning:return"WARN";case LogLevel::Error:return"ERROR";case LogLevel::Critical:return"CRITICAL";default:return"UNKNOWN";}}
} // namespace nexus::utility::logging
