#pragma once
#include <iostream>

using namespace std;

class PowerSupply
{
private:
	string m_psName;
	float m_fVoltage;

public:
	void TurnOn(int channel) {/* Power On method */}
	void TurnOff(int channel) {/* Power off method */}
	void ChangeVoltage(float fVoltage) { m_fVoltage = fVoltage; }
};

class RelayCard
{
private:
	string m_rcName;

public:
	void SetStates() {}
	void GetStates() {}
	void SetStateList() {}
};

class DigitalMultimeter
{
private:
	string m_dmmName;

public:

};

class Timer
{

};