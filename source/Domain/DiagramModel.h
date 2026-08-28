#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arteststudio::domain
{
	struct NodeId
	{
		std::uint64_t value = 0;

		[[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
		auto operator<=>(const NodeId&) const = default;
	};

	struct ConnectionId
	{
		std::uint64_t value = 0;

		[[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
		auto operator<=>(const ConnectionId&) const = default;
	};

	struct Point
	{
		int x = 0;
		int y = 0;

		auto operator<=>(const Point&) const = default;
	};

	enum class NodeKind
	{
		Rectangle,
		Diamond
	};

	enum class PortId : std::uint8_t
	{
		Top = 0,
		Right = 1,
		Bottom = 2,
		Left = 3
	};

	struct Node
	{
		NodeId id;
		NodeKind kind = NodeKind::Rectangle;
		Point position;
		int width = 150;
		int height = 100;
		std::wstring label;
	};

	struct ConnectionEndpoint
	{
		NodeId nodeId;
		PortId portId = PortId::Top;
	};

	struct Connection
	{
		ConnectionId id;
		ConnectionEndpoint from;
		ConnectionEndpoint to;
		std::vector<Point> intermediatePoints;
	};

	struct DiagramSnapshot
	{
		std::vector<Node> nodes;
		std::vector<Connection> connections;
	};

	enum class DiagramSnapshotError
	{
		None,
		InvalidNodeId,
		DuplicateNodeId,
		InvalidNodeKind,
		InvalidNodeDimensions,
		InvalidConnectionId,
		DuplicateConnectionId,
		NodeNotFound,
		InvalidPort,
		IdentifierOverflow,
		AllocationFailure,
		UnexpectedFailure
	};

	struct RestoreSnapshotResult
	{
		DiagramSnapshotError error = DiagramSnapshotError::None;
		std::uint64_t identifier = 0;

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return error == DiagramSnapshotError::None;
		}
	};

	enum class DiagramError
	{
		None,
		NodeNotFound,
		ConnectionNotFound,
		InvalidPort
	};

	struct AddConnectionResult
	{
		ConnectionId connectionId;
		DiagramError error = DiagramError::None;

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return error == DiagramError::None && static_cast<bool>(connectionId);
		}
	};

	class DiagramModel
	{
	public:
		[[nodiscard]] NodeId AddNode(NodeKind kind, Point position, std::wstring label);
		[[nodiscard]] NodeId AddNode(NodeKind kind, Point position, int width, int height, std::wstring label);

		[[nodiscard]] DiagramError MoveNodeBy(NodeId nodeId, int dx, int dy) noexcept;
		[[nodiscard]] DiagramError MoveNodeTo(NodeId nodeId, Point position) noexcept;
		[[nodiscard]] DiagramError RemoveNode(NodeId nodeId) noexcept;

		[[nodiscard]] AddConnectionResult AddConnection(ConnectionEndpoint from, ConnectionEndpoint to, std::vector<Point> intermediatePoints = {});
		[[nodiscard]] DiagramError RemoveConnection(ConnectionId connectionId) noexcept;

		[[nodiscard]] Node* FindNode(NodeId nodeId) noexcept;
		[[nodiscard]] const Node* FindNode(NodeId nodeId) const noexcept;
		[[nodiscard]] Connection* FindConnection(ConnectionId connectionId) noexcept;
		[[nodiscard]] const Connection* FindConnection(ConnectionId connectionId) const noexcept;

		[[nodiscard]] const std::vector<Node>& Nodes() const noexcept { return m_nodes; }
		[[nodiscard]] const std::vector<Connection>& Connections() const noexcept { return m_connections; }
		[[nodiscard]] DiagramSnapshot CaptureSnapshot() const;
		[[nodiscard]] static RestoreSnapshotResult ValidateSnapshot(const DiagramSnapshot& snapshot) noexcept;
		[[nodiscard]] RestoreSnapshotResult RestoreSnapshot(DiagramSnapshot snapshot) noexcept;

		void Clear() noexcept;

	private:
		[[nodiscard]] static bool IsValidPort(PortId portId) noexcept;

		std::vector<Node> m_nodes;
		std::vector<Connection> m_connections;
		std::uint64_t m_nextNodeId = 1;
		std::uint64_t m_nextConnectionId = 1;
	};
}
