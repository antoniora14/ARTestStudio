#include "UnexpectedCloseRecovery.h"

#include <cwctype>

namespace arteststudio::application
{
	namespace
	{
		[[nodiscard]] bool IsValidRestartIdentifier(const std::wstring& identifier) noexcept
		{
			if (identifier.size() != 36)
			{
				return false;
			}

			for (std::size_t index = 0; index < identifier.size(); ++index)
			{
				const bool separator = index == 8 || index == 13 || index == 18 || index == 23;
				if (separator)
				{
					if (identifier[index] != L'-')
					{
						return false;
					}
				}
				else if (std::iswxdigit(identifier[index]) == 0)
				{
					return false;
				}
			}

			return true;
		}
	}

	PendingRecoveryDisposition EvaluatePendingRecoverySession(
		const PendingRecoverySession& session,
		std::uint32_t currentProcessId,
		bool recordedProcessIsRunning) noexcept
	{
		if (session.restartIdentifier.empty() && session.processId == 0)
		{
			return PendingRecoveryDisposition::None;
		}

		if (!IsValidRestartIdentifier(session.restartIdentifier) || session.processId == 0)
		{
			return PendingRecoveryDisposition::Invalid;
		}

		if (session.processId == currentProcessId)
		{
			return PendingRecoveryDisposition::OwnedByCurrentProcess;
		}

		if (recordedProcessIsRunning)
		{
			return PendingRecoveryDisposition::PreviousProcessStillRunning;
		}

		return PendingRecoveryDisposition::RecoverPreviousSession;
	}
}
