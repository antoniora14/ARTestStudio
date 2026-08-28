#pragma once

#include "Application/AtomicFileWriter.h"
#include "Application/FaultService.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace arteststudio::tests
{
	using application::AtomicWriteError;
	using application::AtomicWriteResult;
	using application::Fault;
	using application::FaultService;
	using application::FaultSeverity;
	using application::IAtomicFileWriter;
	using application::IFaultReporter;

	class TestAssertionFailure final : public std::runtime_error
	{
	public:
		explicit TestAssertionFailure(const char* message)
			: std::runtime_error(message)
		{
		}
	};

	inline void VerifyTestCondition(bool condition, const char* message)
	{
		if (!condition)
		{
			throw TestAssertionFailure{message};
		}
	}

	class TemporaryDiagramFile final
	{
	public:
		explicit TemporaryDiagramFile(std::string_view extension = ".atd")
		{
			static std::atomic_uint64_t sequence{0};
			const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
			const auto suffix = std::to_string(timestamp) + "." + std::to_string(sequence.fetch_add(1));
			m_path = std::filesystem::temp_directory_path() /
				("ARTestStudio.UnitTests." + suffix + std::string{extension});
		}

		~TemporaryDiagramFile()
		{
			std::error_code error;
			std::filesystem::remove(m_path, error);
			std::filesystem::path temporaryPath = m_path;
			temporaryPath += L".tmp";
			std::filesystem::remove(temporaryPath, error);
			const std::filesystem::path previousPath = m_path.parent_path() /
				(m_path.stem().wstring() + L".previous" + m_path.extension().wstring());
			std::filesystem::remove(previousPath, error);
		}

		[[nodiscard]] const std::filesystem::path& Path() const noexcept
		{
			return m_path;
		}

	private:
		std::filesystem::path m_path;
	};

	class RecordingFaultReporter final : public IFaultReporter
	{
	public:
		void Report(const Fault& fault) override
		{
			++count;
			lastSeverity = fault.severity;
			lastCode = fault.code;
			lastOperation = fault.operation;
		}

		int count = 0;
		FaultSeverity lastSeverity = FaultSeverity::Information;
		std::wstring lastCode;
		std::wstring lastOperation;
	};

	class FailingAtomicFileWriter final : public IAtomicFileWriter
	{
	public:
		explicit FailingAtomicFileWriter(AtomicWriteError error)
			: m_error(error)
		{
		}

		[[nodiscard]] AtomicWriteResult Write(
			const std::filesystem::path& destination,
			std::string_view content) const noexcept override
		{
			++calls;
			lastDestination = destination;
			lastContentSize = content.size();
			return {m_error, L"Fallo simulado por la prueba."};
		}

		mutable int calls = 0;
		mutable std::filesystem::path lastDestination;
		mutable std::size_t lastContentSize = 0;

	private:
		AtomicWriteError m_error;
	};

	[[nodiscard]] inline std::string ReadFileContents(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		return {
			std::istreambuf_iterator<char>{input},
			std::istreambuf_iterator<char>{}};
	}

	class ThrowingFaultReporter final : public IFaultReporter
	{
	public:
		void Report(const Fault&) override
		{
			throw std::runtime_error("simulated reporter failure");
		}
	};

	class FaultReporterScope final
	{
	public:
		explicit FaultReporterScope(IFaultReporter* reporter) noexcept
		{
			FaultService::Configure(reporter);
		}

		~FaultReporterScope()
		{
			FaultService::Configure(nullptr);
		}

		FaultReporterScope(const FaultReporterScope&) = delete;
		FaultReporterScope& operator=(const FaultReporterScope&) = delete;
	};
}
