#pragma once

#include "../Application/FaultService.h"

#include <filesystem>
#include <mutex>

namespace arteststudio::infrastructure
{
	class FileFaultReporter final : public application::IFaultReporter
	{
	public:
		FileFaultReporter() noexcept = default;
		explicit FileFaultReporter(std::filesystem::path logPath);

		void Report(const application::Fault& fault) noexcept override;

	private:
		[[nodiscard]] std::filesystem::path ResolveLogPath() const;

		std::filesystem::path m_logPath;
		std::mutex m_mutex;
	};
}
