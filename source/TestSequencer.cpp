#include "pch.h"
#include "TestSequencer.h"


void TestSequencer::Load(vector<unique_ptr<ITestCommand>>&& TstCmds)
{
	m_TestCommands = move(TstCmds);
	m_currentStep = 0;
	m_running = false;
	m_paused = false;
}
