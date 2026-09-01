#include "DiagramRecoveryService.h"

#include "FaultService.h"

#include <string>
#include <string_view>
#include <utility>

namespace arteststudio::application
{
	namespace
	{
		void ReportRecoveryFault(
			FaultSeverity severity,
			std::wstring_view code,
			std::wstring_view operation,
			std::wstring_view message,
			const std::filesystem::path& destination,
			const RecoveryCandidate* candidate = nullptr,
			std::wstring_view detail = {}) noexcept
		{
			try
			{
				std::wstring technicalDetail = L"Destination: ";
				technicalDetail += destination.native();
				if (candidate != nullptr)
				{
					technicalDetail += L" | Candidate: ";
					technicalDetail += candidate->path.native();
					technicalDetail += L" | Source: ";
					technicalDetail += DescribeRecoverySource(candidate->source);
					technicalDetail += L" | Reason: ";
					technicalDetail += DescribeRecoveryReason(candidate->reason);
				}
				if (!detail.empty())
				{
					technicalDetail += L" | Detail: ";
					technicalDetail += detail;
				}

				FaultService::Report(Fault{
					severity,
					FaultCategory::Storage,
					std::wstring{code},
					std::wstring{operation},
					std::wstring{message},
					std::move(technicalDetail)});
			}
			catch (...)
			{
			}
		}
	}

	DiagramRecoveryService::DiagramRecoveryService(IDiagramRecoveryStorage& storage) noexcept
		: m_storage(&storage)
	{
	}

	RecoveryInspection DiagramRecoveryService::Inspect(
		const std::filesystem::path& path) const noexcept
	{
		RecoveryInspection inspection = m_storage->InspectRecovery(path);
		if (!inspection.result)
		{
			ReportRecoveryFault(
				FaultSeverity::Warning,
				L"RECOVERY_INSPECTION_FAILED",
				L"Inspect recovery",
				L"Recovery-copy inspection could not be completed.",
				path,
				nullptr,
				inspection.result.detail);
			return inspection;
		}

		if (inspection.invalidPreviousVersionDetected)
		{
			ReportRecoveryFault(
				FaultSeverity::Warning,
				L"RECOVERY_INVALID_PREVIOUS_VERSION",
				L"Inspect recovery",
				L"A previous-version copy was ignored because it does not contain a valid diagram.",
				path);
		}
		if (inspection.invalidInterruptedSaveDetected)
		{
			ReportRecoveryFault(
				FaultSeverity::Warning,
				L"RECOVERY_INVALID_INTERRUPTED_SAVE",
				L"Inspect recovery",
				L"An incomplete or corrupted temporary file was ignored.",
				path);
		}
		if (inspection.candidate.available)
		{
			ReportRecoveryFault(
				inspection.candidate.reason == RecoveryReason::PrimaryInvalid
					? FaultSeverity::Warning
					: FaultSeverity::Information,
				L"RECOVERY_CANDIDATE_DETECTED",
				L"Inspect recovery",
				L"A valid recoverable copy was found.",
				path,
				&inspection.candidate);
		}
		return inspection;
	}

	StorageResult DiagramRecoveryService::Recover(
		const std::filesystem::path& destination,
		const RecoveryCandidate& candidate,
		domain::DiagramModel& diagram) const noexcept
	{
		ReportRecoveryFault(
			FaultSeverity::Information,
			L"RECOVERY_STARTED",
			L"Recover diagram",
			L"Controlled document recovery started.",
			destination,
			&candidate);

		const StorageResult result = m_storage->Recover(destination, candidate, diagram);
		if (result)
		{
			ReportRecoveryFault(
				FaultSeverity::Information,
				L"RECOVERY_COMPLETED",
				L"Recover diagram",
				L"Controlled recovery completed successfully.",
				destination,
				&candidate);
		}
		else
		{
			ReportRecoveryFault(
				FaultSeverity::Error,
				L"RECOVERY_FAILED",
				L"Recover diagram",
				L"Controlled recovery could not be completed.",
				destination,
				&candidate,
				result.detail);
		}
		return result;
	}

	void DiagramRecoveryService::RecordDeclined(
		const std::filesystem::path& destination,
		const RecoveryCandidate& candidate) const noexcept
	{
		ReportRecoveryFault(
			FaultSeverity::Information,
			L"RECOVERY_DECLINED",
			L"Recover diagram",
			L"The user declined the recovery copy.",
			destination,
			&candidate);
	}

	void DiagramRecoveryService::RecordCancelled(
		const std::filesystem::path& destination,
		const RecoveryCandidate& candidate) const noexcept
	{
		ReportRecoveryFault(
			FaultSeverity::Information,
			L"RECOVERY_CANCELLED",
			L"Recover diagram",
			L"The user cancelled opening without modifying the document or recovery copy.",
			destination,
			&candidate);
	}
}
