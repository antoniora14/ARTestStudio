#include "DiagramModel.h"

#include <algorithm>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace arteststudio::domain
{
	NodeId DiagramModel::AddNode(NodeKind kind, Point position, std::wstring label)
	{
		const int width = kind == NodeKind::Diamond ? 100 : 150;
		const int height = kind == NodeKind::Diamond ? 80 : 100;
		return AddNode(kind, position, width, height, std::move(label));
	}

	NodeId DiagramModel::AddNode(NodeKind kind, Point position, int width, int height, std::wstring label)
	{
		const NodeId id{m_nextNodeId++};
		m_nodes.push_back(Node{id, kind, position, width, height, std::move(label)});
		return id;
	}

	DiagramError DiagramModel::MoveNodeBy(NodeId nodeId, int dx, int dy) noexcept
	{
		Node* node = FindNode(nodeId);
		if (node == nullptr)
		{
			return DiagramError::NodeNotFound;
		}

		node->position.x += dx;
		node->position.y += dy;
		return DiagramError::None;
	}

	DiagramError DiagramModel::MoveNodeTo(NodeId nodeId, Point position) noexcept
	{
		Node* node = FindNode(nodeId);
		if (node == nullptr)
		{
			return DiagramError::NodeNotFound;
		}

		node->position = position;
		return DiagramError::None;
	}

	DiagramError DiagramModel::RemoveNode(NodeId nodeId) noexcept
	{
		const auto node = std::find_if(m_nodes.begin(), m_nodes.end(), [nodeId](const Node& candidate)
		{
			return candidate.id == nodeId;
		});

		if (node == m_nodes.end())
		{
			return DiagramError::NodeNotFound;
		}

		m_connections.erase(
			std::remove_if(m_connections.begin(), m_connections.end(), [nodeId](const Connection& connection)
			{
				return connection.from.nodeId == nodeId || connection.to.nodeId == nodeId;
			}),
			m_connections.end());
		m_nodes.erase(node);
		return DiagramError::None;
	}

	AddConnectionResult DiagramModel::AddConnection(
		ConnectionEndpoint from,
		ConnectionEndpoint to,
		std::vector<Point> intermediatePoints)
	{
		if (FindNode(from.nodeId) == nullptr || FindNode(to.nodeId) == nullptr)
		{
			return {{}, DiagramError::NodeNotFound};
		}

		if (!IsValidPort(from.portId) || !IsValidPort(to.portId))
		{
			return {{}, DiagramError::InvalidPort};
		}

		const ConnectionId id{m_nextConnectionId++};
		m_connections.push_back(Connection{id, from, to, std::move(intermediatePoints)});
		return {id, DiagramError::None};
	}

	DiagramError DiagramModel::RemoveConnection(ConnectionId connectionId) noexcept
	{
		const auto connection = std::find_if(
			m_connections.begin(), m_connections.end(), [connectionId](const Connection& candidate)
			{
				return candidate.id == connectionId;
			});

		if (connection == m_connections.end())
		{
			return DiagramError::ConnectionNotFound;
		}

		m_connections.erase(connection);
		return DiagramError::None;
	}

	Node* DiagramModel::FindNode(NodeId nodeId) noexcept
	{
		const auto node = std::find_if(m_nodes.begin(), m_nodes.end(), [nodeId](const Node& candidate)
		{
			return candidate.id == nodeId;
		});
		return node == m_nodes.end() ? nullptr : &*node;
	}

	const Node* DiagramModel::FindNode(NodeId nodeId) const noexcept
	{
		const auto node = std::find_if(m_nodes.cbegin(), m_nodes.cend(), [nodeId](const Node& candidate)
		{
			return candidate.id == nodeId;
		});
		return node == m_nodes.cend() ? nullptr : &*node;
	}

	Connection* DiagramModel::FindConnection(ConnectionId connectionId) noexcept
	{
		const auto connection = std::find_if(
			m_connections.begin(), m_connections.end(), [connectionId](const Connection& candidate)
			{
				return candidate.id == connectionId;
			});
		return connection == m_connections.end() ? nullptr : &*connection;
	}

	const Connection* DiagramModel::FindConnection(ConnectionId connectionId) const noexcept
	{
		const auto connection = std::find_if(
			m_connections.cbegin(), m_connections.cend(), [connectionId](const Connection& candidate)
			{
				return candidate.id == connectionId;
			});
		return connection == m_connections.cend() ? nullptr : &*connection;
	}

	DiagramSnapshot DiagramModel::CaptureSnapshot() const
	{
		return {m_nodes, m_connections};
	}

	RestoreSnapshotResult DiagramModel::ValidateSnapshot(const DiagramSnapshot& snapshot) noexcept
	{
		try
		{
			std::unordered_set<std::uint64_t> nodeIds;
			nodeIds.reserve(snapshot.nodes.size());
			for (const Node& node : snapshot.nodes)
			{
				if (!node.id)
				{
					return {DiagramSnapshotError::InvalidNodeId, node.id.value};
				}
				if (!nodeIds.insert(node.id.value).second)
				{
					return {DiagramSnapshotError::DuplicateNodeId, node.id.value};
				}
				if (node.kind != NodeKind::Rectangle && node.kind != NodeKind::Diamond)
				{
					return {DiagramSnapshotError::InvalidNodeKind, node.id.value};
				}
				if (node.width <= 0 || node.height <= 0)
				{
					return {DiagramSnapshotError::InvalidNodeDimensions, node.id.value};
				}
				if (node.id.value == std::numeric_limits<std::uint64_t>::max())
				{
					return {DiagramSnapshotError::IdentifierOverflow, node.id.value};
				}
			}

			std::unordered_set<std::uint64_t> connectionIds;
			connectionIds.reserve(snapshot.connections.size());
			for (const Connection& connection : snapshot.connections)
			{
				if (!connection.id)
				{
					return {DiagramSnapshotError::InvalidConnectionId, connection.id.value};
				}
				if (!connectionIds.insert(connection.id.value).second)
				{
					return {DiagramSnapshotError::DuplicateConnectionId, connection.id.value};
				}
				if (connection.id.value == std::numeric_limits<std::uint64_t>::max())
				{
					return {DiagramSnapshotError::IdentifierOverflow, connection.id.value};
				}
				if (!nodeIds.contains(connection.from.nodeId.value))
				{
					return {DiagramSnapshotError::NodeNotFound, connection.from.nodeId.value};
				}
				if (!nodeIds.contains(connection.to.nodeId.value))
				{
					return {DiagramSnapshotError::NodeNotFound, connection.to.nodeId.value};
				}
				if (!IsValidPort(connection.from.portId) || !IsValidPort(connection.to.portId))
				{
					return {DiagramSnapshotError::InvalidPort, connection.id.value};
				}
			}

			return {};
		}
		catch (const std::bad_alloc&)
		{
			return {DiagramSnapshotError::AllocationFailure};
		}
		catch (...)
		{
			return {DiagramSnapshotError::UnexpectedFailure};
		}
	}

	RestoreSnapshotResult DiagramModel::RestoreSnapshot(DiagramSnapshot snapshot) noexcept
	{
		const RestoreSnapshotResult validation = ValidateSnapshot(snapshot);
		if (!validation)
		{
			return validation;
		}

		std::uint64_t maximumNodeId = 0;
		for (const Node& node : snapshot.nodes)
		{
			maximumNodeId = std::max(maximumNodeId, node.id.value);
		}

		std::uint64_t maximumConnectionId = 0;
		for (const Connection& connection : snapshot.connections)
		{
			maximumConnectionId = std::max(maximumConnectionId, connection.id.value);
		}

		m_nodes = std::move(snapshot.nodes);
		m_connections = std::move(snapshot.connections);
		m_nextNodeId = maximumNodeId + 1;
		m_nextConnectionId = maximumConnectionId + 1;
		return {};
	}

	void DiagramModel::Clear() noexcept
	{
		m_nodes.clear();
		m_connections.clear();
		m_nextNodeId = 1;
		m_nextConnectionId = 1;
	}

	bool DiagramModel::IsValidPort(PortId portId) noexcept
	{
		return portId == PortId::Top || portId == PortId::Right ||
			portId == PortId::Bottom || portId == PortId::Left;
	}
}
