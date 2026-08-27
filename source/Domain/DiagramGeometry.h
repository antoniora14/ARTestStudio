#pragma once

#include "DiagramModel.h"

namespace arteststudio::domain
{
	struct Rect
	{
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;

		auto operator<=>(const Rect&) const = default;
	};

	[[nodiscard]] Rect GetNodeBounds(const Node& node) noexcept;
	[[nodiscard]] Point GetConnectionPoint(const Node& node, PortId portId) noexcept;
	[[nodiscard]] Rect Inflate(Rect rectangle, int amount) noexcept;
	[[nodiscard]] bool ContainsInterior(Rect rectangle, Point point) noexcept;
}
