#pragma once

#include <cstdint>
#include <string>

namespace arteststudio::application
{
	struct PendingRecoverySession
	{
		std::wstring restartIdentifier;
		std::uint32_t processId = 0;
	};

	enum class PendingRecoveryDisposition
	{
		None,
		Invalid,
		OwnedByCurrentProcess,
		PreviousProcessStillRunning,
		RecoverPreviousSession
	};

	[[nodiscard]] PendingRecoveryDisposition EvaluatePendingRecoverySession(
		const PendingRecoverySession& session,
		std::uint32_t currentProcessId,
		bool recordedProcessIsRunning) noexcept;
}
