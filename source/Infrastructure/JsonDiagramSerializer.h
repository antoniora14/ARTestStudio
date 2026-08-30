#pragma once

#include "../Application/DiagramSerializer.h"

namespace arteststudio::infrastructure
{
	class JsonDiagramSerializer final : public application::IDiagramSerializer
	{
	public:
		[[nodiscard]] bool CanDeserialize(std::string_view content) const noexcept;

		[[nodiscard]] application::StorageResult Serialize(
			const domain::DiagramModel& diagram,
			std::string& content) const noexcept override;

		[[nodiscard]] application::StorageResult Deserialize(
			std::string_view content,
			domain::DiagramModel& diagram) const noexcept override;
	};
}
