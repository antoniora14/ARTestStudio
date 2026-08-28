#include "../TestSupport/TestSupport.h"

#include <gtest/gtest.h>

namespace arteststudio::tests
{
	using application::FaultCategory;

	TEST(FaultServiceTests, RoutesFaultsThroughTheConfiguredReporter)
	{
		RecordingFaultReporter reporter;
		const FaultReporterScope scope{&reporter};
		FaultService::Report(Fault{
			FaultSeverity::Error,
			FaultCategory::Storage,
			L"TEST_STORAGE_FAILURE",
			L"Regression test",
			L"Expected test fault",
			L"No external side effect"});

		VerifyTestCondition(reporter.count == 1, "The configured reporter must receive each fault.");
		VerifyTestCondition(reporter.lastSeverity == FaultSeverity::Error, "Fault severity must be preserved.");
		VerifyTestCondition(reporter.lastCode == L"TEST_STORAGE_FAILURE", "Fault code must be preserved.");
		VerifyTestCondition(reporter.lastOperation == L"Regression test", "Fault operation must be preserved.");
	}

	TEST(FaultServiceTests, FaultReportingFailuresNeverEscape)
	{
		ThrowingFaultReporter reporter;
		const FaultReporterScope scope{&reporter};
		FaultService::Report(Fault{
			FaultSeverity::Critical,
			FaultCategory::Unexpected,
			L"TEST_THROWING_REPORTER",
			L"Fault boundary regression",
			L"The reporter intentionally throws",
			{}});
		VerifyTestCondition(true, "A reporter exception must not escape FaultService.");
	}


}
