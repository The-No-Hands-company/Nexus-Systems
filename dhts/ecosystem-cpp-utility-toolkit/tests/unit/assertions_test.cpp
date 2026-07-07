#include <gtest/gtest.h>

#include "nexus/utility/assertions/assert_enhanced.h"
#include "nexus/utility/assertions/precondition.h"
#include "nexus/utility/assertions/contract_violation_handler.h"

using namespace nexus::utility::assertions;

TEST(AssertionsTest, EnhancedAssertCustomHandler) {
    bool handler_called = false;

    EnhancedAssert::setHandler([&](const char*, const char*, const std::source_location&,
                                    const nexus::utility::stacktrace&) {
        handler_called = true;
    });

    NEXUS_VERIFY(false, "test message");
    EXPECT_TRUE(handler_called);

    EnhancedAssert::resetHandler();
}

TEST(AssertionsTest, ContractViolationHandlerPolicy) {
    ContractViolationHandler::setPolicy(ViolationPolicy::LogAndContinue);
    EXPECT_EQ(ContractViolationHandler::getPolicy(), ViolationPolicy::LogAndContinue);

    ContractViolationHandler::setPolicy(ViolationPolicy::Abort);
    EXPECT_EQ(ContractViolationHandler::getPolicy(), ViolationPolicy::Abort);

    // Don't test Throw policy as it would actually throw
}

TEST(AssertionsTest, ContractViolationHandlerCallback) {
    bool callback_called = false;

    ContractViolationHandler::setCallback([&](const char* type, const char*, const char*,
                                               const std::source_location&, const nexus::utility::stacktrace&) {
        callback_called = true;
        EXPECT_STREQ(type, "CUSTOM");
    });

    ContractViolationHandler::handleViolation("CUSTOM", "x", "msg");
    EXPECT_TRUE(callback_called);

    // Reset to avoid affecting other tests
    ContractViolationHandler::setCallback(nullptr);
    ContractViolationHandler::setPolicy(ViolationPolicy::LogAndContinue);
}

TEST(AssertionsTest, PreconditionCheck) {
    // With LogAndContinue, precondition should not abort
    ContractViolationHandler::setPolicy(ViolationPolicy::LogAndContinue);
    EXPECT_NO_THROW(Precondition::check(false, "test precondition"));

    ContractViolationHandler::setPolicy(ViolationPolicy::Abort);
}
