#pragma once

#include "DiagramModel.h"

#include <vector>

namespace arteststudio::domain
{
	struct RoutingOptions
	{
		int obstacleClearance = 12;
		int bendPenalty = 40;
	};

	class OrthogonalRouter
	{
	public:
		[[nodiscard]] static std::vector<Point> ComputeRoute(
			const DiagramModel& diagram,
			const Connection& connection,
			RoutingOptions options = {});

		[[nodiscard]] static bool RouteConnection(
			DiagramModel& diagram,
			ConnectionId connectionId,
			RoutingOptions options = {});

		static void RouteAll(DiagramModel& diagram, RoutingOptions options = {});
	};
}
