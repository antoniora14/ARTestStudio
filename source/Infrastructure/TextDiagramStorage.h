#pragma once

#include "../Application/DiagramStorage.h"

namespace arteststudio::infrastructure
{
	class TextDiagramStorage final : public application::IDiagramStorage
	{
	public:
		[[nodiscard]] application::StorageResult Load(
			const std::filesystem::path& path,
			domain::DiagramModel& diagram) const noexcept override;

		[[nodiscard]] application::StorageResult Save(
			const std::filesystem::path& path,
			const domain::DiagramModel& diagram) const noexcept override;
	};
}
