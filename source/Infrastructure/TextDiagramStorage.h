#pragma once

#include "../Application/AtomicFileWriter.h"
#include "../Application/DiagramStorage.h"

namespace arteststudio::infrastructure
{
	class TextDiagramStorage final : public application::IDiagramStorage
	{
	public:
		TextDiagramStorage() noexcept;
		explicit TextDiagramStorage(const application::IAtomicFileWriter& writer) noexcept;

		[[nodiscard]] application::StorageResult Load(
			const std::filesystem::path& path,
			domain::DiagramModel& diagram) const noexcept override;

		[[nodiscard]] application::StorageResult Save(
			const std::filesystem::path& path,
			const domain::DiagramModel& diagram) const noexcept override;

	private:
		const application::IAtomicFileWriter* m_writer;
	};
}
