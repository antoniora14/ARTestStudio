#include "Domain/DiagramModel.h"
#include "Domain/DiagramGeometry.h"
#include "Domain/OrthogonalRouter.h"

#include <algorithm>
#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
	using namespace arteststudio::domain;

	struct TestCase
	{
		std::string_view name;
		std::function<void()> execute;
	};

	class TestFailure final : public std::exception
	{
	public:
		explicit TestFailure(const char* message) noexcept
			: m_message(message)
		{
		}

		[[nodiscard]] const char* what() const noexcept override
		{
			return m_message;
		}

	private:
		const char* m_message;
	};

	void Require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw TestFailure(message);
		}
	}

	std::vector<Point> GetFullRoute(const DiagramModel& diagram, const Connection& connection)
	{
		const Node* source = diagram.FindNode(connection.from.nodeId);
		const Node* target = diagram.FindNode(connection.to.nodeId);
		Require(source != nullptr && target != nullptr, "Route endpoints must exist.");

		std::vector<Point> points{GetConnectionPoint(*source, connection.from.portId)};
		points.insert(points.end(), connection.intermediatePoints.begin(), connection.intermediatePoints.end());
		points.push_back(GetConnectionPoint(*target, connection.to.portId));
		return points;
	}

	void RequireOrthogonal(const std::vector<Point>& points)
	{
		for (std::size_t index = 0; index + 1 < points.size(); ++index)
		{
			Require(points[index].x == points[index + 1].x || points[index].y == points[index + 1].y,
				"Every route segment must be horizontal or vertical.");
		}

		for (std::size_t index = 1; index + 1 < points.size(); ++index)
		{
			const bool redundantVertical = points[index - 1].x == points[index].x &&
				points[index].x == points[index + 1].x;
			const bool redundantHorizontal = points[index - 1].y == points[index].y &&
				points[index].y == points[index + 1].y;
			Require(!redundantVertical && !redundantHorizontal,
				"Collinear route points must be simplified.");
		}
	}

	bool SegmentCrossesInterior(Point first, Point second, Rect rectangle)
	{
		if (first.x == second.x)
		{
			const int minimum = std::min(first.y, second.y);
			const int maximum = std::max(first.y, second.y);
			return first.x > rectangle.left && first.x < rectangle.right &&
				maximum > rectangle.top && minimum < rectangle.bottom;
		}

		if (first.y == second.y)
		{
			const int minimum = std::min(first.x, second.x);
			const int maximum = std::max(first.x, second.x);
			return first.y > rectangle.top && first.y < rectangle.bottom &&
				maximum > rectangle.left && minimum < rectangle.right;
		}
		return true;
	}

	void RequireAvoids(const std::vector<Point>& route, Rect obstacle)
	{
		for (std::size_t index = 0; index + 1 < route.size(); ++index)
		{
			Require(!SegmentCrossesInterior(route[index], route[index + 1], obstacle),
				"A route segment must not cross an obstacle.");
		}
	}

	void RequirePortDirection(Point port, Point adjacent, PortId portId)
	{
		switch (portId)
		{
		case PortId::Top:
			Require(adjacent.x == port.x && adjacent.y <= port.y, "Top port route must leave upward.");
			break;
		case PortId::Right:
			Require(adjacent.y == port.y && adjacent.x >= port.x, "Right port route must leave to the right.");
			break;
		case PortId::Bottom:
			Require(adjacent.x == port.x && adjacent.y >= port.y, "Bottom port route must leave downward.");
			break;
		case PortId::Left:
			Require(adjacent.y == port.y && adjacent.x <= port.x, "Left port route must leave to the left.");
			break;
		}
	}

	void AddsAndMovesNodes()
	{
		DiagramModel diagram;
		const NodeId first = diagram.AddNode(NodeKind::Rectangle, {10, 20}, L"PowerON");
		const NodeId second = diagram.AddNode(NodeKind::Diamond, {100, 120}, L"IF");

		Require(first != second, "Node IDs must be unique.");
		Require(diagram.Nodes().size() == 2, "Both nodes must be stored.");
		Require(diagram.MoveNodeBy(first, 5, -2) == DiagramError::None, "Existing node must move.");

		const Node* moved = diagram.FindNode(first);
		Require(moved != nullptr, "Moved node must still exist.");
		Require(moved->position == Point{15, 18}, "MoveNodeBy must update the node position.");
		Require(diagram.MoveNodeTo(second, {7, 9}) == DiagramError::None, "Existing node must move to a position.");
		Require(diagram.FindNode(second)->position == Point{7, 9}, "MoveNodeTo must replace the position.");
	}

	void CreatesValidConnections()
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Source");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {200, 0}, L"Target");

		const AddConnectionResult result = diagram.AddConnection(
			{source, PortId::Right},
			{target, PortId::Left},
			{{100, 50}});

		Require(static_cast<bool>(result), "A connection between existing nodes must succeed.");
		Require(diagram.Connections().size() == 1, "The connection must be stored.");
		Require(diagram.Connections().front().intermediatePoints == std::vector<Point>{{100, 50}},
			"Intermediate points must be preserved.");
	}

	void RejectsInvalidConnections()
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Existing");

		const AddConnectionResult missingNode = diagram.AddConnection(
			{existing, PortId::Right},
			{NodeId{999}, PortId::Left});
		Require(missingNode.error == DiagramError::NodeNotFound, "Unknown nodes must be rejected.");

		const AddConnectionResult invalidPort = diagram.AddConnection(
			{existing, static_cast<PortId>(99)},
			{existing, PortId::Left});
		Require(invalidPort.error == DiagramError::InvalidPort, "Unknown ports must be rejected.");
		Require(diagram.Connections().empty(), "Rejected connections must not modify the model.");
	}

	void RemovingNodeRemovesItsConnections()
	{
		DiagramModel diagram;
		const NodeId first = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"First");
		const NodeId second = diagram.AddNode(NodeKind::Rectangle, {200, 0}, L"Second");
		const NodeId third = diagram.AddNode(NodeKind::Rectangle, {400, 0}, L"Third");

		Require(static_cast<bool>(diagram.AddConnection({first, PortId::Right}, {second, PortId::Left})),
			"First connection must be created.");
		Require(static_cast<bool>(diagram.AddConnection({second, PortId::Right}, {third, PortId::Left})),
			"Second connection must be created.");

		Require(diagram.RemoveNode(second) == DiagramError::None, "Existing node must be removed.");
		Require(diagram.FindNode(second) == nullptr, "Removed node must not be found.");
		Require(diagram.Nodes().size() == 2, "Unrelated nodes must remain.");
		Require(diagram.Connections().empty(), "Connections referencing the removed node must be removed.");
	}

	void RoutesOrthogonallyAroundBlocks()
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 100}, L"Source");
		const NodeId obstacleId = diagram.AddNode(NodeKind::Rectangle, {250, 50}, 150, 200, L"Obstacle");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {500, 100}, L"Target");
		const AddConnectionResult added = diagram.AddConnection(
			{source, PortId::Right}, {target, PortId::Left});

		Require(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
			"Existing connection must be routed.");

		const Connection* connection = diagram.FindConnection(added.connectionId);
		Require(connection != nullptr, "Routed connection must still exist.");
		const std::vector<Point> route = GetFullRoute(diagram, *connection);
		RequireOrthogonal(route);
		const Node* obstacle = diagram.FindNode(obstacleId);
		Require(obstacle != nullptr, "Obstacle must exist.");
		RequireAvoids(route, Inflate(GetNodeBounds(*obstacle), 12));
	}

	void ReroutesWhenAnObstacleMoves()
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 100}, L"Source");
		const NodeId obstacleId = diagram.AddNode(NodeKind::Rectangle, {250, 350}, 150, 200, L"Obstacle");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {500, 100}, L"Target");
		const AddConnectionResult added = diagram.AddConnection(
			{source, PortId::Right}, {target, PortId::Left});
		Require(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
			"Initial connection must be routed.");

		const Connection* initialConnection = diagram.FindConnection(added.connectionId);
		Require(initialConnection != nullptr, "Initial connection must exist.");
		const std::vector<Point> initialRoute = GetFullRoute(diagram, *initialConnection);
		Require(initialConnection->intermediatePoints.empty(),
			"Aligned nodes without obstacles should use a direct route.");

		Require(diagram.MoveNodeTo(obstacleId, {250, 50}) == DiagramError::None,
			"Obstacle must move into the direct route.");
		OrthogonalRouter::RouteAll(diagram);

		const Connection* reroutedConnection = diagram.FindConnection(added.connectionId);
		Require(reroutedConnection != nullptr, "Rerouted connection must exist.");
		const std::vector<Point> rerouted = GetFullRoute(diagram, *reroutedConnection);
		Require(rerouted != initialRoute, "The route must change after an obstacle moves into it.");
		RequireOrthogonal(rerouted);
		RequireAvoids(rerouted, Inflate(GetNodeBounds(*diagram.FindNode(obstacleId)), 12));
	}

	void RoutesEveryPortCombination()
	{
		constexpr std::array<PortId, 4> ports{
			PortId::Top, PortId::Right, PortId::Bottom, PortId::Left};

		for (const PortId sourcePort : ports)
		{
			for (const PortId targetPort : ports)
			{
				DiagramModel diagram;
				const NodeId sourceId = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Source");
				const NodeId targetId = diagram.AddNode(NodeKind::Rectangle, {400, 300}, L"Target");
				const AddConnectionResult added = diagram.AddConnection(
					{sourceId, sourcePort}, {targetId, targetPort});
				Require(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
					"Every valid port combination must be routable.");

				const Connection* connection = diagram.FindConnection(added.connectionId);
				Require(connection != nullptr, "Port combination connection must exist.");
				const std::vector<Point> route = GetFullRoute(diagram, *connection);
				Require(route.size() >= 2, "A routed connection must contain both endpoints.");
				RequireOrthogonal(route);
				RequirePortDirection(route.front(), route[1], sourcePort);
				RequirePortDirection(route.back(), route[route.size() - 2], targetPort);
			}
		}
	}
}

int main()
{
	const std::vector<TestCase> tests{
		{"adds and moves nodes", AddsAndMovesNodes},
		{"creates valid connections", CreatesValidConnections},
		{"rejects invalid connections", RejectsInvalidConnections},
		{"removing node removes its connections", RemovingNodeRemovesItsConnections},
		{"routes orthogonally around blocks", RoutesOrthogonallyAroundBlocks},
		{"reroutes when an obstacle moves", ReroutesWhenAnObstacleMoves},
		{"routes every port combination", RoutesEveryPortCombination},
	};

	int failures = 0;
	for (const auto& test : tests)
	{
		try
		{
			test.execute();
			std::cout << "[PASS] " << test.name << '\n';
		}
		catch (const std::exception& error)
		{
			++failures;
			std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
		}
		catch (...)
		{
			++failures;
			std::cerr << "[FAIL] " << test.name << ": unknown error\n";
		}
	}

	std::cout << tests.size() - static_cast<std::size_t>(failures)
		<< '/' << tests.size() << " tests passed\n";
	return failures == 0 ? 0 : 1;
}
