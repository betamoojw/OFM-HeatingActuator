#include "HeatingActuatorChannel.h"
#include "HeatingActuatorModule.h"

HeatingActuatorChannel::HeatingActuatorChannel(uint8_t iChannelNumber)
{
    _channelIndex = iChannelNumber;
}

HeatingActuatorChannel::~HeatingActuatorChannel() {}

void HeatingActuatorChannel::processInputKo(GroupObject &iKo)
{
    if (ParamSWA_ChActive != 1)
    {
        logTraceP("processInputKo: channel not active (%u)", ParamSWA_ChActive);
        return;
    }

    logDebugP("processInputKo: channel %u", _channelIndex);
    logIndentUp();

    bool newActive;
    switch (iKo.asap())
    {
        case SWA_KoCentralFunction:
            if (ParamSWA_ChCentralFunction)
            {
                newActive = iKo.value(DPT_Switch);
                logDebugP("SWA_KoCentralFunction: %u", newActive);

                processSwitchInput(newActive);
            }
            break;
    }

    switch (SWA_KoCalcIndex(iKo.asap()))
    {
        case SWA_KoChSwitch:
            newActive = iKo.value(DPT_Switch);
            logDebugP("SWA_KocSwitch: %u", newActive);

            processSwitchInput(newActive);
            break;
        case SWA_KoChLock:
            newActive = iKo.value(DPT_Switch);
            logDebugP("SWA_KocLock: %u", newActive);

            if (newActive == (bool)KoSWA_ChLockStatus.value(DPT_Switch))
            {
                logDebugP("New value equals current status");
                break;
            }

            KoSWA_ChLockStatus.value(newActive, DPT_Switch);

            if (newActive)
            {
                statusDuringLock = (bool)KoSWA_ChStatus.value(DPT_Switch);

                if (ParamSWA_ChBehaviorLock > 0)
                    doSwitch(ParamSWA_ChBehaviorLock == 2);
            }
            else
            {
                switch (ParamSWA_ChBehaviorUnlock)
                {
                    case 1:
                        doSwitch(false);
                        break;
                    case 2:
                        doSwitch(true);
                        break;
                    case 3:
                    case 4:
                        doSwitch(statusDuringLock);
                        break;
                }
            }

            break;
    }

    logIndentDown();
}

void HeatingActuatorChannel::processSwitchInput(bool newActive)
{
    
}

void HeatingActuatorChannel::runMotor()
{
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_ON);
}

void HeatingActuatorChannel::stopMotor()
{
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_OFF);
}

void HeatingActuatorChannel::doSwitch(bool active, bool syncSwitch)
{
    
}

void HeatingActuatorChannel::doSwitchInternal(bool active, bool syncSwitch)
{
    
}

void HeatingActuatorChannel::loop()
{
    
}

void HeatingActuatorChannel::setup(bool configured)
{
    logDebugP("Setup channel %u", _channelIndex);

    // preset PIN state before changing PIN mode
    digitalWriteFast(MOTOR_PINS[_channelIndex], MOT_OFF);

    pinMode(MOTOR_PINS[_channelIndex], OUTPUT);

    // // set it again the standard way, just in case
    // relaisOff();

    // if (configured)
    // {
    //     if (ParamSWA_ChStatusCyclicTimeMS > 0)
    //         statusCyclicSendTimer = delayTimerInit();
    // }
}

void HeatingActuatorChannel::relaisOff()
{
    logDebugP("Write motor state off to GPIO %u with value %u", MOTOR_PINS[_channelIndex], MOT_OFF);
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_OFF);
}

void HeatingActuatorChannel::savePower()
{
    if (ParamSWA_ChBehaviorPowerLoss > 0)
        doSwitchInternal(ParamSWA_ChBehaviorPowerLoss == 2);
}

bool HeatingActuatorChannel::restorePower()
{
    if (ParamSWA_ChBehaviorPowerRegain > 0)
        doSwitchInternal(ParamSWA_ChBehaviorPowerRegain == 2);

    return true;
}

const std::string HeatingActuatorChannel::name()
{
    return "HeatingChannel";
}