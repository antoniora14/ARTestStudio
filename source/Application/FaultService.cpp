#include "FaultService.h"

namespace arteststudio::application
{
	std::atomic<IFaultReporter*> FaultService::s_reporter{nullptr};

	void FaultService::Configure(IFaultReporter* reporter) noexcept
	{
		s_reporter.store(reporter, std::memory_order_release);
	}

	void FaultService::Report(const Fault& fault) noexcept
	{
		IFaultReporter* reporter = s_reporter.load(std::memory_order_acquire);
		if (reporter == nullptr)
		{
			return;
		}

		try
		{
			reporter->Report(fault);
		}
		catch (...)
		{
			// Fault reporting is a last-resort boundary and must never fail the caller.
		}
	}
}
