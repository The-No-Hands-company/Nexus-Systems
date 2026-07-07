#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::cloud {

class AwsSdkErrorTranslator {
public:
    static AwsSdkErrorTranslator& instance() {
        static AwsSdkErrorTranslator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); last_error_.clear(); }

    std::string translateError(const std::string& error_code) {
        std::lock_guard<std::mutex> lk(mutex_);
        last_error_ = error_code;
        static const std::map<std::string, std::string> errors = {
            {"AccessDenied", "Access denied - check IAM permissions"},
            {"NoSuchBucket", "The specified bucket does not exist"},
            {"BucketAlreadyExists", "The bucket name is already in use"},
            {"NoSuchKey", "The specified key does not exist"},
            {"InvalidAccessKeyId", "The AWS access key ID is invalid"},
            {"SignatureDoesNotMatch", "The request signature does not match"},
            {"RequestTimeTooSkewed", "The request timestamp is too far from server time"},
            {"InvalidInstanceID.NotFound", "The instance ID was not found"},
            {"InvalidParameterValue", "An invalid or out-of-range parameter value was supplied"},
            {"ResourceLimitExceeded", "The limit for the resource has been reached"},
            {"InsufficientInstanceCapacity", "AWS does not have enough available capacity"},
            {"InvalidParameterCombination", "Parameters that cannot be used together were specified"},
            {"KMS.AccessDeniedException", "KMS access denied - check key policy"},
            {"Lambda.ServiceException", "The Lambda service encountered an internal error"},
            {"Lambda.ResourceNotFoundException", "The Lambda function was not found"},
            {"TooManyRequestsException", "Rate limit exceeded, retry after delay"},
            {"ThrottlingException", "Request throttled, implement backoff"},
            {"UnrecognizedClientException", "The security token is invalid or expired"},
        };
        auto it = errors.find(error_code);
        if (it != errors.end()) return it->second;
        auto cit = custom_errors_.find(error_code);
        if (cit != custom_errors_.end()) return cit->second;
        return "Unknown AWS error: " + error_code;
    }

    void addCustomError(const std::string& code, const std::string& message) {
        std::lock_guard<std::mutex> lk(mutex_);
        custom_errors_[code] = message;
    }

    std::string getLastError() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return last_error_;
    }

private:
    AwsSdkErrorTranslator() = default;
    ~AwsSdkErrorTranslator() = default;
    AwsSdkErrorTranslator(const AwsSdkErrorTranslator&) = delete;
    AwsSdkErrorTranslator& operator=(const AwsSdkErrorTranslator&) = delete;
    AwsSdkErrorTranslator(AwsSdkErrorTranslator&&) = delete;
    AwsSdkErrorTranslator& operator=(AwsSdkErrorTranslator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::string last_error_;
    std::map<std::string, std::string> custom_errors_;
};

} // namespace nexus::utility::cloud
