// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Observer — mutation notification system
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentObserver.h>

#include <gtest/gtest.h>

using namespace nexus::agent;

namespace {

class CountingListener : public AgentMutationListener {
public:
    void onTopologyChanged(const AgentStateReport& report) override {
        (void)report;
        topologyCount++;
    }
    void onCommandExecuted(const AgentCommand&, const AgentResponse&) override {
        commandCount++;
    }
    int topologyCount = 0;
    int commandCount = 0;
};

} // namespace

TEST(AgentObserver, AddListener)
{
    AgentMutationNotifier notifier;
    CountingListener listener;
    notifier.addListener(&listener);
    EXPECT_EQ(notifier.listenerCount(), 1u);
    EXPECT_TRUE(notifier.hasListeners());
}

TEST(AgentObserver, RemoveListener)
{
    AgentMutationNotifier notifier;
    CountingListener listener;
    notifier.addListener(&listener);
    EXPECT_EQ(notifier.listenerCount(), 1u);
    notifier.removeListener(&listener);
    EXPECT_EQ(notifier.listenerCount(), 0u);
    EXPECT_FALSE(notifier.hasListeners());
}

TEST(AgentObserver, NotifyTopologyChanged)
{
    AgentMutationNotifier notifier;
    CountingListener listener;
    notifier.addListener(&listener);
    AgentStateReport report;
    report.valid = true;
    report.messages.push_back("test");
    notifier.notifyTopologyChanged(report);
    EXPECT_EQ(listener.topologyCount, 1);
}

TEST(AgentObserver, NotifyCommandExecuted)
{
    AgentMutationNotifier notifier;
    CountingListener listener;
    notifier.addListener(&listener);
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    AgentResponse resp;
    resp.success = true;
    notifier.notifyCommandExecuted(cmd, resp);
    EXPECT_EQ(listener.commandCount, 1);
}

TEST(AgentObserver, RemoveWhileIterating)
{
    AgentMutationNotifier notifier;
    CountingListener listener1;
    CountingListener listener2;
    notifier.addListener(&listener1);
    notifier.addListener(&listener2);
    EXPECT_EQ(notifier.listenerCount(), 2u);
    notifier.removeListener(&listener1);
    EXPECT_EQ(notifier.listenerCount(), 1u);
    AgentStateReport report;
    notifier.notifyTopologyChanged(report);
    EXPECT_EQ(listener2.topologyCount, 1);
}

TEST(AgentObserver, NoLeakOnDoubleRemove)
{
    AgentMutationNotifier notifier;
    CountingListener listener;
    notifier.addListener(&listener);
    notifier.removeListener(&listener);
    notifier.removeListener(&listener); // should not crash
    EXPECT_EQ(notifier.listenerCount(), 0u);
}

TEST(AgentObserver, NullListenerNotAdded)
{
    AgentMutationNotifier notifier;
    notifier.addListener(nullptr);
    EXPECT_EQ(notifier.listenerCount(), 0u);
}
