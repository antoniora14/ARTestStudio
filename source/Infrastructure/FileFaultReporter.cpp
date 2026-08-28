#include "FileFaultReporter.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace arteststudio::infrastructure
{
	namespace
	{
		constexpr std::uintmax_t kMaximumLogSize = 2 * 1024 * 1024;

		[[nodiscard]] std::wstring_view SeverityName(application::FaultSeverity severity) noexcept
		{
			switch (severity)
			{
			case application::FaultSeverity::Information:
				return L"INFO";
			case application::FaultSeverity::Warning:
				return L"WARNING";
			case application::FaultSeverity::Error:
				return L"ERROR";
			case application::FaultSeverity::Critical:
				return L"CRITICAL";
			}
			return L"UNKNOWN";
		}

		[[nodiscard]] std::wstring_view CategoryName(application::FaultCategory category) noexcept
		{
			switch (category)
			{
			case application::FaultCategory::Application:
				return L"APPLICATION";
			case application::FaultCategory::Storage:
				return L"STORAGE";
			case application::FaultCategory::UserInterface:
				return L"UI";
			case application::FaultCategory::Unexpected:
				return L"UNEXPECTED";
			}
			return L"UNKNOWN";
		}

		[[nodiscard]] std::wstring CurrentTimestamp()
		{
			SYSTEMTIME time{};
			GetLocalTime(&time);
			std::wostringstream output;
			output << std::setfill(L'0')
				<< std::setw(4) << time.wYear << L'-'
				<< std::setw(2) << time.wMonth << L'-'
				<< std::setw(2) << time.wDay << L'T'
				<< std::setw(2) << time.wHour << L':'
				<< std::setw(2) << time.wMinute << L':'
				<< std::setw(2) << time.wSecond << L'.'
				<< std::setw(3) << time.wMilliseconds;
			return output.str();
		}

		[[nodiscard]] std::string ToUtf8(std::wstring_view value)
		{
			if (value.empty())
			{
				return {};
			}

			const int size = static_cast<int>(value.size());
			const int required = WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), size, nullptr, 0, nullptr, nullptr);
			if (required <= 0)
			{
				return "<invalid-unicode>";
			}

			std::string result(static_cast<std::size_t>(required), '\0');
			if (WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), size,
				result.data(), required, nullptr, nullptr) != required)
			{
				return "<invalid-unicode>";
			}
			return result;
		}

		[[nodiscard]] std::wstring BuildLine(const application::Fault& fault)
		{
			std::wstring line = L"[";
			line += CurrentTimestamp();
			line += L"] [";
			line += SeverityName(fault.severity);
			line += L"] [";
			line += CategoryName(fault.category);
			line += L"] [";
			line += fault.code.empty() ? L"UNSPECIFIED" : fault.code;
			line += L"] ";
			line += fault.operation;
			if (!fault.message.empty())
			{
				line += L" | ";
				line += fault.message;
			}
			if (!fault.technicalDetail.empty())
			{
				line += L" | Detail: ";
				line += fault.technicalDetail;
			}
			line += L"\r\n";
			return line;
		}
	}

	FileFaultReporter::FileFaultReporter(std::filesystem::path logPath)
		: m_logPath(std::move(logPath))
	{
	}

	void FileFaultReporter::Report(const application::Fault& fault) noexcept
	{
		try
		{
			const std::wstring line = BuildLine(fault);
			OutputDebugStringW(line.c_str());

			std::scoped_lock lock(m_mutex);
			const std::filesystem::path path = ResolveLogPath();
			if (path.empty())
			{
				return;
			}

			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
			if (error)
			{
				return;
			}

			const std::uintmax_t currentSize = std::filesystem::file_size(path, error);
			if (!error && currentSize >= kMaximumLogSize)
			{
				std::filesystem::path previousPath = path.parent_path() /
					(path.stem().wstring() + L".previous" + path.extension().wstring());
				std::filesystem::remove(previousPath, error);
				error.clear();
				std::filesystem::rename(path, previousPath, error);
			}

			const std::string utf8Line = ToUtf8(line);
			std::ofstream output(path, std::ios::binary | std::ios::app);
			if (!output)
			{
				return;
			}
			output.write(utf8Line.data(), static_cast<std::streamsize>(utf8Line.size()));
		}
		catch (...)
		{
			// Logging must not throw while another failure is already being handled.
		}
	}

	std::filesystem::path FileFaultReporter::ResolveLogPath() const
	{
		if (!m_logPath.empty())
		{
			return m_logPath;
		}

		std::wstring localAppData(32768, L'\0');
		const DWORD length = GetEnvironmentVariableW(
			L"LOCALAPPDATA", localAppData.data(), static_cast<DWORD>(localAppData.size()));
		if (length > 0 && length < localAppData.size())
		{
			localAppData.resize(length);
			return std::filesystem::path{localAppData} / L"ARTestStudio" / L"Logs" / L"ARTestStudio.log";
		}

		std::error_code error;
		const std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
		return error ? std::filesystem::path{} : temporary / L"ARTestStudio.log";
	}
}
