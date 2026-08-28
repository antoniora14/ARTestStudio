#include "WindowsAtomicFileWriter.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <limits>

namespace arteststudio::infrastructure
{
	namespace
	{
		using application::AtomicWriteError;
		using application::AtomicWriteResult;

		[[nodiscard]] AtomicWriteResult Failure(AtomicWriteError error, DWORD systemError)
		{
			return {error, L"Windows reporto el codigo " + std::to_wstring(systemError) + L"."};
		}

		[[nodiscard]] bool IsAccessDenied(DWORD error) noexcept
		{
			return error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ||
				error == ERROR_LOCK_VIOLATION;
		}

		void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
		{
			(void)DeleteFileW(path.c_str());
		}
	}

	std::filesystem::path WindowsAtomicFileWriter::TemporaryPathFor(
		const std::filesystem::path& destination)
	{
		std::filesystem::path temporary = destination;
		temporary += L".tmp";
		return temporary;
	}

	application::AtomicWriteResult WindowsAtomicFileWriter::Write(
		const std::filesystem::path& destination,
		std::string_view content) const noexcept
	{
		if (destination.empty() || destination.filename().empty())
		{
			return {AtomicWriteError::InvalidPath, L"La ruta de destino no es valida."};
		}

		try
		{
			const std::filesystem::path temporary = TemporaryPathFor(destination);
			if (!DeleteFileW(temporary.c_str()))
			{
				const DWORD cleanupError = GetLastError();
				if (cleanupError != ERROR_FILE_NOT_FOUND && cleanupError != ERROR_PATH_NOT_FOUND)
				{
					return Failure(AtomicWriteError::TemporaryCleanupFailure, cleanupError);
				}
			}

			HANDLE file = CreateFileW(
				temporary.c_str(),
				GENERIC_WRITE,
				0,
				nullptr,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
				nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				const DWORD error = GetLastError();
				return Failure(
					IsAccessDenied(error) ? AtomicWriteError::AccessDenied : AtomicWriteError::TemporaryWriteFailure,
					error);
			}

			std::size_t written = 0;
			while (written < content.size())
			{
				const std::size_t remaining = content.size() - written;
				const DWORD requested = static_cast<DWORD>((std::min)(
					remaining,
					static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
				DWORD blockWritten = 0;
				if (!WriteFile(file, content.data() + written, requested, &blockWritten, nullptr) ||
					blockWritten != requested)
				{
					const DWORD error = GetLastError();
					CloseHandle(file);
					RemoveTemporaryFile(temporary);
					return Failure(AtomicWriteError::TemporaryWriteFailure, error);
				}
				written += blockWritten;
			}

			if (!FlushFileBuffers(file))
			{
				const DWORD error = GetLastError();
				CloseHandle(file);
				RemoveTemporaryFile(temporary);
				return Failure(AtomicWriteError::TemporaryWriteFailure, error);
			}
			if (!CloseHandle(file))
			{
				const DWORD error = GetLastError();
				RemoveTemporaryFile(temporary);
				return Failure(AtomicWriteError::TemporaryWriteFailure, error);
			}

			if (!MoveFileExW(
				temporary.c_str(),
				destination.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				const DWORD error = GetLastError();
				RemoveTemporaryFile(temporary);
				return Failure(AtomicWriteError::ReplacementFailure, error);
			}

			return {};
		}
		catch (const std::bad_alloc&)
		{
			return {AtomicWriteError::TemporaryWriteFailure, L"No hay memoria suficiente para completar la escritura."};
		}
		catch (...)
		{
			return {AtomicWriteError::TemporaryWriteFailure, L"Ocurrio un error inesperado durante la escritura atomica."};
		}
	}
}
