#include "pch.h"
#include "TestCommands.h"


#define CMD_PSTURNON_NAME	"PowerOn"
#define CMD_PSTURNOFF_NAME	"PowerOff"
#define CMD_WAIT_NAME		"Wait"



string TstCmdPowerOn::Name() const
{
	return CMD_PSTURNON_NAME;
}

bool TstCmdPowerOn::Validate(string& csError)
{
	return true;
}

void TstCmdPowerOn::Execute()
{
	m_pPS->TurnOn(m_nChannel);
}


string TstCmdPowerOff::Name() const
{
	return CMD_PSTURNOFF_NAME;
}

bool TstCmdPowerOff::Validate(string& csError)
{
	return true;
}

void TstCmdPowerOff::Execute()
{
	m_pPS->TurnOff(m_nChannel);
}


string TstCmdChangeVoltage::Name() const
{
	return CMD_PSTURNOFF_NAME;
}

bool TstCmdChangeVoltage::Validate(string& csError)
{
	return true;
}

void TstCmdChangeVoltage::Execute()
{
	m_pPS->ChangeVoltage(m_fVoltage);
}


string TstCmdWait::Name() const
{
	return CMD_WAIT_NAME;
}

bool TstCmdWait::Validate(string& csError)
{
	return true;
}

void TstCmdWait::Execute()
{
	this_thread::sleep_for(chrono::milliseconds(m_milliseconds));
}
