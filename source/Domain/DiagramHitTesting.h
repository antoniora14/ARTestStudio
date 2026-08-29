#pragma once

#include "DiagramModel.h"

#include <optional>

namespace arteststudio::domain
{
	struct PortHit
	{
		NodeId nodeId;
		PortId portId = PortId::Top;
		Point point;
	};

	class DiagramHitTesting final
	{
	public:
		[[nodiscard]] static std::optional<PortHit> FindPortAt(
			const DiagramModel& diagram,
			Point point,
			int radius = 5) noexcept;

		[[nodiscard]] static std::optional<NodeId> FindNodeAt(
			const DiagramModel& diagram,
			Point point) noexcept;

		[[nodiscard]] static std::optional<ConnectionId> FindConnectionAt(
			const DiagramModel& diagram,
			Point point,
			double tolerance = 6.0) noexcept;
	};
}
