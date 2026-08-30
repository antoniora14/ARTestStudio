#pragma once

#include "DiagramRecovery.h"

namespace arteststudio::application
{
	class DiagramRecoveryService final
	{
	public:
		explicit DiagramRecoveryService(IDiagramRecoveryStorage& storage) noexcept;

		[[nodiscard]] RecoveryInspection Inspect(
			const std::filesystem::path& path) const noexcept;

		[[nodiscard]] StorageResult Recover(
			const std::filesystem::path& destination,
			const RecoveryCandidate& candidate,
			domain::DiagramModel& diagram) const noexcept;

		void RecordDeclined(
			const std::filesystem::path& destination,
			const RecoveryCandidate& candidate) const noexcept;

		void RecordCancelled(
			const std::filesystem::path& destination,
			const RecoveryCandidate& candidate) const noexcept;

	private:
		IDiagramRecoveryStorage* m_storage;
	};
}
