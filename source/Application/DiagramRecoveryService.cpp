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
				std::wstring technicalDetail = L"Destino: ";
				technicalDetail += destination.native();
				if (candidate != nullptr)
				{
					technicalDetail += L" | Candidato: ";
					technicalDetail += candidate->path.native();
					technicalDetail += L" | Origen: ";
					technicalDetail += DescribeRecoverySource(candidate->source);
					technicalDetail += L" | Motivo: ";
					technicalDetail += DescribeRecoveryReason(candidate->reason);
				}
				if (!detail.empty())
				{
					technicalDetail += L" | Detalle: ";
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
				L"Inspeccionar recuperacion",
				L"No se pudo completar la inspeccion de copias de recuperacion.",
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
				L"Inspeccionar recuperacion",
				L"Se ignoro una copia de version anterior porque no contiene un diagrama valido.",
				path);
		}
		if (inspection.invalidInterruptedSaveDetected)
		{
			ReportRecoveryFault(
				FaultSeverity::Warning,
				L"RECOVERY_INVALID_INTERRUPTED_SAVE",
				L"Inspeccionar recuperacion",
				L"Se ignoro un archivo temporal incompleto o corrupto.",
				path);
		}
		if (inspection.candidate.available)
		{
			ReportRecoveryFault(
				inspection.candidate.reason == RecoveryReason::PrimaryInvalid
					? FaultSeverity::Warning
					: FaultSeverity::Information,
				L"RECOVERY_CANDIDATE_DETECTED",
				L"Inspeccionar recuperacion",
				L"Se encontro una copia valida que puede recuperarse.",
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
			L"Recuperar diagrama",
			L"Se inicio la recuperacion controlada del documento.",
			destination,
			&candidate);

		const StorageResult result = m_storage->Recover(destination, candidate, diagram);
		if (result)
		{
			ReportRecoveryFault(
				FaultSeverity::Information,
				L"RECOVERY_COMPLETED",
				L"Recuperar diagrama",
				L"La recuperacion controlada termino correctamente.",
				destination,
				&candidate);
		}
		else
		{
			ReportRecoveryFault(
				FaultSeverity::Error,
				L"RECOVERY_FAILED",
				L"Recuperar diagrama",
				L"La recuperacion controlada no pudo completarse.",
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
			L"Recuperar diagrama",
			L"El usuario decidio no utilizar la copia de recuperacion.",
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
			L"Recuperar diagrama",
			L"El usuario cancelo la apertura sin modificar el documento ni la copia de recuperacion.",
			destination,
			&candidate);
	}
}
