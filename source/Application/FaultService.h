#pragma once

#include <atomic>
#include <string>

namespace arteststudio::application
{
	enum class FaultSeverity
	{
		Information,
		Warning,
		Error,
		Critical
	};

	enum class FaultCategory
	{
		Application,
		Storage,
		UserInterface,
		Unexpected
	};

	struct Fault
	{
		FaultSeverity severity = FaultSeverity::Error;
		FaultCategory category = FaultCategory::Unexpected;
		std::wstring code;
		std::wstring operation;
		std::wstring message;
		std::wstring technicalDetail;
	};

	class IFaultReporter
	{
	public:
		virtual ~IFaultReporter() = default;
		virtual void Report(const Fault& fault) = 0;
	};

	class FaultService final
	{
	public:
		FaultService() = delete;

		static void Configure(IFaultReporter* reporter) noexcept;
		static void Report(const Fault& fault) noexcept;

	private:
		static std::atomic<IFaultReporter*> s_reporter;
	};
}
