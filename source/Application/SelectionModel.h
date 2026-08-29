#pragma once

#include "../Domain/DiagramModel.h"

#include <optional>

namespace arteststudio::application
{
	class SelectionModel
	{
	public:
		void SelectNode(domain::NodeId nodeId) noexcept
		{
			m_nodeId = nodeId;
			m_connectionId.reset();
		}

		void SelectConnection(domain::ConnectionId connectionId) noexcept
		{
			m_connectionId = connectionId;
			m_nodeId.reset();
		}

		void Clear() noexcept
		{
			m_nodeId.reset();
			m_connectionId.reset();
		}

		[[nodiscard]] const std::optional<domain::NodeId>& NodeId() const noexcept
		{
			return m_nodeId;
		}

		[[nodiscard]] const std::optional<domain::ConnectionId>& ConnectionId() const noexcept
		{
			return m_connectionId;
		}

		[[nodiscard]] bool IsSelected(domain::NodeId nodeId) const noexcept
		{
			return m_nodeId == nodeId;
		}

		[[nodiscard]] bool IsSelected(domain::ConnectionId connectionId) const noexcept
		{
			return m_connectionId == connectionId;
		}

	private:
		std::optional<domain::NodeId> m_nodeId;
		std::optional<domain::ConnectionId> m_connectionId;
	};
}
