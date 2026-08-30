#pragma once

#include "../Application/AtomicFileWriter.h"
#include "../Application/DiagramRecovery.h"
#include "../Application/DiagramStorage.h"

namespace arteststudio::infrastructure
{
	class TextDiagramStorage final
		: public application::IDiagramStorage,
		  public application::IDiagramRecoveryStorage
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

		[[nodiscard]] application::RecoveryInspection InspectRecovery(
			const std::filesystem::path& path) const noexcept override;

		[[nodiscard]] application::StorageResult Recover(
			const std::filesystem::path& destination,
			const application::RecoveryCandidate& candidate,
			domain::DiagramModel& diagram) const noexcept override;

		[[nodiscard]] static std::filesystem::path PreviousVersionPathFor(
			const std::filesystem::path& destination);

		[[nodiscard]] static std::filesystem::path InterruptedSavePathFor(
			const std::filesystem::path& destination);

private:
		const application::IAtomicFileWriter* m_writer;
	};
}
