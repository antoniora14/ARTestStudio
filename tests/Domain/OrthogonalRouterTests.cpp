#include "../TestSupport/TestSupport.h"

#include "Domain/DiagramGeometry.h"
#include "Domain/DiagramModel.h"
#include "Domain/OrthogonalRouter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace arteststudio::tests
{
	using namespace domain;

	std::vector<Point> GetFullRoute(const DiagramModel& diagram, const Connection& connection)
	{
		const Node* source = diagram.FindNode(connection.from.nodeId);
		const Node* target = diagram.FindNode(connection.to.nodeId);
		if (source == nullptr || target == nullptr)
		{
			ADD_FAILURE() << "Route endpoints must exist.";
			return {};
		}

		std::vector<Point> points{GetConnectionPoint(*source, connection.from.portId)};
		points.insert(points.end(), connection.intermediatePoints.begin(), connection.intermediatePoints.end());
		points.push_back(GetConnectionPoint(*target, connection.to.portId));
		return points;
	}

	void RequireOrthogonal(const std::vector<Point>& points)
	{
		for (std::size_t index = 0; index + 1 < points.size(); ++index)
		{
			VerifyTestCondition(points[index].x == points[index + 1].x || points[index].y == points[index + 1].y,
				"Every route segment must be horizontal or vertical.");
		}

		for (std::size_t index = 1; index + 1 < points.size(); ++index)
		{
			const bool redundantVertical = points[index - 1].x == points[index].x &&
				points[index].x == points[index + 1].x;
			const bool redundantHorizontal = points[index - 1].y == points[index].y &&
				points[index].y == points[index + 1].y;
			VerifyTestCondition(!redundantVertical && !redundantHorizontal,
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
			VerifyTestCondition(!SegmentCrossesInterior(route[index], route[index + 1], obstacle),
				"A route segment must not cross an obstacle.");
		}
	}

	void RequirePortDirection(Point port, Point adjacent, PortId portId)
	{
		switch (portId)
		{
		case PortId::Top:
			VerifyTestCondition(adjacent.x == port.x && adjacent.y <= port.y, "Top port route must leave upward.");
			break;
		case PortId::Right:
			VerifyTestCondition(adjacent.y == port.y && adjacent.x >= port.x, "Right port route must leave to the right.");
			break;
		case PortId::Bottom:
			VerifyTestCondition(adjacent.x == port.x && adjacent.y >= port.y, "Bottom port route must leave downward.");
			break;
		case PortId::Left:
			VerifyTestCondition(adjacent.y == port.y && adjacent.x <= port.x, "Left port route must leave to the left.");
			break;
		}
	}

	TEST(OrthogonalRouterTests, RoutesOrthogonallyAroundBlocks)
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 100}, L"Source");
		const NodeId obstacleId = diagram.AddNode(NodeKind::Rectangle, {250, 50}, 150, 200, L"Obstacle");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {500, 100}, L"Target");
		const AddConnectionResult added = diagram.AddConnection(
			{source, PortId::Right}, {target, PortId::Left});

		VerifyTestCondition(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
			"Existing connection must be routed.");

		const Connection* connection = diagram.FindConnection(added.connectionId);
		VerifyTestCondition(connection != nullptr, "Routed connection must still exist.");
		const std::vector<Point> route = GetFullRoute(diagram, *connection);
		RequireOrthogonal(route);
		const Node* obstacle = diagram.FindNode(obstacleId);
		VerifyTestCondition(obstacle != nullptr, "Obstacle must exist.");
		RequireAvoids(route, Inflate(GetNodeBounds(*obstacle), 12));
	}

	TEST(OrthogonalRouterTests, ReroutesWhenAnObstacleMoves)
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 100}, L"Source");
		const NodeId obstacleId = diagram.AddNode(NodeKind::Rectangle, {250, 350}, 150, 200, L"Obstacle");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {500, 100}, L"Target");
		const AddConnectionResult added = diagram.AddConnection(
			{source, PortId::Right}, {target, PortId::Left});
		VerifyTestCondition(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
			"Initial connection must be routed.");

		const Connection* initialConnection = diagram.FindConnection(added.connectionId);
		VerifyTestCondition(initialConnection != nullptr, "Initial connection must exist.");
		const std::vector<Point> initialRoute = GetFullRoute(diagram, *initialConnection);
		VerifyTestCondition(initialConnection->intermediatePoints.empty(),
			"Aligned nodes without obstacles should use a direct route.");

		VerifyTestCondition(diagram.MoveNodeTo(obstacleId, {250, 50}) == DiagramError::None,
			"Obstacle must move into the direct route.");
		OrthogonalRouter::RouteAll(diagram);

		const Connection* reroutedConnection = diagram.FindConnection(added.connectionId);
		VerifyTestCondition(reroutedConnection != nullptr, "Rerouted connection must exist.");
		const std::vector<Point> rerouted = GetFullRoute(diagram, *reroutedConnection);
		VerifyTestCondition(rerouted != initialRoute, "The route must change after an obstacle moves into it.");
		RequireOrthogonal(rerouted);
		RequireAvoids(rerouted, Inflate(GetNodeBounds(*diagram.FindNode(obstacleId)), 12));
	}

	TEST(OrthogonalRouterTests, RoutesEveryPortCombination)
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
				VerifyTestCondition(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
					"Every valid port combination must be routable.");

				const Connection* connection = diagram.FindConnection(added.connectionId);
				VerifyTestCondition(connection != nullptr, "Port combination connection must exist.");
				const std::vector<Point> route = GetFullRoute(diagram, *connection);
				VerifyTestCondition(route.size() >= 2, "A routed connection must contain both endpoints.");
				RequireOrthogonal(route);
				RequirePortDirection(route.front(), route[1], sourcePort);
				RequirePortDirection(route.back(), route[route.size() - 2], targetPort);
			}
		}
	}


}
