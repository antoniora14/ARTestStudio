#pragma once

#include "../Application/AtomicFileWriter.h"

namespace arteststudio::infrastructure
{
	class WindowsAtomicFileWriter final : public application::IAtomicFileWriter
	{
	public:
		[[nodiscard]] application::AtomicWriteResult Write(
			const std::filesystem::path& destination,
			std::string_view content) const noexcept override;

		[[nodiscard]] static std::filesystem::path TemporaryPathFor(
			const std::filesystem::path& destination);
	};
}
