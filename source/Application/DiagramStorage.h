#pragma once

#include "../Domain/DiagramModel.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace arteststudio::application
{
	inline constexpr std::wstring_view kDiagramFileExtension = L".atd";
	inline constexpr std::wstring_view kProjectFileExtension = L".atprj";

	struct DiagramStorageLimits
	{
		static constexpr std::uintmax_t MaximumFileBytes = 16ULL * 1024ULL * 1024ULL;
		static constexpr std::size_t MaximumLabelBytes = 64ULL * 1024ULL;
	};

	enum class StorageError
	{
		None,
		InvalidPath,
		UnsupportedFileExtension,
		FileNotFound,
		AccessDenied,
		IoFailure,
		InvalidFormat,
		UnsupportedVersion,
		InvalidData,
		InvalidEncoding,
		FileTooLarge,
		DataLimitExceeded,
		TemporaryFileFailure,
		ReplacementFailure
	};

	struct StorageResult
	{
		StorageError error = StorageError::None;
		std::wstring detail;

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return error == StorageError::None;
		}
	};

	[[nodiscard]] constexpr std::wstring_view DescribeStorageError(StorageError error) noexcept
	{
		switch (error)
		{
		case StorageError::None:
			return L"The operation completed successfully.";
		case StorageError::InvalidPath:
			return L"The document path is invalid.";
		case StorageError::UnsupportedFileExtension:
			return L"The file extension is not valid for an ARTestStudio diagram (.atd).";
		case StorageError::FileNotFound:
			return L"The requested file was not found.";
		case StorageError::AccessDenied:
			return L"Insufficient permissions to access the file.";
		case StorageError::IoFailure:
			return L"An input/output error occurred.";
		case StorageError::InvalidFormat:
			return L"The file does not contain a valid ARTestStudio format.";
		case StorageError::UnsupportedVersion:
			return L"The file version is not supported by this application.";
		case StorageError::InvalidData:
			return L"The file contains invalid diagram data.";
		case StorageError::InvalidEncoding:
			return L"The file contains invalid UTF-8 text.";
		case StorageError::FileTooLarge:
			return L"The file exceeds the 16 MB size limit.";
		case StorageError::DataLimitExceeded:
			return L"The diagram exceeds one or more security limits.";
		case StorageError::TemporaryFileFailure:
			return L"The temporary save file could not be prepared or written.";
		case StorageError::ReplacementFailure:
			return L"The previous document could not be replaced; its contents were preserved.";
		}

		return L"An unknown error occurred.";
	}

	class IDiagramStorage
	{
	public:
		virtual ~IDiagramStorage() = default;

		[[nodiscard]] virtual StorageResult Load(
			const std::filesystem::path& path,
			domain::DiagramModel& diagram) const noexcept = 0;

		[[nodiscard]] virtual StorageResult Save(
			const std::filesystem::path& path,
			const domain::DiagramModel& diagram) const noexcept = 0;
	};
}
