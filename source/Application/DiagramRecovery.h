#pragma once

#include "DiagramStorage.h"

#include <filesystem>

namespace arteststudio::application
{
	enum class RecoverySource
	{
		None,
		PreviousVersion,
		InterruptedSave
	};

	enum class RecoveryReason
	{
		None,
		PrimaryMissing,
		PrimaryInvalid,
		CandidateNewer
	};

	struct RecoveryCandidate
	{
		bool available = false;
		RecoverySource source = RecoverySource::None;
		RecoveryReason reason = RecoveryReason::None;
		std::filesystem::path path;
	};

	struct RecoveryInspection
	{
		StorageResult result;
		RecoveryCandidate candidate;
		StorageError primaryError = StorageError::None;
		bool invalidPreviousVersionDetected = false;
		bool invalidInterruptedSaveDetected = false;
	};

	class IDiagramRecoveryStorage
	{
	public:
		virtual ~IDiagramRecoveryStorage() = default;

		[[nodiscard]] virtual RecoveryInspection InspectRecovery(
			const std::filesystem::path& path) const noexcept = 0;

		[[nodiscard]] virtual StorageResult Recover(
			const std::filesystem::path& destination,
			const RecoveryCandidate& candidate,
			domain::DiagramModel& diagram) const noexcept = 0;
	};

	[[nodiscard]] constexpr std::wstring_view DescribeRecoverySource(RecoverySource source) noexcept
	{
		switch (source)
		{
		case RecoverySource::PreviousVersion:
			return L"last valid version";
		case RecoverySource::InterruptedSave:
			return L"interrupted save";
		case RecoverySource::None:
			break;
		}
		return L"unknown source";
	}

	[[nodiscard]] constexpr std::wstring_view DescribeRecoveryReason(RecoveryReason reason) noexcept
	{
		switch (reason)
		{
		case RecoveryReason::PrimaryMissing:
			return L"the primary document is missing";
		case RecoveryReason::PrimaryInvalid:
			return L"the primary document is invalid";
		case RecoveryReason::CandidateNewer:
			return L"the recovery copy is newer";
		case RecoveryReason::None:
			break;
		}
		return L"no recovery reason";
	}
}
