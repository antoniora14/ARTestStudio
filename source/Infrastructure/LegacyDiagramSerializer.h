#pragma once

#include "../Application/DiagramStorage.h"

#include <string_view>

namespace arteststudio::infrastructure
{
	class LegacyDiagramSerializer final
	{
	public:
		[[nodiscard]] bool CanDeserialize(std::string_view content) const noexcept;

		[[nodiscard]] application::StorageResult Deserialize(
			std::string_view content,
			domain::DiagramModel& diagram) const noexcept;
	};
}
