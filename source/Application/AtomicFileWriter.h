#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace arteststudio::application
{
	enum class AtomicWriteError
	{
		None,
		InvalidPath,
		AccessDenied,
		TemporaryCleanupFailure,
		TemporaryWriteFailure,
		ReplacementFailure
	};

	struct AtomicWriteResult
	{
		AtomicWriteError error = AtomicWriteError::None;
		std::wstring detail;

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return error == AtomicWriteError::None;
		}
	};

	class IAtomicFileWriter
	{
	public:
		virtual ~IAtomicFileWriter() = default;

		[[nodiscard]] virtual AtomicWriteResult Write(
			const std::filesystem::path& destination,
			std::string_view content) const noexcept = 0;
	};
}
