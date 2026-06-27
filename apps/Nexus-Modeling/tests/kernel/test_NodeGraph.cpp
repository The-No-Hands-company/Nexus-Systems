#include <gtest/gtest.h>
#include "nexus/nodegraph/NodeGraphTypes.h"
#include "nexus/nodegraph/GraphNode.h"
#include "nexus/nodegraph/NodeGraph.h"
#include "nexus/nodegraph/NodeTypeRegistry.h"
#include "nexus/nodegraph/NodeGraphSerializer.h"

#include <algorithm>
#include <cmath>

using namespace nexus::nodegraph;

namespace {

NodeTypeInfo makeFloatConst() {
    NodeTypeInfo info;
    info.typeName = "FloatConst";
    info.outputs = {{"value", PortValueType::Float32}};
    return info;
}

NodeTypeInfo makeFloatOutput() {
    NodeTypeInfo info;
    info.typeName = "FloatOutput";
    info.inputs = {{"in", PortValueType::Float32}};
    return info;
}

NodeTypeInfo makeFloatOp() {
    NodeTypeInfo info;
    info.typeName = "FloatOp";
    info.inputs = {{"in", PortValueType::Float32}};
    info.outputs = {{"out", PortValueType::Float32}};
    return info;
}

NodeTypeInfo makeFloat64Op() {
    NodeTypeInfo info;
    info.typeName = "Float64Op";
    info.inputs = {{"in", PortValueType::Float64}};
    info.outputs = {{"out", PortValueType::Float64}};
    return info;
}

NodeTypeInfo makeIntOp() {
    NodeTypeInfo info;
    info.typeName = "IntOp";
    info.inputs = {{"in", PortValueType::Int32}};
    info.outputs = {{"out", PortValueType::Int32}};
    return info;
}

NodeTypeInfo makeNoneOp() {
    NodeTypeInfo info;
    info.typeName = "NoneOp";
    info.inputs = {{"in", PortValueType::None}};
    info.outputs = {{"out", PortValueType::None}};
    return info;
}

NodeTypeInfo makeBidirectional() {
    NodeTypeInfo info;
    info.typeName = "Bidirectional";
    info.inputs = {{"in", PortValueType::Float32}};
    info.outputs = {{"out", PortValueType::Float32}};
    return info;
}

NodeTypeInfo makeFloatMerge() {
    NodeTypeInfo info;
    info.typeName = "FloatMerge";
    info.inputs = {{"l", PortValueType::Float32}, {"r", PortValueType::Float32}};
    return info;
}

} // namespace

TEST(PortValue, DefaultIsNone) {
    PortValue pv;
    EXPECT_EQ(pv.type(), PortValueType::None);
}

TEST(PortValue, Float32Roundtrip) {
    auto pv = PortValue::fromFloat(3.14f);
    EXPECT_EQ(pv.type(), PortValueType::Float32);
    ASSERT_NE(pv.float32(), nullptr);
    EXPECT_FLOAT_EQ(*pv.float32(), 3.14f);
}

TEST(PortValue, DoubleRoundtrip) {
    auto pv = PortValue::fromDouble(2.718281828);
    EXPECT_EQ(pv.type(), PortValueType::Float64);
    ASSERT_NE(pv.float64(), nullptr);
    EXPECT_DOUBLE_EQ(*pv.float64(), 2.718281828);
}

TEST(PortValue, Int32Roundtrip) {
    auto pv = PortValue::fromInt32(-42);
    EXPECT_EQ(pv.type(), PortValueType::Int32);
    ASSERT_NE(pv.int32(), nullptr);
    EXPECT_EQ(*pv.int32(), -42);
}

TEST(PortValue, Int64Roundtrip) {
    auto pv = PortValue::fromInt64(1234567890123LL);
    EXPECT_EQ(pv.type(), PortValueType::Int64);
    ASSERT_NE(pv.int64(), nullptr);
    EXPECT_EQ(*pv.int64(), 1234567890123LL);
}

TEST(PortValue, BoolRoundtrip) {
    auto pv = PortValue::fromBool(true);
    EXPECT_EQ(pv.type(), PortValueType::Bool);
    ASSERT_NE(pv.boolean(), nullptr);
    EXPECT_TRUE(*pv.boolean());

    auto pv2 = PortValue::fromBool(false);
    ASSERT_NE(pv2.boolean(), nullptr);
    EXPECT_FALSE(*pv2.boolean());
}

TEST(PortValue, StringRoundtrip) {
    auto pv = PortValue::fromString("hello world");
    EXPECT_EQ(pv.type(), PortValueType::String);
    ASSERT_NE(pv.string(), nullptr);
    EXPECT_EQ(*pv.string(), "hello world");
}

TEST(PortValue, BinaryRoundtrip) {
    PortValue::Binary bin = {0x01, 0x02, 0x03, 0x04};
    auto pv = PortValue::fromBinary(bin);
    EXPECT_EQ(pv.type(), PortValueType::Binary);
    ASSERT_NE(pv.binary(), nullptr);
    EXPECT_EQ(pv.binary()->size(), 4u);
    EXPECT_EQ((*pv.binary())[0], 0x01);
}

TEST(GraphNode, DefaultConstruction) {
    GraphNode node;
    EXPECT_EQ(node.id(), kInvalidNodeId);
    EXPECT_TRUE(node.dirty());
    EXPECT_EQ(node.inputPortCount(), 0u);
    EXPECT_EQ(node.outputPortCount(), 0u);
}

TEST(GraphNode, ParameterizedConstruction) {
    GraphNode node(42u, "TestType", "MyNode");
    EXPECT_EQ(node.id(), 42u);
    EXPECT_EQ(node.typeName(), "TestType");
    EXPECT_EQ(node.name(), "MyNode");
    EXPECT_TRUE(node.dirty());
}

TEST(GraphNode, SetNameAndPosition) {
    GraphNode node(1u, "Type", "oldName");
    node.setName("newName");
    EXPECT_EQ(node.name(), "newName");

    node.setPosition(100.0f, 200.0f);
    EXPECT_FLOAT_EQ(node.positionX(), 100.0f);
    EXPECT_FLOAT_EQ(node.positionY(), 200.0f);
}

TEST(GraphNode, ConfigurePortsSetsInputsAndOutputs) {
    GraphNode node(1u, "Type", "Node");
    node.configurePorts(
        {{"in1", PortValueType::Float32}, {"in2", PortValueType::Int32}},
        {{"out1", PortValueType::Float64}}
    );

    EXPECT_EQ(node.inputPortCount(), 2u);
    EXPECT_EQ(node.outputPortCount(), 1u);

    EXPECT_EQ(node.inputs()[0].name, "in1");
    EXPECT_EQ(node.inputs()[0].valueType, PortValueType::Float32);
    EXPECT_EQ(node.inputs()[0].direction, PortDirection::Input);

    EXPECT_EQ(node.outputs()[0].name, "out1");
    EXPECT_EQ(node.outputs()[0].valueType, PortValueType::Float64);
    EXPECT_EQ(node.outputs()[0].direction, PortDirection::Output);
}

TEST(GraphNode, PortLookupByIndex) {
    GraphNode node(1u, "Type", "Node");
    node.configurePorts(
        {{"a", PortValueType::Float32}},
        {{"b", PortValueType::Float64}}
    );

    EXPECT_NE(node.inputPort(0), nullptr);
    EXPECT_EQ(node.inputPort(99), nullptr);
    EXPECT_NE(node.outputPort(0), nullptr);
    EXPECT_EQ(node.outputPort(99), nullptr);
}

TEST(GraphNode, PortLookupByName) {
    GraphNode node(1u, "Type", "Node");
    node.configurePorts(
        {{"position", PortValueType::Float32}, {"scale", PortValueType::Float32}},
        {{"result", PortValueType::Float32}}
    );

    EXPECT_NE(node.inputPortByName("position"), nullptr);
    EXPECT_NE(node.inputPortByName("scale"), nullptr);
    EXPECT_EQ(node.inputPortByName("nonexistent"), nullptr);
    EXPECT_NE(node.outputPortByName("result"), nullptr);
}

TEST(GraphNode, Attributes) {
    GraphNode node(1u, "Type", "Node");
    EXPECT_FALSE(node.hasAttribute("key"));

    node.setAttribute("key", PortValue::fromFloat(5.0f));
    EXPECT_TRUE(node.hasAttribute("key"));

    const auto* attr = node.attribute("key");
    ASSERT_NE(attr, nullptr);
    EXPECT_FLOAT_EQ(*attr->float32(), 5.0f);

    EXPECT_EQ(node.attribute("missing"), nullptr);
}

TEST(GraphNode, SetOutputValue) {
    GraphNode node(1u, "Type", "Node");
    node.configurePorts({}, {{"out", PortValueType::Float32}});

    node.setOutputValue(0, PortValue::fromFloat(42.0f));
    const auto* val = node.outputValue(0);
    ASSERT_NE(val, nullptr);
    EXPECT_FLOAT_EQ(*val->float32(), 42.0f);

    EXPECT_EQ(node.outputValue(99), nullptr);
}

TEST(GraphNode, DirtyFlag) {
    GraphNode node;
    EXPECT_TRUE(node.dirty());
    node.clearDirty();
    EXPECT_FALSE(node.dirty());
    node.markDirty();
    EXPECT_TRUE(node.dirty());
}

TEST(GraphNode, GroupNode) {
    GraphNode node(1u, "Type", "Node");
    EXPECT_FALSE(node.isGroupNode());
    node.setGroupNode(true);
    EXPECT_TRUE(node.isGroupNode());

    EXPECT_EQ(node.groupParentId(), kInvalidNodeId);
    node.setGroupParentId(42u);
    EXPECT_EQ(node.groupParentId(), 42u);
}

TEST(NodeGraph, EmptyGraphHasZeroNodes) {
    NodeGraph graph;
    EXPECT_EQ(graph.nodeCount(), 0u);
    EXPECT_TRUE(graph.connections().empty());
}

TEST(NodeGraph, AddNodeFromTypeReturnsValidId) {
    NodeGraph graph;
    NodeId id = graph.addNodeFromType("TestType", "TestNode");
    EXPECT_NE(id, kInvalidNodeId);
    EXPECT_EQ(graph.nodeCount(), 1u);
    EXPECT_TRUE(graph.hasNode(id));
}

TEST(NodeGraph, AddNodeProperties) {
    NodeGraph graph;
    NodeId id = graph.addNodeFromType("Math", "Add");
    const auto* n = graph.node(id);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->typeName(), "Math");
    EXPECT_EQ(n->name(), "Add");
}

TEST(NodeGraph, AddNodeWithTypeInfo) {
    NodeTypeInfo info;
    info.typeName = "FloatAdd";
    info.inputs = {{"a", PortValueType::Float32}, {"b", PortValueType::Float32}};
    info.outputs = {{"sum", PortValueType::Float32}};

    NodeGraph graph;
    NodeId id = graph.addNode(info, "myAdd");
    const auto* n = graph.node(id);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->inputPortCount(), 2u);
    EXPECT_EQ(n->outputPortCount(), 1u);
}

TEST(NodeGraph, RemoveNode) {
    NodeGraph graph;
    NodeId id = graph.addNodeFromType("Type", "Node");
    EXPECT_TRUE(graph.removeNode(id));
    EXPECT_EQ(graph.nodeCount(), 0u);
    EXPECT_FALSE(graph.hasNode(id));
}

TEST(NodeGraph, RemoveUnknownNodeReturnsFalse) {
    NodeGraph graph;
    EXPECT_FALSE(graph.removeNode(99u));
}

TEST(NodeGraph, RemoveNodeClearsConnections) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

    EXPECT_TRUE(graph.connect(a, 0, b, 0));
    EXPECT_EQ(graph.connections().size(), 1u);

    EXPECT_TRUE(graph.removeNode(a));
    EXPECT_EQ(graph.connections().size(), 0u);
}

TEST(NodeGraph, ConnectTwoNodes) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

    EXPECT_TRUE(graph.connect(a, 0, b, 0));
    EXPECT_TRUE(graph.isConnected(a, 0, b, 0));
    EXPECT_EQ(graph.connections().size(), 1u);
}

TEST(NodeGraph, ConnectMismatchedTypesFails) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeTypeInfo intInfo;
    intInfo.typeName = "IntOutput";
    intInfo.inputs = {{"input", PortValueType::Int32}};
    NodeId b = graph.addNode(intInfo, "B");

    EXPECT_FALSE(graph.connect(a, 0, b, 0));
    EXPECT_EQ(graph.connections().size(), 0u);
}

TEST(NodeGraph, ConnectSameNodeFails) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeBidirectional(), "A");
    EXPECT_FALSE(graph.connect(a, 0, a, 0));
}

TEST(NodeGraph, ConnectUnknownNodeReturnsFalse) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    EXPECT_FALSE(graph.connect(a, 0, 99u, 0));
    EXPECT_FALSE(graph.connect(99u, 0, a, 0));
}

TEST(NodeGraph, DuplicateConnectionFails) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

    EXPECT_TRUE(graph.connect(a, 0, b, 0));
    EXPECT_FALSE(graph.connect(a, 0, b, 0));
}

TEST(NodeGraph, DisconnectRemovesConnection) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

    EXPECT_TRUE(graph.connect(a, 0, b, 0));
    EXPECT_TRUE(graph.disconnect(a, 0, b, 0));
    EXPECT_FALSE(graph.isConnected(a, 0, b, 0));
    EXPECT_EQ(graph.connections().size(), 0u);
}

TEST(NodeGraph, DisconnectNonexistentReturnsFalse) {
    NodeGraph graph;
    EXPECT_FALSE(graph.disconnect(1u, 0, 2u, 0));
}

TEST(NodeGraph, ConnectionsForNode) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");
    NodeId c = graph.addNode(makeFloatOutput(), "C");

    (void)graph.connect(a, 0, b, 0);
    (void)graph.connect(a, 0, c, 0);

    auto conns = graph.connectionsForNode(a);
    EXPECT_EQ(conns.size(), 2u);

    auto connsB = graph.connectionsForNode(b);
    EXPECT_EQ(connsB.size(), 1u);
}

TEST(NodeGraph, TopologicalSortLinear) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOp(), "B");
    NodeId c = graph.addNode(makeFloatOutput(), "C");

(void)graph.connect(a, 0, b, 0);
(void)graph.connect(b, 0, c, 0);

    bool hasCycle = false;
    auto order = graph.topoSort(hasCycle);
    EXPECT_FALSE(hasCycle);
    EXPECT_EQ(order.size(), 3u);

    auto posA = static_cast<std::size_t>(std::find(order.begin(), order.end(), a) - order.begin());
    auto posB = static_cast<std::size_t>(std::find(order.begin(), order.end(), b) - order.begin());
    auto posC = static_cast<std::size_t>(std::find(order.begin(), order.end(), c) - order.begin());

    EXPECT_LT(posA, posB);
    EXPECT_LT(posB, posC);
}

TEST(NodeGraph, CycleDetection) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeBidirectional(), "A");
    NodeId b = graph.addNode(makeBidirectional(), "B");

(void)graph.connect(a, 0, b, 0);

    bool hasCycle = false;
    (void)graph.topoSort(hasCycle);
    EXPECT_FALSE(hasCycle);
    EXPECT_FALSE(graph.hasCycle());
}

TEST(NodeGraph, SimpleCycleIsDetected) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeBidirectional(), "A");
    NodeId b = graph.addNode(makeBidirectional(), "B");

(void)graph.connect(a, 0, b, 0);
(void)graph.connect(b, 0, a, 0);

    EXPECT_TRUE(graph.hasCycle());

    bool hasCycle = false;
    auto order = graph.topoSort(hasCycle);
    EXPECT_TRUE(hasCycle);
    EXPECT_NE(order.size(), graph.nodeCount());
}

TEST(NodeGraph, EvaluateWithCycleFails) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeBidirectional(), "A");
    NodeId b = graph.addNode(makeBidirectional(), "B");

(void)graph.connect(a, 0, b, 0);
(void)graph.connect(b, 0, a, 0);

    auto report = graph.evaluate();
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(report.hasCycle);
}

TEST(NodeGraph, EvaluateCallsComputeForDirtyNodes) {
    NodeGraph graph;
    NodeId constNode = graph.addNode(makeFloatConst(), "C");
    NodeId outNode = graph.addNode(makeFloatOutput(), "O");

(void)graph.connect(constNode, 0, outNode, 0);

    int callCount = 0;
    graph.setComputeCallback([&](GraphNode& node, const NodeGraph& g) {
        (void)g;
        callCount++;
        return true;
    });

    auto report = graph.evaluate();
    EXPECT_TRUE(report.success);
    EXPECT_EQ(callCount, 2);
}

TEST(NodeGraph, EvaluateSkipsCleanNodes) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOp(), "B");

(void)graph.connect(a, 0, b, 0);

    int callCount = 0;
    graph.setComputeCallback([&](GraphNode& node, const NodeGraph& g) {
        (void)g;
        callCount++;
        return true;
    });

    auto report1 = graph.evaluate();
    EXPECT_TRUE(report1.success);
    EXPECT_EQ(callCount, 2);

    callCount = 0;
    auto report2 = graph.evaluate();
    EXPECT_TRUE(report2.success);
    EXPECT_EQ(callCount, 0);
}

TEST(NodeGraph, MarkDirtyPropagatesDownstream) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOp(), "B");
    NodeId c = graph.addNode(makeFloatOp(), "C");

(void)graph.connect(a, 0, b, 0);
(void)graph.connect(b, 0, c, 0);

    int callCount = 0;
    graph.setComputeCallback([&](GraphNode& node, const NodeGraph& g) {
        (void)g;
        callCount++;
        return true;
    });

    (void)graph.evaluate();

    graph.markDirty(a);

    callCount = 0;
    (void)graph.evaluate();
    EXPECT_EQ(callCount, 3);
}

TEST(NodeGraph, ComputeCallbackFailure) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOp(), "B");

(void)graph.connect(a, 0, b, 0);

    bool secondCall = false;
    graph.setComputeCallback([&](GraphNode& node, const NodeGraph& g) {
        (void)g;
        if (node.id() == b) {
            secondCall = true;
            return false;
        }
        return true;
    });

    auto report = graph.evaluate();
    EXPECT_TRUE(secondCall);
    EXPECT_FALSE(report.success);
    EXPECT_EQ(report.failedNodeId, b);
}

TEST(NodeGraph, ClearRemovesAllNodes) {
    NodeGraph graph;
    (void)graph.addNodeFromType("Type1", "A");
    (void)graph.addNodeFromType("Type2", "B");
    graph.clear();
    EXPECT_EQ(graph.nodeCount(), 0u);
}

TEST(NodeGraph, MarkAllDirty) {
    NodeGraph graph;
    NodeId a = graph.addNodeFromType("Type", "A");
    NodeId b = graph.addNodeFromType("Type", "B");

    auto* nA = graph.node(a);
    auto* nB = graph.node(b);
    nA->clearDirty();
    nB->clearDirty();

    graph.markAllDirty();
    EXPECT_TRUE(nA->dirty());
    EXPECT_TRUE(nB->dirty());
}

TEST(NodeGraph, TransferValueThroughConnection) {
    NodeGraph graph;
    NodeId src = graph.addNode(makeFloatConst(), "Src");
    NodeId dst = graph.addNode(makeFloatOutput(), "Dst");

(void)graph.connect(src, 0, dst, 0);

    auto* srcNode = graph.node(src);
    srcNode->setOutputValue(0, PortValue::fromFloat(7.5f));

    int called = 0;
    graph.setComputeCallback([&](GraphNode& node, const NodeGraph& g) {
        (void)g;
        called++;
        return true;
    });

    (void)graph.evaluate();

    auto* dstNode = graph.node(dst);
    ASSERT_NE(dstNode, nullptr);
    const auto* inVal = dstNode->inputPort(0);
    ASSERT_NE(inVal, nullptr);
    EXPECT_EQ(inVal->value.type(), PortValueType::Float32);
    EXPECT_FLOAT_EQ(*inVal->value.float32(), 7.5f);
}

TEST(NodeGraph, PortConnectedFlag) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeFloatConst(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

(void)graph.connect(a, 0, b, 0);

    const auto* nA = graph.node(a);
    const auto* nB = graph.node(b);
    EXPECT_TRUE(nA->outputPort(0)->connected);
    EXPECT_TRUE(nB->inputPort(0)->connected);

(void)graph.disconnect(a, 0, b, 0);
    EXPECT_FALSE(nA->outputPort(0)->connected);
    EXPECT_FALSE(nB->inputPort(0)->connected);
}

TEST(NodeTypeRegistry, SingletonIsStable) {
    auto& r1 = NodeTypeRegistry::instance();
    auto& r2 = NodeTypeRegistry::instance();
    EXPECT_EQ(&r1, &r2);
}

TEST(NodeTypeRegistry, RegisterAndQueryType) {
    auto& reg = NodeTypeRegistry::instance();

    NodeTypeInfo info;
    info.typeName = "TestNode";
    info.displayName = "Test Node";
    info.category = "Testing";
    info.inputs = {{"in", PortValueType::Float32}};
    info.outputs = {{"out", PortValueType::Float32}};

    reg.registerType(info);
    EXPECT_TRUE(reg.hasType("TestNode"));

    const auto* retrieved = reg.typeInfo("TestNode");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->typeName, "TestNode");
    EXPECT_EQ(retrieved->displayName, "Test Node");
    EXPECT_EQ(retrieved->category, "Testing");
    EXPECT_EQ(retrieved->inputs.size(), 1u);
    EXPECT_EQ(retrieved->outputs.size(), 1u);
}

TEST(NodeTypeRegistry, UnknownTypeReturnsNull) {
    auto& reg = NodeTypeRegistry::instance();
    EXPECT_FALSE(reg.hasType("NonExistentType"));
    EXPECT_EQ(reg.typeInfo("NonExistentType"), nullptr);
}

TEST(NodeTypeRegistry, CreateNodeFromRegistry) {
    auto& reg = NodeTypeRegistry::instance();

    NodeTypeInfo info;
    info.typeName = "CreateTest";
    info.inputs = {{"x", PortValueType::Float32}};
    info.outputs = {{"y", PortValueType::Float32}};

    reg.registerType(info);

    auto node = reg.createNode(100u, "CreateTest", "MyCreateNode");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->id(), 100u);
    EXPECT_EQ(node->typeName(), "CreateTest");
    EXPECT_EQ(node->name(), "MyCreateNode");
    EXPECT_EQ(node->inputPortCount(), 1u);
    EXPECT_EQ(node->outputPortCount(), 1u);
}

TEST(NodeTypeRegistry, TypeNames) {
    auto& reg = NodeTypeRegistry::instance();

    NodeTypeInfo info1;
    info1.typeName = "RegistryNodeA";
    info1.category = "CatA";
    reg.registerType(info1);

    NodeTypeInfo info2;
    info2.typeName = "RegistryNodeB";
    info2.category = "CatB";
    reg.registerType(info2);

    NodeTypeInfo info3;
    info3.typeName = "RegistryNodeC";
    info3.category = "CatA";
    reg.registerType(info3);

    auto all = reg.typeNames();
    EXPECT_GE(all.size(), 3u);

    auto catA = reg.typeNamesInCategory("CatA");
    EXPECT_GE(catA.size(), 2u);
}

TEST(NodeTypeRegistry, ClearRemovesAll) {
    auto& reg = NodeTypeRegistry::instance();

    NodeTypeInfo info;
    info.typeName = "ClearTest";
    reg.registerType(info);
    EXPECT_TRUE(reg.hasType("ClearTest"));

    reg.clear();
    EXPECT_FALSE(reg.hasType("ClearTest"));
}

TEST(NodeGraphSerializer, PortValueToStringFloat) {
    auto pv = PortValue::fromFloat(3.14f);
    EXPECT_EQ(NodeGraphSerializer::portValueToString(pv), "3.14");
}

TEST(NodeGraphSerializer, PortValueToStringInt) {
    auto pv = PortValue::fromInt32(42);
    EXPECT_EQ(NodeGraphSerializer::portValueToString(pv), "42");
}

TEST(NodeGraphSerializer, PortValueToStringBool) {
    EXPECT_EQ(NodeGraphSerializer::portValueToString(PortValue::fromBool(true)), "true");
    EXPECT_EQ(NodeGraphSerializer::portValueToString(PortValue::fromBool(false)), "false");
}

TEST(NodeGraphSerializer, PortValueToStringString) {
    auto pv = PortValue::fromString("test123");
    EXPECT_EQ(NodeGraphSerializer::portValueToString(pv), "\"test123\"");
}

TEST(NodeGraphSerializer, PortValueToStringNone) {
    PortValue pv;
    EXPECT_EQ(NodeGraphSerializer::portValueToString(pv), "null");
}

TEST(NodeGraphSerializer, SerializeNode) {
    GraphNode node(5u, "FloatAdd", "Add");
    node.configurePorts(
        {{"a", PortValueType::Float32}, {"b", PortValueType::Float32}},
        {{"sum", PortValueType::Float32}}
    );
    node.setPosition(100.f, 200.f);

    std::string json = NodeGraphSerializer::serializeNode(node);
    EXPECT_TRUE(json.find("\"id\":5") != std::string::npos);
    EXPECT_TRUE(json.find("\"type\":\"FloatAdd\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"name\":\"Add\"") != std::string::npos);
}

TEST(NodeGraphSerializer, SerializeConnection) {
    Connection conn;
    conn.sourceNodeId = 1u;
    conn.sourcePortIndex = 0;
    conn.destinationNodeId = 2u;
    conn.destinationPortIndex = 1;

    std::string json = NodeGraphSerializer::serializeConnection(conn);
    EXPECT_TRUE(json.find("\"src\":1") != std::string::npos);
    EXPECT_TRUE(json.find("\"dst\":2") != std::string::npos);
}

TEST(NodeGraphSerializer, SerializeEmptyGraph) {
    NodeGraph graph;
    std::string json = NodeGraphSerializer::serialize(graph);
    EXPECT_TRUE(json.find("\"nodes\": [") != std::string::npos);
    EXPECT_TRUE(json.find("\"connections\": [") != std::string::npos);
}

TEST(NodeGraphSerializer, SerializeGraphWithNodes) {
    NodeGraph graph;
    (void)graph.addNode(makeFloatConst(), "C1");
    (void)graph.addNode(makeFloatOutput(), "O1");

    std::string json = NodeGraphSerializer::serialize(graph);
    EXPECT_TRUE(json.find("\"type\":\"FloatConst\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"type\":\"FloatOutput\"") != std::string::npos);
}

TEST(NodeGraphSerializer, DeserializeClearsExistingGraph) {
    NodeGraph graph;
    (void)graph.addNodeFromType("Type", "N");
    EXPECT_EQ(graph.nodeCount(), 1u);

    bool result = NodeGraphSerializer::deserialize(graph, "{}");
    (void)result;
    EXPECT_EQ(graph.nodeCount(), 0u);
}

TEST(NodeGraph, PortTypesMatchSupertypeFloat32toFloat64) {
    NodeGraph graph;
    NodeTypeInfo float32Const;
    float32Const.typeName = "Float32Const";
    float32Const.outputs = {{"value", PortValueType::Float32}};

    NodeTypeInfo float64Input;
    float64Input.typeName = "Float64Input";
    float64Input.inputs = {{"in", PortValueType::Float64}};

    NodeId a = graph.addNode(float32Const, "A");
    NodeId b = graph.addNode(float64Input, "B");

    EXPECT_TRUE(graph.connect(a, 0, b, 0));
}

TEST(NodeGraph, PortTypesMatchInt32toFloat32) {
    NodeGraph graph;
    NodeTypeInfo intConst;
    intConst.typeName = "IntConst";
    intConst.outputs = {{"value", PortValueType::Int32}};

    NodeId a = graph.addNode(intConst, "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

    EXPECT_TRUE(graph.connect(a, 0, b, 0));
}

TEST(NodeGraph, PortTypesMatchNoneFails) {
    NodeGraph graph;
    NodeId a = graph.addNode(makeNoneOp(), "A");
    NodeId b = graph.addNode(makeFloatOutput(), "B");

    EXPECT_FALSE(graph.connect(a, 0, b, 0));
}

TEST(NodeGraph, DiamondDependencyHandledCorrectly) {
    NodeGraph graph;
    NodeId input = graph.addNode(makeFloatConst(), "Input");
    NodeId left = graph.addNode(makeFloatOp(), "Left");
    NodeId right = graph.addNode(makeFloatOp(), "Right");
    NodeId merge = graph.addNode(makeFloatMerge(), "Merge");

    (void)graph.connect(input, 0, left, 0);
    (void)graph.connect(input, 0, right, 0);
    (void)graph.connect(left, 0, merge, 0);
    (void)graph.connect(right, 0, merge, 1);

    bool hasCycle = false;
    auto order = graph.topoSort(hasCycle);
    EXPECT_FALSE(hasCycle);
    EXPECT_EQ(order.size(), 4u);

    auto posInput = static_cast<std::size_t>(std::find(order.begin(), order.end(), input) - order.begin());
    auto posLeft = static_cast<std::size_t>(std::find(order.begin(), order.end(), left) - order.begin());
    auto posRight = static_cast<std::size_t>(std::find(order.begin(), order.end(), right) - order.begin());
    auto posMerge = static_cast<std::size_t>(std::find(order.begin(), order.end(), merge) - order.begin());

    EXPECT_LT(posInput, posLeft);
    EXPECT_LT(posInput, posRight);
    EXPECT_LT(posLeft, posMerge);
    EXPECT_LT(posRight, posMerge);
}

TEST(NodeGraph, MultipleOutputConnections) {
    NodeGraph graph;
    NodeId src = graph.addNode(makeFloatConst(), "Src");
    NodeId dst1 = graph.addNode(makeFloatOutput(), "Dst1");
    NodeId dst2 = graph.addNode(makeFloatOutput(), "Dst2");

    EXPECT_TRUE(graph.connect(src, 0, dst1, 0));
    EXPECT_TRUE(graph.connect(src, 0, dst2, 0));
    EXPECT_EQ(graph.connections().size(), 2u);
}
