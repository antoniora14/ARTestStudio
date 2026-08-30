#include "TextDiagramStorage.h"
#include "DiagramSerializationSupport.h"
#include "VersionedDiagramSerializer.h"
#include "WindowsAtomicFileWriter.h"

#include <cwchar>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace arteststudio::infrastructure
{
	namespace
	{
		using application::AtomicWriteError;
		using application::AtomicWriteResult;
		using application::DiagramStorageLimits;
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

			std::error_code existsError;
			const bool exists = std::filesystem::exists(path, existsError);
			if (existsError)
			{
				return Failure(StorageError::IoFailure, L"No se pudo comprobar la existencia del archivo.");
			}
			if (!exists)
			{
				return Failure(StorageError::FileNotFound);
			}

			std::error_code sizeError;
			const std::uintmax_t fileSize = std::filesystem::file_size(path, sizeError);
			if (sizeError)
			{
				return Failure(StorageError::IoFailure, L"No se pudo determinar el tamano del archivo.");
			}
			if (fileSize > DiagramStorageLimits::MaximumFileBytes)
			{
				return Failure(StorageError::FileTooLarge, L"Tamano detectado: " + std::to_wstring(fileSize) + L" bytes.");
			}

			std::string content;
			const StorageResult read = ReadFile(path, fileSize, content);
			if (!read)
			{
				return read;
			}
			return Serializer().Deserialize(content, diagram);
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
}
