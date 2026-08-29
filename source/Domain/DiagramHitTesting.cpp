#include "DiagramHitTesting.h"

#include "DiagramGeometry.h"

#include <algorithm>
#include <cmath>

namespace arteststudio::domain
{
	namespace
	{
		[[nodiscard]] bool IsPointInsideNode(const Node& node, Point point) noexcept
		{
			const Rect bounds = GetNodeBounds(node);
			if (node.kind == NodeKind::Rectangle)
			{
				return point.x >= bounds.left && point.x <= bounds.right &&
					point.y >= bounds.top && point.y <= bounds.bottom;
			}

			const double halfWidth = std::max(1, node.width / 2);
			const double halfHeight = std::max(1, node.height / 2);
			const double normalizedX = std::abs(point.x - node.position.x) / halfWidth;
			const double normalizedY = std::abs(point.y - node.position.y) / halfHeight;
			return normalizedX + normalizedY <= 1.0;
		}

		[[nodiscard]] double DistanceToSegment(Point point, Point start, Point end) noexcept
		{
			const double segmentX = static_cast<double>(end.x - start.x);
			const double segmentY = static_cast<double>(end.y - start.y);
			const double lengthSquared = segmentX * segmentX + segmentY * segmentY;
			if (lengthSquared == 0.0)
			{
				return std::hypot(
					static_cast<double>(point.x - start.x),
					static_cast<double>(point.y - start.y));
			}

			const double projection = std::clamp(
				((point.x - start.x) * segmentX + (point.y - start.y) * segmentY) / lengthSquared,
				0.0,
				1.0);
			const double nearestX = start.x + projection * segmentX;
			const double nearestY = start.y + projection * segmentY;
			return std::hypot(point.x - nearestX, point.y - nearestY);
		}
	}

	std::optional<PortHit> DiagramHitTesting::FindPortAt(
		const DiagramModel& diagram,
		Point point,
		int radius) noexcept
	{
		constexpr PortId ports[] = {PortId::Top, PortId::Right, PortId::Bottom, PortId::Left};
		for (auto node = diagram.Nodes().rbegin(); node != diagram.Nodes().rend(); ++node)
		{
			for (const PortId port : ports)
			{
				const Point portPoint = GetConnectionPoint(*node, port);
				if (std::abs(point.x - portPoint.x) <= radius &&
					std::abs(point.y - portPoint.y) <= radius)
				{
					return PortHit{node->id, port, portPoint};
				}
			}
		}
		return std::nullopt;
	}

	std::optional<NodeId> DiagramHitTesting::FindNodeAt(
		const DiagramModel& diagram,
		Point point) noexcept
	{
		for (auto node = diagram.Nodes().rbegin(); node != diagram.Nodes().rend(); ++node)
		{
			if (IsPointInsideNode(*node, point))
			{
				return node->id;
			}
		}
		return std::nullopt;
	}

	std::optional<ConnectionId> DiagramHitTesting::FindConnectionAt(
		const DiagramModel& diagram,
		Point point,
		double tolerance) noexcept
	{
		for (auto connection = diagram.Connections().rbegin(); connection != diagram.Connections().rend(); ++connection)
		{
			const Node* fromNode = diagram.FindNode(connection->from.nodeId);
			const Node* toNode = diagram.FindNode(connection->to.nodeId);
			if (fromNode == nullptr || toNode == nullptr)
			{
				continue;
			}

			Point previous = GetConnectionPoint(*fromNode, connection->from.portId);
			for (const Point routePoint : connection->intermediatePoints)
			{
				if (DistanceToSegment(point, previous, routePoint) <= tolerance)
				{
					return connection->id;
				}
				previous = routePoint;
			}

			const Point end = GetConnectionPoint(*toNode, connection->to.portId);
			if (DistanceToSegment(point, previous, end) <= tolerance)
			{
				return connection->id;
			}
		}
		return std::nullopt;
	}
}
