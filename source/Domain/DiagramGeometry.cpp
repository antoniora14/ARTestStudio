#include "DiagramGeometry.h"

namespace arteststudio::domain
{
	Rect GetNodeBounds(const Node& node) noexcept
	{
		if (node.kind == NodeKind::Diamond)
		{
			const int halfWidth = node.width / 2;
			const int halfHeight = node.height / 2;
			return {
				node.position.x - halfWidth,
				node.position.y - halfHeight,
				node.position.x + halfWidth,
				node.position.y + halfHeight};
		}

		return
		{
			node.position.x,
			node.position.y,
			node.position.x + node.width,
			node.position.y + node.height
		};
	}

	Point GetConnectionPoint(const Node& node, PortId portId) noexcept
	{
		const Rect bounds = GetNodeBounds(node);
		switch (portId)
		{
		case PortId::Top:
			return {(bounds.left + bounds.right) / 2, bounds.top};
		case PortId::Right:
			return {bounds.right, (bounds.top + bounds.bottom) / 2};
		case PortId::Bottom:
			return {(bounds.left + bounds.right) / 2, bounds.bottom};
		case PortId::Left:
			return {bounds.left, (bounds.top + bounds.bottom) / 2};
		default:
			return {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
		}
	}

	Rect Inflate(Rect rectangle, int amount) noexcept
	{
		return {
			rectangle.left - amount,
			rectangle.top - amount,
			rectangle.right + amount,
			rectangle.bottom + amount};
	}

	bool ContainsInterior(Rect rectangle, Point point) noexcept
	{
		return point.x > rectangle.left && point.x < rectangle.right &&
			point.y > rectangle.top && point.y < rectangle.bottom;
	}
}
