#include "../TestSupport/TestSupport.h"

#include "Domain/DiagramModel.h"

#include <gtest/gtest.h>

#include <vector>

namespace arteststudio::tests
{
	using namespace domain;

	TEST(DiagramModelTests, AddsAndMovesNodes)
	{
		DiagramModel diagram;
		const NodeId first = diagram.AddNode(NodeKind::Rectangle, {10, 20}, L"PowerON");
		const NodeId second = diagram.AddNode(NodeKind::Diamond, {100, 120}, L"IF");

		VerifyTestCondition(first != second, "Node IDs must be unique.");
		VerifyTestCondition(diagram.Nodes().size() == 2, "Both nodes must be stored.");
		VerifyTestCondition(diagram.MoveNodeBy(first, 5, -2) == DiagramError::None, "Existing node must move.");

		const Node* moved = diagram.FindNode(first);
		VerifyTestCondition(moved != nullptr, "Moved node must still exist.");
		VerifyTestCondition(moved->position == Point{15, 18}, "MoveNodeBy must update the node position.");
		VerifyTestCondition(diagram.MoveNodeTo(second, {7, 9}) == DiagramError::None, "Existing node must move to a position.");
		VerifyTestCondition(diagram.FindNode(second)->position == Point{7, 9}, "MoveNodeTo must replace the position.");
	}

	TEST(DiagramModelTests, CreatesValidConnections)
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Source");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {200, 0}, L"Target");

		const AddConnectionResult result = diagram.AddConnection(
			{source, PortId::Right},
			{target, PortId::Left},
			{{100, 50}});

		VerifyTestCondition(static_cast<bool>(result), "A connection between existing nodes must succeed.");
		VerifyTestCondition(diagram.Connections().size() == 1, "The connection must be stored.");
		VerifyTestCondition(diagram.Connections().front().intermediatePoints == std::vector<Point>{{100, 50}},
			"Intermediate points must be preserved.");
	}

	TEST(DiagramModelTests, RejectsInvalidConnections)
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Existing");

		const AddConnectionResult missingNode = diagram.AddConnection(
			{existing, PortId::Right},
			{NodeId{999}, PortId::Left});
		VerifyTestCondition(missingNode.error == DiagramError::NodeNotFound, "Unknown nodes must be rejected.");

		const AddConnectionResult invalidPort = diagram.AddConnection(
			{existing, static_cast<PortId>(99)},
			{existing, PortId::Left});
		VerifyTestCondition(invalidPort.error == DiagramError::InvalidPort, "Unknown ports must be rejected.");
		VerifyTestCondition(diagram.Connections().empty(), "Rejected connections must not modify the model.");
	}

	TEST(DiagramModelTests, RemovingNodeRemovesItsConnections)
	{
		DiagramModel diagram;
		const NodeId first = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"First");
		const NodeId second = diagram.AddNode(NodeKind::Rectangle, {200, 0}, L"Second");
		const NodeId third = diagram.AddNode(NodeKind::Rectangle, {400, 0}, L"Third");

		VerifyTestCondition(static_cast<bool>(diagram.AddConnection({first, PortId::Right}, {second, PortId::Left})),
			"First connection must be created.");
		VerifyTestCondition(static_cast<bool>(diagram.AddConnection({second, PortId::Right}, {third, PortId::Left})),
			"Second connection must be created.");

		VerifyTestCondition(diagram.RemoveNode(second) == DiagramError::None, "Existing node must be removed.");
		VerifyTestCondition(diagram.FindNode(second) == nullptr, "Removed node must not be found.");
		VerifyTestCondition(diagram.Nodes().size() == 2, "Unrelated nodes must remain.");
		VerifyTestCondition(diagram.Connections().empty(), "Connections referencing the removed node must be removed.");
	}

	TEST(DiagramModelTests, RejectsDuplicateNodeIdentifiersAtomically)
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {5, 5}, L"Existing");
		DiagramSnapshot snapshot;
		snapshot.nodes = {
			{NodeId{7}, NodeKind::Rectangle, {0, 0}, 150, 100, L"First"},
			{NodeId{7}, NodeKind::Diamond, {200, 0}, 100, 80, L"Duplicate"}};

		const RestoreSnapshotResult result = diagram.RestoreSnapshot(std::move(snapshot));
		VerifyTestCondition(result.error == DiagramSnapshotError::DuplicateNodeId,
			"Duplicate node identifiers must be rejected explicitly.");
		VerifyTestCondition(result.identifier == 7, "The duplicate identifier must be reported.");
		VerifyTestCondition(diagram.Nodes().size() == 1 && diagram.FindNode(existing) != nullptr,
			"A rejected snapshot must not replace the active diagram.");
	}

	TEST(DiagramModelTests, RejectsDuplicateConnectionIdentifiersAtomically)
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {5, 5}, L"Existing");
		DiagramSnapshot snapshot;
		snapshot.nodes = {
			{NodeId{10}, NodeKind::Rectangle, {0, 0}, 150, 100, L"First"},
			{NodeId{20}, NodeKind::Rectangle, {300, 0}, 150, 100, L"Second"}};
		snapshot.connections = {
			{ConnectionId{30}, {NodeId{10}, PortId::Right}, {NodeId{20}, PortId::Left}, {}},
			{ConnectionId{30}, {NodeId{20}, PortId::Left}, {NodeId{10}, PortId::Right}, {}}};

		const RestoreSnapshotResult result = diagram.RestoreSnapshot(std::move(snapshot));
		VerifyTestCondition(result.error == DiagramSnapshotError::DuplicateConnectionId,
			"Duplicate connection identifiers must be rejected explicitly.");
		VerifyTestCondition(result.identifier == 30, "The duplicate connection identifier must be reported.");
		VerifyTestCondition(diagram.Nodes().size() == 1 && diagram.FindNode(existing) != nullptr,
			"A rejected connection snapshot must preserve the active diagram.");
	}

	TEST(DiagramModelTests, RejectsDanglingSnapshotConnectionsAtomically)
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {5, 5}, L"Existing");
		DiagramSnapshot snapshot;
		snapshot.nodes = {
			{NodeId{10}, NodeKind::Rectangle, {0, 0}, 150, 100, L"Only node"}};
		snapshot.connections = {
			{ConnectionId{20}, {NodeId{10}, PortId::Right}, {NodeId{99}, PortId::Left}, {}}};

		const RestoreSnapshotResult result = diagram.RestoreSnapshot(std::move(snapshot));
		VerifyTestCondition(result.error == DiagramSnapshotError::NodeNotFound,
			"Connections to nodes outside the snapshot must be rejected.");
		VerifyTestCondition(result.identifier == 99, "The missing endpoint identifier must be reported.");
		VerifyTestCondition(diagram.Nodes().size() == 1 && diagram.FindNode(existing) != nullptr,
			"A dangling snapshot must not partially replace the active diagram.");
	}


}
