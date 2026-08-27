#include "TestReceptors.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace std;


// Test Command common interface
class ITestCommand
{
public:
    virtual bool Validate(string& csError) = 0;
    virtual void Execute() = 0;
    virtual string Name() const = 0;
    virtual ~ITestCommand() = default;
};


#pragma region Power Supply Commands
class TstCmdPowerOn : ITestCommand
{
private:
    PowerSupply*    m_pPS;
    int             m_nChannel;

public:
    TstCmdPowerOn(PowerSupply* powersupply, int channel) : m_pPS(powersupply), m_nChannel(channel) {}

    virtual string Name() const override;
    virtual bool Validate(string& csError) override;
    virtual void Execute() override;
};

class TstCmdPowerOff : ITestCommand
{
private:
    PowerSupply*    m_pPS;
    int             m_nChannel;

public:
    TstCmdPowerOff(PowerSupply* powersupply, int channel) : m_pPS(powersupply), m_nChannel(channel) {}

    virtual string Name() const override;
    virtual bool Validate(string& csError) override;
    virtual void Execute() override;
};

class TstCmdChangeVoltage : ITestCommand
{
private:
    PowerSupply* m_pPS;
    FLOAT m_fVoltage;

public:
    TstCmdChangeVoltage(PowerSupply* powersupply, FLOAT voltage) : m_pPS(powersupply), m_fVoltage(voltage) {}

    virtual string Name() const override;
    virtual bool Validate(string& csError) override;
    virtual void Execute() override;
};
#pragma endregion Power Supply Commands



#pragma region Switching Commands
class TstCmdSetStates
{};

class TstCmdGetStates
{};

class TstCmdSetStateList
{};
#pragma endregion Switching Commands



#pragma region Wait Time Commands
class TstCmdWait : ITestCommand
{
private:
    INT m_milliseconds;

public:
    TstCmdWait(int ms) : m_milliseconds(ms) {}

    virtual string Name() const override;
    virtual bool Validate(string& csError) override;
    virtual void Execute() override;
};
#pragma endregion Wait Time Commands
