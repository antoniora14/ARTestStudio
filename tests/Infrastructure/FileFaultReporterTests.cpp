#include "../TestSupport/TestSupport.h"

#include "Application/FaultService.h"
#include "Infrastructure/FileFaultReporter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace arteststudio::tests
{
	using application::FaultCategory;
	using infrastructure::FileFaultReporter;

	TEST(FileFaultReporterTests, WritesStructuredFaultLogs)
	{
		TemporaryDiagramFile file{".log"};
		FileFaultReporter reporter{file.Path()};
		reporter.Report(Fault{
			FaultSeverity::Warning,
			FaultCategory::Storage,
			L"TEST_LOG_ENTRY",
			L"Write regression log",
			L"The logger must write UTF-8",
			L"Technical detail 42"});

		std::ifstream input(file.Path(), std::ios::binary);
		const std::string contents{
			std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
		VerifyTestCondition(input.good() || input.eof(), "The generated fault log must be readable.");
		VerifyTestCondition(contents.find("[WARNING]") != std::string::npos, "The log must contain severity.");
		VerifyTestCondition(contents.find("[STORAGE]") != std::string::npos, "The log must contain category.");
		VerifyTestCondition(contents.find("TEST_LOG_ENTRY") != std::string::npos, "The log must contain the stable code.");
		VerifyTestCondition(contents.find("Technical detail 42") != std::string::npos,
			"The log must contain the technical detail.");
	}

	TEST(FileFaultReporterTests, RotatesOversizedFaultLogs)
	{
		TemporaryDiagramFile file{".log"};
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			const std::string block(1024, 'x');
			for (int index = 0; index < 2048; ++index)
			{
				output.write(block.data(), static_cast<std::streamsize>(block.size()));
			}
		}

		FileFaultReporter reporter{file.Path()};
		reporter.Report(Fault{
			FaultSeverity::Error,
			FaultCategory::Application,
			L"TEST_ROTATED_ENTRY",
			L"Rotate regression log",
			L"This entry belongs to the new log",
			{}});

		const std::filesystem::path previousPath = file.Path().parent_path() /
			(file.Path().stem().wstring() + L".previous" + file.Path().extension().wstring());
		VerifyTestCondition(std::filesystem::exists(previousPath), "An oversized log must be preserved as previous.log.");
		VerifyTestCondition(std::filesystem::file_size(previousPath) >= 2 * 1024 * 1024,
			"The rotated file must contain the previous log contents.");

		std::ifstream input(file.Path(), std::ios::binary);
		const std::string contents{
			std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
		VerifyTestCondition(contents.find("TEST_ROTATED_ENTRY") != std::string::npos,
			"The fault that triggered rotation must be written to the new log.");
	}
}
