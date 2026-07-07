#!/usr/bin/env python3
"""Flesh out graphics (8), database (8), cpp23 (8) skeletons."""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC, SRC = f"{BASE}/include/nexus/utility", f"{BASE}/src/utility"
def gen(cat, name, h, s):
    os.makedirs(f"{INC}/{cat}", exist_ok=True); os.makedirs(f"{SRC}/{cat}", exist_ok=True)
    with open(f"{INC}/{cat}/{name}.h",'w') as f: f.write(h)
    with open(f"{SRC}/{cat}/{name}.cpp",'w') as f: f.write(s)

# === GRAPHICS (8) ===
for item in [
('compute_kernel_profiler','''#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::graphics {
class ComputeKernelProfiler {
public:
    struct KernelStats { std::string name; size_t workGroupsX,workGroupsY,workGroupsZ; std::chrono::microseconds duration{0}; size_t invocations; };
    static ComputeKernelProfiler& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordKernel(const std::string& name, size_t gx,size_t gy,size_t gz,std::chrono::microseconds dur);
    std::vector<KernelStats> getKernels() const;
    std::chrono::microseconds getTotalTime() const;
    void clear();
private:
    ComputeKernelProfiler()=default; ~ComputeKernelProfiler()=default; bool enabled_=false;
    std::vector<KernelStats> kernels_;
};
}''','''#include "nexus/utility/graphics/compute_kernel_profiler.h"
namespace nexus::utility::graphics {
ComputeKernelProfiler& ComputeKernelProfiler::instance(){static ComputeKernelProfiler i;return i;}
void ComputeKernelProfiler::initialize(){enabled_=true;kernels_.clear();}
void ComputeKernelProfiler::shutdown(){enabled_=false;}
bool ComputeKernelProfiler::isEnabled()const{return enabled_;}
void ComputeKernelProfiler::recordKernel(const std::string& n,size_t x,size_t y,size_t z,std::chrono::microseconds d){kernels_.push_back({n,x,y,z,d,x*y*z});}
auto ComputeKernelProfiler::getKernels()const->std::vector<KernelStats>{return kernels_;}
std::chrono::microseconds ComputeKernelProfiler::getTotalTime()const{std::chrono::microseconds t{0};for(auto&k:kernels_)t+=k.duration;return t;}
void ComputeKernelProfiler::clear(){kernels_.clear();}
}'''),
('dx12_debug_layer_wrapper','''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class Dx12DebugLayerWrapper {
public:
    struct Dx12Message { std::string category; int severity; std::string message; };
    static Dx12DebugLayerWrapper& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordMessage(const std::string& cat, int sev, const std::string& msg);
    std::vector<Dx12Message> getMessages() const;
    size_t getMessageCount() const;
    size_t getErrorCount() const;
    void clear();
private:
    Dx12DebugLayerWrapper()=default; ~Dx12DebugLayerWrapper()=default; bool enabled_=false;
    std::vector<Dx12Message> messages_;
};
}''','''#include "nexus/utility/graphics/dx12_debug_layer_wrapper.h"
namespace nexus::utility::graphics {
Dx12DebugLayerWrapper& Dx12DebugLayerWrapper::instance(){static Dx12DebugLayerWrapper i;return i;}
void Dx12DebugLayerWrapper::initialize(){enabled_=true;messages_.clear();}
void Dx12DebugLayerWrapper::shutdown(){enabled_=false;}
bool Dx12DebugLayerWrapper::isEnabled()const{return enabled_;}
void Dx12DebugLayerWrapper::recordMessage(const std::string& c,int s,const std::string& m){messages_.push_back({c,s,m});}
auto Dx12DebugLayerWrapper::getMessages()const->std::vector<Dx12Message>{return messages_;}
size_t Dx12DebugLayerWrapper::getMessageCount()const{return messages_.size();}
size_t Dx12DebugLayerWrapper::getErrorCount()const{size_t c=0;for(auto&m:messages_)if(m.severity>=3)c++;return c;}
void Dx12DebugLayerWrapper::clear(){messages_.clear();}
}'''),
('framebuffer_validator','''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class FramebufferValidator {
public:
    struct FbStatus { bool complete; std::vector<std::string> attachments; std::vector<std::string> errors; };
    static FramebufferValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    FbStatus validate(size_t colorAttachments, bool hasDepth, bool hasStencil);
    std::vector<FbStatus> getHistory() const;
    size_t getValidationCount() const;
    void clear();
private:
    FramebufferValidator()=default; ~FramebufferValidator()=default; bool enabled_=false;
    std::vector<FbStatus> history_;
};
}''','''#include "nexus/utility/graphics/framebuffer_validator.h"
namespace nexus::utility::graphics {
FramebufferValidator& FramebufferValidator::instance(){static FramebufferValidator i;return i;}
void FramebufferValidator::initialize(){enabled_=true;history_.clear();}
void FramebufferValidator::shutdown(){enabled_=false;}
bool FramebufferValidator::isEnabled()const{return enabled_;}
FramebufferValidator::FbStatus FramebufferValidator::validate(size_t ca,bool d,bool s){
    FbStatus r{true,{},{} };
    for(size_t i=0;i<ca;i++)r.attachments.push_back("color_"+std::to_string(i));
    if(d)r.attachments.push_back("depth");if(s)r.attachments.push_back("stencil");
    if(ca==0&&!d){r.complete=false;r.errors.push_back("No attachments");}
    history_.push_back(r);return r;
}
auto FramebufferValidator::getHistory()const->std::vector<FbStatus>{return history_;}
size_t FramebufferValidator::getValidationCount()const{return history_.size();}
void FramebufferValidator::clear(){history_.clear();}
}'''),
('gpu_hang_detector','''#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::graphics {
class GpuHangDetector {
public:
    struct GpuOperation { std::string name; std::chrono::milliseconds elapsed{0}; bool timedOut; bool recovered; };
    static GpuHangDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void startOperation(const std::string& name);
    void endOperation(const std::string& name, bool recovered=true);
    std::vector<GpuOperation> getOperations() const;
    size_t getTimeoutCount() const;
    void setTimeoutThreshold(std::chrono::milliseconds threshold);
    void clear();
private:
    GpuHangDetector()=default; ~GpuHangDetector()=default; bool enabled_=false;
    std::chrono::milliseconds threshold_{2000};
    std::unordered_map<std::string,std::chrono::steady_clock::time_point> active_;
    std::vector<GpuOperation> history_;
};
}''','''#include "nexus/utility/graphics/gpu_hang_detector.h"
namespace nexus::utility::graphics {
GpuHangDetector& GpuHangDetector::instance(){static GpuHangDetector i;return i;}
void GpuHangDetector::initialize(){enabled_=true;history_.clear();active_.clear();}
void GpuHangDetector::shutdown(){enabled_=false;}
bool GpuHangDetector::isEnabled()const{return enabled_;}
void GpuHangDetector::startOperation(const std::string& n){if(!enabled_)return;active_[n]=std::chrono::steady_clock::now();}
void GpuHangDetector::endOperation(const std::string& n,bool rec){auto it=active_.find(n);if(it==active_.end())return;auto d=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-it->second);history_.push_back({n,d,d>threshold_,rec});active_.erase(it);}
auto GpuHangDetector::getOperations()const->std::vector<GpuOperation>{return history_;}
size_t GpuHangDetector::getTimeoutCount()const{size_t c=0;for(auto&o:history_)if(o.timedOut)c++;return c;}
void GpuHangDetector::setTimeoutThreshold(std::chrono::milliseconds t){threshold_=t;}
void GpuHangDetector::clear(){history_.clear();active_.clear();}
}'''),
('pipeline_state_dumper','''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class PipelineStateDumper {
public:
    struct PipelineState { std::string shader; std::string blendMode; std::string depthTest; std::string cullMode; bool wireframe; };
    static PipelineStateDumper& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void setState(const std::string& shader, const std::string& blend, const std::string& depth, const std::string& cull, bool wireframe=false);
    PipelineState getCurrent() const;
    std::vector<PipelineState> getHistory() const;
    size_t getStateChangeCount() const;
    void clear();
private:
    PipelineStateDumper()=default; ~PipelineStateDumper()=default; bool enabled_=false;
    PipelineState current_;
    std::vector<PipelineState> history_;
};
}''','''#include "nexus/utility/graphics/pipeline_state_dumper.h"
namespace nexus::utility::graphics {
PipelineStateDumper& PipelineStateDumper::instance(){static PipelineStateDumper i;return i;}
void PipelineStateDumper::initialize(){enabled_=true;history_.clear();}
void PipelineStateDumper::shutdown(){enabled_=false;}
bool PipelineStateDumper::isEnabled()const{return enabled_;}
void PipelineStateDumper::setState(const std::string& s,const std::string& b,const std::string& d,const std::string& c,bool w){current_={s,b,d,c,w};history_.push_back(current_);}
auto PipelineStateDumper::getCurrent()const->PipelineState{return current_;}
auto PipelineStateDumper::getHistory()const->std::vector<PipelineState>{return history_;}
size_t PipelineStateDumper::getStateChangeCount()const{return history_.size();}
void PipelineStateDumper::clear(){history_.clear();}
}'''),
('shader_compiler_error_parser','''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class ShaderCompilerErrorParser {
public:
    struct ShaderError { std::string shaderFile; int line; int column; std::string message; bool isError; };
    static ShaderCompilerErrorParser& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    ShaderError parse(const std::string& errorLine);
    std::vector<ShaderError> parseLog(const std::string& log);
    std::vector<ShaderError> getErrors() const;
    size_t getErrorCount() const;
    size_t getWarningCount() const;
    void clear();
private:
    ShaderCompilerErrorParser()=default; ~ShaderCompilerErrorParser()=default; bool enabled_=false;
    std::vector<ShaderError> errors_;
};
}''','''#include "nexus/utility/graphics/shader_compiler_error_parser.h"
#include <regex>
namespace nexus::utility::graphics {
ShaderCompilerErrorParser& ShaderCompilerErrorParser::instance(){static ShaderCompilerErrorParser i;return i;}
void ShaderCompilerErrorParser::initialize(){enabled_=true;errors_.clear();}
void ShaderCompilerErrorParser::shutdown(){enabled_=false;}
bool ShaderCompilerErrorParser::isEnabled()const{return enabled_;}
auto ShaderCompilerErrorParser::parse(const std::string& l)->ShaderError{
    ShaderError e{"",0,0,"",true};std::smatch m;
    if(std::regex_search(l,m,std::regex(R"(ERROR:)")))e.isError=true;else e.isError=false;
    if(std::regex_search(l,m,std::regex(R"((\\S+):(\\d+):(\\d+))"))){e.shaderFile=m[1];e.line=std::stoi(m[2]);e.column=std::stoi(m[3]);}
    e.message=l;errors_.push_back(e);return e;
}
auto ShaderCompilerErrorParser::parseLog(const std::string& log)->std::vector<ShaderError>{std::vector<ShaderError> r;std::istringstream ss(log);std::string l;while(std::getline(ss,l))if(l.find("ERROR")!=std::string::npos||l.find("WARNING")!=std::string::npos)r.push_back(parse(l));return r;}
auto ShaderCompilerErrorParser::getErrors()const->std::vector<ShaderError>{return errors_;}
size_t ShaderCompilerErrorParser::getErrorCount()const{size_t c=0;for(auto&e:errors_)if(e.isError)c++;return c;}
size_t ShaderCompilerErrorParser::getWarningCount()const{return errors_.size()-getErrorCount();}
void ShaderCompilerErrorParser::clear(){errors_.clear();}
}'''),
('texture_leak_detector','''#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace nexus::utility::graphics {
class TextureLeakDetector {
public:
    struct TextureInfo { uint64_t handle; size_t width,height; size_t mipLevels; std::string format; bool freed; };
    static TextureLeakDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordAlloc(uint64_t handle, size_t w, size_t h, size_t mips, const std::string& format);
    void recordFree(uint64_t handle);
    std::vector<TextureInfo> getActiveTextures() const;
    std::vector<TextureInfo> getLeakedTextures() const;
    size_t getLeakCount() const;
    size_t getTotalBytesLeaked() const;
    void clear();
private:
    TextureLeakDetector()=default; ~TextureLeakDetector()=default; bool enabled_=false;
    std::unordered_map<uint64_t,TextureInfo> textures_;
};
}''','''#include "nexus/utility/graphics/texture_leak_detector.h"
namespace nexus::utility::graphics {
TextureLeakDetector& TextureLeakDetector::instance(){static TextureLeakDetector i;return i;}
void TextureLeakDetector::initialize(){enabled_=true;textures_.clear();}
void TextureLeakDetector::shutdown(){enabled_=false;}
bool TextureLeakDetector::isEnabled()const{return enabled_;}
void TextureLeakDetector::recordAlloc(uint64_t h,size_t w,size_t he,size_t m,const std::string& f){textures_[h]={h,w,he,m,f,false};}
void TextureLeakDetector::recordFree(uint64_t h){auto it=textures_.find(h);if(it!=textures_.end())it->second.freed=true;}
auto TextureLeakDetector::getActiveTextures()const->std::vector<TextureInfo>{std::vector<TextureInfo> r;for(auto&[_,t]:textures_)if(!t.freed)r.push_back(t);return r;}
auto TextureLeakDetector::getLeakedTextures()const->std::vector<TextureInfo>{return getActiveTextures();}
size_t TextureLeakDetector::getLeakCount()const{return getLeakedTextures().size();}
size_t TextureLeakDetector::getTotalBytesLeaked()const{size_t b=0;for(auto&t:getLeakedTextures())b+=t.width*t.height*4;return b;}
void TextureLeakDetector::clear(){textures_.clear();}
}'''),
('vulkan_validation_wrapper','''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class VulkanValidationWrapper {
public:
    struct VkMessage { std::string layer; int severity; std::string message; uint64_t objectHandle; };
    static VulkanValidationWrapper& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordMessage(const std::string& layer, int severity, const std::string& msg, uint64_t obj=0);
    std::vector<VkMessage> getMessages() const;
    std::vector<VkMessage> getErrors() const;
    std::vector<VkMessage> getWarnings() const;
    size_t getMessageCount() const;
    void clear();
private:
    VulkanValidationWrapper()=default; ~VulkanValidationWrapper()=default; bool enabled_=false;
    std::vector<VkMessage> messages_;
};
}''','''#include "nexus/utility/graphics/vulkan_validation_wrapper.h"
namespace nexus::utility::graphics {
VulkanValidationWrapper& VulkanValidationWrapper::instance(){static VulkanValidationWrapper i;return i;}
void VulkanValidationWrapper::initialize(){enabled_=true;messages_.clear();}
void VulkanValidationWrapper::shutdown(){enabled_=false;}
bool VulkanValidationWrapper::isEnabled()const{return enabled_;}
void VulkanValidationWrapper::recordMessage(const std::string& l,int s,const std::string& m,uint64_t o){messages_.push_back({l,s,m,o});}
auto VulkanValidationWrapper::getMessages()const->std::vector<VkMessage>{return messages_;}
auto VulkanValidationWrapper::getErrors()const->std::vector<VkMessage>{std::vector<VkMessage> r;for(auto&m: messages_)if(m.severity>=3)r.push_back(m);return r;}
auto VulkanValidationWrapper::getWarnings()const->std::vector<VkMessage>{std::vector<VkMessage> r;for(auto&m: messages_)if(m.severity<3)r.push_back(m);return r;}
size_t VulkanValidationWrapper::getMessageCount()const{return messages_.size();}
void VulkanValidationWrapper::clear(){messages_.clear();}
}'''),
]:
    gen('graphics', item[0], item[1], item[2])

print("Graphics (8) done!")
