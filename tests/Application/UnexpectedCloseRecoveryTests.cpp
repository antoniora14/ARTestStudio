#include "../TestSupport/TestSupport.h"

#include "Application/UnexpectedCloseRecovery.h"

#include <gtest/gtest.h>

namespace arteststudio::tests
{
	using namespace application;

	namespace
	{
		constexpr wchar_t ValidRestartIdentifier[] =
			L"01234567-89AB-CDEF-0123-456789ABCDEF";
	}

	TEST(UnexpectedCloseRecoveryTests, IgnoresAnEmptySession)
	{
		VerifyTestCondition(
			EvaluatePendingRecoverySession({}, 100, false) == PendingRecoveryDisposition::None,
			"An empty persisted session must not start recovery.");
	}

	TEST(UnexpectedCloseRecoveryTests, RejectsAnInvalidCatalog)
	{
		const PendingRecoverySession session{L"not-a-restart-identifier", 42};
		VerifyTestCondition(
			EvaluatePendingRecoverySession(session, 100, false) == PendingRecoveryDisposition::Invalid,
			"A malformed restart identifier must never be used to read recovery state.");
	}

	TEST(UnexpectedCloseRecoveryTests, DoesNotRecoverTheCurrentProcess)
	{
		const PendingRecoverySession session{ValidRestartIdentifier, 100};
		VerifyTestCondition(
			EvaluatePendingRecoverySession(session, 100, true)
				== PendingRecoveryDisposition::OwnedByCurrentProcess,
			"The application must not treat its own live catalog as a previous crash.");
	}

	TEST(UnexpectedCloseRecoveryTests, DoesNotStealAutosaveFromAnotherLiveInstance)
	{
		const PendingRecoverySession session{ValidRestartIdentifier, 42};
		VerifyTestCondition(
			EvaluatePendingRecoverySession(session, 100, true)
				== PendingRecoveryDisposition::PreviousProcessStillRunning,
			"A second instance must not restore an autosave owned by a live process.");
	}

	TEST(UnexpectedCloseRecoveryTests, RecoversAValidSessionOwnedByADeadProcess)
	{
		const PendingRecoverySession session{ValidRestartIdentifier, 42};
		VerifyTestCondition(
			EvaluatePendingRecoverySession(session, 100, false)
				== PendingRecoveryDisposition::RecoverPreviousSession,
			"A valid catalog whose owner is no longer running must be recoverable.");
	}
}
