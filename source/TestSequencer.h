#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include "TestCommands.h"

using namespace std;

struct DebugCommand
{
	unique_ptr<ITestCommand> tstcmd;
	bool breakpoint = false;

	DebugCommand(unique_ptr<ITestCommand> dbgCmd, bool bp = false) : tstcmd(move(dbgCmd)), breakpoint(bp) {}
};

class TestSequencer
{
private:
	vector<unique_ptr<ITestCommand>> m_TestCommands;
	size_t m_currentStep = 0;
	atomic<bool> m_running = false;
	atomic<bool> m_paused = false;

public:
	void Load(vector<unique_ptr<ITestCommand>>&& TstCmds);
	void Step();
	void Run();
	void Stop();
	bool IsFinished()const;
};
