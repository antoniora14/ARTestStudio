#include "OrthogonalRouter.h"

#include "DiagramGeometry.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace arteststudio::domain
{
	namespace
	{
		enum class Direction : std::uint8_t
		{
			None = 0,
			Horizontal = 1,
			Vertical = 2
		};

		struct QueueEntry
		{
			std::int64_t estimatedCost = 0;
			std::int64_t cost = 0;
			std::size_t nodeIndex = 0;
			Direction direction = Direction::None;

			bool operator>(const QueueEntry& other) const noexcept
			{
				return estimatedCost > other.estimatedCost;
			}
		};

		Point GetEscapePoint(const Node& node, PortId portId, int clearance)
		{
			const Rect bounds = Inflate(GetNodeBounds(node), clearance);
			const Point port = GetConnectionPoint(node, portId);
			switch (portId)
			{
			case PortId::Top:
				return {port.x, bounds.top};
			case PortId::Right:
				return {bounds.right, port.y};
			case PortId::Bottom:
				return {port.x, bounds.bottom};
			case PortId::Left:
				return {bounds.left, port.y};
			default:
				return port;
			}
		}

		void SortAndRemoveDuplicates(std::vector<int>& values)
		{
			std::sort(values.begin(), values.end());
			values.erase(std::unique(values.begin(), values.end()), values.end());
		}

		bool SegmentIsClear(Point first, Point second, const std::vector<Rect>& obstacles)
		{
			if (first.x == second.x)
			{
				const int minimumY = std::min(first.y, second.y);
				const int maximumY = std::max(first.y, second.y);
				for (const Rect obstacle : obstacles)
				{
					if (first.x > obstacle.left && first.x < obstacle.right &&
						maximumY > obstacle.top && minimumY < obstacle.bottom)
					{
						return false;
					}
				}
				return true;
			}

			if (first.y == second.y)
			{
				const int minimumX = std::min(first.x, second.x);
				const int maximumX = std::max(first.x, second.x);
				for (const Rect obstacle : obstacles)
				{
					if (first.y > obstacle.top && first.y < obstacle.bottom &&
						maximumX > obstacle.left && minimumX < obstacle.right)
					{
						return false;
					}
				}
				return true;
			}

			return false;
		}

		std::vector<Point> Simplify(std::vector<Point> points)
		{
			points.erase(std::unique(points.begin(), points.end()), points.end());
			if (points.size() < 3)
			{
				return points;
			}

			std::vector<Point> simplified;
			simplified.reserve(points.size());
			simplified.push_back(points.front());
			for (std::size_t index = 1; index + 1 < points.size(); ++index)
			{
				const Point previous = simplified.back();
				const Point current = points[index];
				const Point next = points[index + 1];
				const bool vertical = previous.x == current.x && current.x == next.x;
				const bool horizontal = previous.y == current.y && current.y == next.y;
				if (!vertical && !horizontal)
				{
					simplified.push_back(current);
				}
			}
			simplified.push_back(points.back());
			return simplified;
		}

		std::int64_t ManhattanDistance(Point first, Point second)
		{
			return static_cast<std::int64_t>(std::abs(first.x - second.x)) +
				static_cast<std::int64_t>(std::abs(first.y - second.y));
		}

		std::size_t StateIndex(std::size_t nodeIndex, Direction direction)
		{
			return nodeIndex * 3 + static_cast<std::size_t>(direction);
		}

		std::vector<Point> BuildFallback(Point start, Point end)
		{
			std::vector<Point> points{start};
			if (start.x != end.x && start.y != end.y)
			{
				points.push_back({end.x, start.y});
			}
			points.push_back(end);
			return points;
		}

		std::vector<Point> FindPath(
			Point start,
			Point goal,
			const std::vector<Rect>& obstacles,
			int bendPenalty)
		{
			std::vector<int> xCoordinates{start.x, goal.x};
			std::vector<int> yCoordinates{start.y, goal.y};
			if (!obstacles.empty())
			{
				int outerLeft = obstacles.front().left;
				int outerTop = obstacles.front().top;
				int outerRight = obstacles.front().right;
				int outerBottom = obstacles.front().bottom;
				for (const Rect obstacle : obstacles)
				{
					xCoordinates.push_back(obstacle.left);
					xCoordinates.push_back(obstacle.right);
					yCoordinates.push_back(obstacle.top);
					yCoordinates.push_back(obstacle.bottom);
					outerLeft = std::min(outerLeft, obstacle.left);
					outerTop = std::min(outerTop, obstacle.top);
					outerRight = std::max(outerRight, obstacle.right);
					outerBottom = std::max(outerBottom, obstacle.bottom);
				}

				constexpr int OuterMargin = 20;
				xCoordinates.push_back(outerLeft - OuterMargin);
				xCoordinates.push_back(outerRight + OuterMargin);
				yCoordinates.push_back(outerTop - OuterMargin);
				yCoordinates.push_back(outerBottom + OuterMargin);
			}

			SortAndRemoveDuplicates(xCoordinates);
			SortAndRemoveDuplicates(yCoordinates);
			const std::size_t width = xCoordinates.size();
			const std::size_t height = yCoordinates.size();
			const std::size_t nodeCount = width * height;

			std::vector<bool> valid(nodeCount, false);
			for (std::size_t yIndex = 0; yIndex < height; ++yIndex)
			{
				for (std::size_t xIndex = 0; xIndex < width; ++xIndex)
				{
					const Point point{xCoordinates[xIndex], yCoordinates[yIndex]};
					const bool insideObstacle = std::any_of(
						obstacles.begin(), obstacles.end(), [point](Rect obstacle)
						{
							return ContainsInterior(obstacle, point);
						});
					valid[yIndex * width + xIndex] = !insideObstacle || point == start || point == goal;
				}
			}

			const auto startX = std::lower_bound(xCoordinates.begin(), xCoordinates.end(), start.x);
			const auto startY = std::lower_bound(yCoordinates.begin(), yCoordinates.end(), start.y);
			const auto goalX = std::lower_bound(xCoordinates.begin(), xCoordinates.end(), goal.x);
			const auto goalY = std::lower_bound(yCoordinates.begin(), yCoordinates.end(), goal.y);
			const std::size_t startNode = static_cast<std::size_t>(startY - yCoordinates.begin()) * width +
				static_cast<std::size_t>(startX - xCoordinates.begin());
			const std::size_t goalNode = static_cast<std::size_t>(goalY - yCoordinates.begin()) * width +
				static_cast<std::size_t>(goalX - xCoordinates.begin());

			const std::int64_t infinity = std::numeric_limits<std::int64_t>::max();
			std::vector<std::int64_t> costs(nodeCount * 3, infinity);
			std::vector<std::int64_t> parents(nodeCount * 3, -1);
			std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> queue;
			const std::size_t initialState = StateIndex(startNode, Direction::None);
			costs[initialState] = 0;
			queue.push({ManhattanDistance(start, goal), 0, startNode, Direction::None});

			std::size_t finalState = initialState;
			bool found = startNode == goalNode;
			while (!queue.empty() && !found)
			{
				const QueueEntry current = queue.top();
				queue.pop();
				const std::size_t currentState = StateIndex(current.nodeIndex, current.direction);
				if (current.cost != costs[currentState])
				{
					continue;
				}

				if (current.nodeIndex == goalNode)
				{
					found = true;
					finalState = currentState;
					break;
				}

				const std::size_t xIndex = current.nodeIndex % width;
				const std::size_t yIndex = current.nodeIndex / width;
				const Point currentPoint{xCoordinates[xIndex], yCoordinates[yIndex]};
				const std::array<std::pair<int, int>, 4> offsets{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
				for (const auto [xOffset, yOffset] : offsets)
				{
					const int neighborX = static_cast<int>(xIndex) + xOffset;
					const int neighborY = static_cast<int>(yIndex) + yOffset;
					if (neighborX < 0 || neighborY < 0 ||
						neighborX >= static_cast<int>(width) || neighborY >= static_cast<int>(height))
					{
						continue;
					}

					const std::size_t neighborNode = static_cast<std::size_t>(neighborY) * width +
						static_cast<std::size_t>(neighborX);
					if (!valid[neighborNode])
					{
						continue;
					}

					const Point neighborPoint{xCoordinates[neighborX], yCoordinates[neighborY]};
					if (!SegmentIsClear(currentPoint, neighborPoint, obstacles))
					{
						continue;
					}

					const Direction nextDirection = xOffset == 0 ? Direction::Vertical : Direction::Horizontal;
					const std::int64_t turnCost = current.direction != Direction::None &&
						current.direction != nextDirection ? bendPenalty : 0;
					const std::int64_t nextCost = current.cost +
						ManhattanDistance(currentPoint, neighborPoint) + turnCost;
					const std::size_t nextState = StateIndex(neighborNode, nextDirection);
					if (nextCost >= costs[nextState])
					{
						continue;
					}

					costs[nextState] = nextCost;
					parents[nextState] = static_cast<std::int64_t>(currentState);
					queue.push({
						nextCost + ManhattanDistance(neighborPoint, goal),
						nextCost,
						neighborNode,
						nextDirection});
				}
			}

			if (!found)
			{
				return BuildFallback(start, goal);
			}

			std::vector<Point> reversedPath;
			for (std::int64_t state = static_cast<std::int64_t>(finalState); state >= 0; state = parents[state])
			{
				const std::size_t nodeIndex = static_cast<std::size_t>(state) / 3;
				const std::size_t xIndex = nodeIndex % width;
				const std::size_t yIndex = nodeIndex / width;
				reversedPath.push_back({xCoordinates[xIndex], yCoordinates[yIndex]});
				if (static_cast<std::size_t>(state) == initialState)
				{
					break;
				}
			}

			std::reverse(reversedPath.begin(), reversedPath.end());
			return Simplify(std::move(reversedPath));
		}
	}

	std::vector<Point> OrthogonalRouter::ComputeRoute(
		const DiagramModel& diagram,
		const Connection& connection,
		RoutingOptions options)
	{
		const Node* sourceNode = diagram.FindNode(connection.from.nodeId);
		const Node* targetNode = diagram.FindNode(connection.to.nodeId);
		if (sourceNode == nullptr || targetNode == nullptr)
		{
			return {};
		}

		const Point sourcePoint = GetConnectionPoint(*sourceNode, connection.from.portId);
		const Point targetPoint = GetConnectionPoint(*targetNode, connection.to.portId);
		const Point sourceEscape = GetEscapePoint(*sourceNode, connection.from.portId, options.obstacleClearance);
		const Point targetEscape = GetEscapePoint(*targetNode, connection.to.portId, options.obstacleClearance);

		std::vector<Rect> obstacles;
		obstacles.reserve(diagram.Nodes().size());
		for (const Node& node : diagram.Nodes())
		{
			obstacles.push_back(Inflate(GetNodeBounds(node), options.obstacleClearance));
		}

		std::vector<Point> fullRoute{sourcePoint};
		std::vector<Point> routed = FindPath(sourceEscape, targetEscape, obstacles, options.bendPenalty);
		fullRoute.insert(fullRoute.end(), routed.begin(), routed.end());
		fullRoute.push_back(targetPoint);
		fullRoute = Simplify(std::move(fullRoute));

		if (fullRoute.size() <= 2)
		{
			return {};
		}
		return {fullRoute.begin() + 1, fullRoute.end() - 1};
	}

	bool OrthogonalRouter::RouteConnection(
		DiagramModel& diagram,
		ConnectionId connectionId,
		RoutingOptions options)
	{
		Connection* connection = diagram.FindConnection(connectionId);
		if (connection == nullptr)
		{
			return false;
		}

		connection->intermediatePoints = ComputeRoute(diagram, *connection, options);
		return true;
	}

	void OrthogonalRouter::RouteAll(DiagramModel& diagram, RoutingOptions options)
	{
		std::vector<ConnectionId> connectionIds;
		connectionIds.reserve(diagram.Connections().size());
		for (const Connection& connection : diagram.Connections())
		{
			connectionIds.push_back(connection.id);
		}

		for (const ConnectionId connectionId : connectionIds)
		{
			(void)RouteConnection(diagram, connectionId, options);
		}
	}
}
