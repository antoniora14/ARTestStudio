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
			return L"ultima version valida";
		case RecoverySource::InterruptedSave:
			return L"guardado interrumpido";
		case RecoverySource::None:
			break;
		}
		return L"origen desconocido";
	}

	[[nodiscard]] constexpr std::wstring_view DescribeRecoveryReason(RecoveryReason reason) noexcept
	{
		switch (reason)
		{
		case RecoveryReason::PrimaryMissing:
			return L"el documento principal no existe";
		case RecoveryReason::PrimaryInvalid:
			return L"el documento principal no es valido";
		case RecoveryReason::CandidateNewer:
			return L"la copia de recuperacion es mas reciente";
		case RecoveryReason::None:
			break;
		}
		return L"sin motivo de recuperacion";
	}
}
