#include "TextDiagramStorage.h"
#include "DiagramSerializationSupport.h"
#include "VersionedDiagramSerializer.h"
#include "WindowsAtomicFileWriter.h"

#include <cwchar>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace arteststudio::infrastructure
{
	namespace
	{
		using application::AtomicWriteError;
		using application::AtomicWriteResult;
		using application::DiagramStorageLimits;
		using application::RecoveryCandidate;
		using application::RecoveryReason;
		using application::RecoverySource;
		using application::StorageError;
		using application::StorageResult;
		using application::kDiagramFileExtension;
		using serialization::Failure;

		[[nodiscard]] const VersionedDiagramSerializer& Serializer() noexcept
		{
			static const VersionedDiagramSerializer serializer;
			return serializer;
		}

		[[nodiscard]] bool HasDiagramFileExtension(const std::filesystem::path& path) noexcept
		{
			const std::wstring extension = path.extension().native();
			return _wcsicmp(extension.c_str(), kDiagramFileExtension.data()) == 0;
		}

		[[nodiscard]] StorageResult MapAtomicWriteFailure(const AtomicWriteResult& result)
		{
			switch (result.error)
			{
			case AtomicWriteError::None:
				return {};
			case AtomicWriteError::InvalidPath:
				return Failure(StorageError::InvalidPath, result.detail);
			case AtomicWriteError::AccessDenied:
				return Failure(StorageError::AccessDenied, result.detail);
			case AtomicWriteError::TemporaryCleanupFailure:
			case AtomicWriteError::TemporaryWriteFailure:
				return Failure(StorageError::TemporaryFileFailure, result.detail);
			case AtomicWriteError::ReplacementFailure:
				return Failure(StorageError::ReplacementFailure, result.detail);
			}
			return Failure(StorageError::IoFailure, result.detail);
		}

		[[nodiscard]] StorageResult ReadFile(
			const std::filesystem::path& path,
			std::uintmax_t fileSize,
			std::string& content)
		{
			if (fileSize > static_cast<std::uintmax_t>((std::numeric_limits<std::streamsize>::max)()))
			{
				return Failure(StorageError::FileTooLarge);
			}

			std::ifstream input(path, std::ios::binary);
			if (!input)
			{
				return Failure(StorageError::AccessDenied, L"No se pudo abrir el archivo para lectura.");
			}

			content.resize(static_cast<std::size_t>(fileSize));
			if (fileSize != 0)
			{
				input.read(content.data(), static_cast<std::streamsize>(fileSize));
				if (!input || input.gcount() != static_cast<std::streamsize>(fileSize))
				{
					content.clear();
					return Failure(StorageError::IoFailure, L"No se pudo leer el contenido completo del archivo.");
				}
			}
			return {};
		}

		struct FileProbe
		{
			bool exists = false;
			bool valid = false;
			std::filesystem::file_time_type modifiedTime{};
			StorageResult result{StorageError::FileNotFound};
			std::string content;
			domain::DiagramModel diagram;
		};

		[[nodiscard]] FileProbe ProbeFile(const std::filesystem::path& path)
		{
			FileProbe probe;
			std::error_code existsError;
			probe.exists = std::filesystem::exists(path, existsError);
			if (existsError)
			{
				probe.result = Failure(StorageError::IoFailure, L"No se pudo comprobar la existencia del archivo.");
				return probe;
			}
			if (!probe.exists)
			{
				return probe;
			}

			std::error_code sizeError;
			const std::uintmax_t fileSize = std::filesystem::file_size(path, sizeError);
			if (sizeError)
			{
				probe.result = Failure(StorageError::IoFailure, L"No se pudo determinar el tamano del archivo.");
				return probe;
			}
			if (fileSize > DiagramStorageLimits::MaximumFileBytes)
			{
				probe.result = Failure(
					StorageError::FileTooLarge,
					L"Tamano detectado: " + std::to_wstring(fileSize) + L" bytes.");
				return probe;
			}

			probe.result = ReadFile(path, fileSize, probe.content);
			if (!probe.result)
			{
				return probe;
			}
			probe.result = Serializer().Deserialize(probe.content, probe.diagram);
			if (!probe.result)
			{
				return probe;
			}

			std::error_code timeError;
			probe.modifiedTime = std::filesystem::last_write_time(path, timeError);
			if (timeError)
			{
				probe.result = Failure(
					StorageError::IoFailure,
					L"No se pudo determinar la fecha de modificacion del archivo.");
				return probe;
			}

			probe.valid = true;
			return probe;
		}

		[[nodiscard]] bool IsUnsafeInspectionFailure(StorageError error) noexcept
		{
			return error == StorageError::AccessDenied || error == StorageError::IoFailure;
		}

		void ConsiderCandidate(
			const FileProbe& primary,
			const FileProbe& probe,
			RecoverySource source,
			const std::filesystem::path& path,
			RecoveryCandidate& selected,
			std::filesystem::file_time_type& selectedTime)
		{
			if (!probe.valid)
			{
				return;
			}

			RecoveryReason reason = RecoveryReason::None;
			if (!primary.exists)
			{
				reason = RecoveryReason::PrimaryMissing;
			}
			else if (!primary.valid)
			{
				reason = RecoveryReason::PrimaryInvalid;
			}
			else if (probe.modifiedTime > primary.modifiedTime)
			{
				reason = RecoveryReason::CandidateNewer;
			}

			if (reason == RecoveryReason::None ||
				(selected.available && probe.modifiedTime <= selectedTime))
			{
				return;
			}

			selected = RecoveryCandidate{true, source, reason, path};
			selectedTime = probe.modifiedTime;
		}
	}

	TextDiagramStorage::TextDiagramStorage() noexcept
		: m_writer([]() -> const application::IAtomicFileWriter*
			{
				static const WindowsAtomicFileWriter writer;
				return &writer;
			}())
	{
	}

	TextDiagramStorage::TextDiagramStorage(const application::IAtomicFileWriter& writer) noexcept
		: m_writer(&writer)
	{
	}

	application::StorageResult TextDiagramStorage::Load(
		const std::filesystem::path& path,
		domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			if (path.empty())
			{
				return Failure(StorageError::InvalidPath);
			}
			if (!HasDiagramFileExtension(path))
			{
				return Failure(StorageError::UnsupportedFileExtension, L"Se esperaba un archivo con extension .atd.");
			}

			FileProbe probe = ProbeFile(path);
			if (!probe.valid)
			{
				return probe.result;
			}
			diagram = std::move(probe.diagram);
			return {};
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para cargar el documento.");
		}
		catch (...)
		{
			return Failure(StorageError::IoFailure, L"Se produjo un error inesperado al cargar el documento.");
		}
	}

	application::StorageResult TextDiagramStorage::Save(
		const std::filesystem::path& path,
		const domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			if (path.empty() || path.filename().empty())
			{
				return Failure(StorageError::InvalidPath);
			}
			if (!HasDiagramFileExtension(path))
			{
				return Failure(StorageError::UnsupportedFileExtension, L"Se esperaba un archivo con extension .atd.");
			}

			std::string content;
			const StorageResult serialization = Serializer().Serialize(diagram, content);
			if (!serialization)
			{
				return serialization;
			}

			const FileProbe current = ProbeFile(path);
			if (current.exists && current.valid)
			{
				const StorageResult backup = MapAtomicWriteFailure(
					m_writer->Write(PreviousVersionPathFor(path), current.content));
				if (!backup)
				{
					StorageResult result = backup;
					result.detail = L"No se pudo conservar la ultima version valida. " + result.detail;
					return result;
				}
			}
			else if (current.exists && IsUnsafeInspectionFailure(current.result.error))
			{
				return current.result;
			}
			return MapAtomicWriteFailure(m_writer->Write(path, content));
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para guardar el documento.");
		}
		catch (...)
		{
			return Failure(StorageError::IoFailure, L"Se produjo un error inesperado al guardar el documento.");
		}
	}

	application::RecoveryInspection TextDiagramStorage::InspectRecovery(
		const std::filesystem::path& path) const noexcept
	{
		application::RecoveryInspection inspection;
		try
		{
			if (path.empty() || path.filename().empty())
			{
				inspection.result = Failure(StorageError::InvalidPath);
				return inspection;
			}
			if (!HasDiagramFileExtension(path))
			{
				inspection.result = Failure(
					StorageError::UnsupportedFileExtension,
					L"Se esperaba un archivo con extension .atd.");
				return inspection;
			}

			const FileProbe primary = ProbeFile(path);
			inspection.primaryError = primary.valid ? StorageError::None : primary.result.error;

			const std::filesystem::path previousPath = PreviousVersionPathFor(path);
			const std::filesystem::path interruptedPath = InterruptedSavePathFor(path);
			const FileProbe previous = ProbeFile(previousPath);
			const FileProbe interrupted = ProbeFile(interruptedPath);
			inspection.invalidPreviousVersionDetected = previous.exists && !previous.valid;
			inspection.invalidInterruptedSaveDetected = interrupted.exists && !interrupted.valid;

			std::filesystem::file_time_type selectedTime{};
			ConsiderCandidate(
				primary,
				previous,
				RecoverySource::PreviousVersion,
				previousPath,
				inspection.candidate,
				selectedTime);
			ConsiderCandidate(
				primary,
				interrupted,
				RecoverySource::InterruptedSave,
				interruptedPath,
				inspection.candidate,
				selectedTime);
			return inspection;
		}
		catch (const std::bad_alloc&)
		{
			inspection.result = Failure(
				StorageError::IoFailure,
				L"No hay memoria suficiente para inspeccionar la recuperacion.");
			return inspection;
		}
		catch (...)
		{
			inspection.result = Failure(
				StorageError::IoFailure,
				L"Se produjo un error inesperado al inspeccionar la recuperacion.");
			return inspection;
		}
	}

	application::StorageResult TextDiagramStorage::Recover(
		const std::filesystem::path& destination,
		const application::RecoveryCandidate& candidate,
		domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			if (destination.empty() || destination.filename().empty() ||
				!HasDiagramFileExtension(destination) || !candidate.available)
			{
				return Failure(StorageError::InvalidPath, L"El destino o candidato de recuperacion no es valido.");
			}

			const std::filesystem::path expectedPath =
				candidate.source == RecoverySource::PreviousVersion
					? PreviousVersionPathFor(destination)
					: candidate.source == RecoverySource::InterruptedSave
						? InterruptedSavePathFor(destination)
						: std::filesystem::path{};
			if (expectedPath.empty() || candidate.path != expectedPath)
			{
				return Failure(StorageError::InvalidPath, L"El candidato no pertenece al documento solicitado.");
			}

			FileProbe recovered = ProbeFile(candidate.path);
			if (!recovered.valid)
			{
				StorageResult result = recovered.result;
				result.detail = L"La copia de recuperacion dejo de ser valida. " + result.detail;
				return result;
			}

			if (candidate.source == RecoverySource::InterruptedSave)
			{
				const StorageResult durableCopy = MapAtomicWriteFailure(
					m_writer->Write(PreviousVersionPathFor(destination), recovered.content));
				if (!durableCopy)
				{
					StorageResult result = durableCopy;
					result.detail = L"No se pudo proteger el guardado interrumpido antes de recuperarlo. " + result.detail;
					return result;
				}
			}

			const StorageResult replacement = MapAtomicWriteFailure(
				m_writer->Write(destination, recovered.content));
			if (!replacement)
			{
				return replacement;
			}

			if (candidate.source == RecoverySource::InterruptedSave)
			{
				std::error_code cleanupError;
				std::filesystem::remove(candidate.path, cleanupError);
			}
			diagram = std::move(recovered.diagram);
			return {};
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para recuperar el documento.");
		}
		catch (...)
		{
			return Failure(StorageError::IoFailure, L"Se produjo un error inesperado al recuperar el documento.");
		}
	}

	std::filesystem::path TextDiagramStorage::PreviousVersionPathFor(
		const std::filesystem::path& destination)
	{
		return destination.parent_path() /
			(destination.stem().wstring() + L".previous" + destination.extension().wstring());
	}

	std::filesystem::path TextDiagramStorage::InterruptedSavePathFor(
		const std::filesystem::path& destination)
	{
		return WindowsAtomicFileWriter::TemporaryPathFor(destination);
	}
}
