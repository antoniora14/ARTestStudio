#pragma once

#include "DiagramStorage.h"

#include <string>
#include <string_view>

namespace arteststudio::application
{
	class IDiagramSerializer
	{
	public:
		virtual ~IDiagramSerializer() = default;

		[[nodiscard]] virtual StorageResult Serialize(
			const domain::DiagramModel& diagram,
			std::string& content) const noexcept = 0;

		[[nodiscard]] virtual StorageResult Deserialize(
			std::string_view content,
			domain::DiagramModel& diagram) const noexcept = 0;
	};
}
