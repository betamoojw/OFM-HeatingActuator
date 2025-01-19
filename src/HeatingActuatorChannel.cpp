#include "HeatingActuatorChannel.h"
#include "HeatingActuatorModule.h"

HeatingActuatorChannel::HeatingActuatorChannel(uint8_t iChannelNumber)
{
    _channelIndex = iChannelNumber;
}

HeatingActuatorChannel::~HeatingActuatorChannel() {}

void HeatingActuatorChannel::processInputKo(GroupObject &iKo)
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

void HeatingActuatorChannel::loop()
{
    
}

void HeatingActuatorChannel::setup(bool configured)
{
    logDebugP("Setup channel %u", _channelIndex);

    // preset PIN state before changing PIN mode
    digitalWriteFast(MOTOR_PINS[_channelIndex], MOT_OFF);

    pinMode(MOTOR_PINS[_channelIndex], OUTPUT);

    // set it again the standard way, just in case
    stopMotor();

    // if (configured)
    // {
    //     if (ParamHTA_ChStatusCyclicTimeMS > 0)
    //         statusCyclicSendTimer = delayTimerInit();
    // }
}

void HeatingActuatorChannel::savePower()
{
    // if (ParamHTA_ChBehaviorPowerLoss > 0)
    //     doSwitchInternal(ParamHTA_ChBehaviorPowerLoss == 2);
}

bool HeatingActuatorChannel::restorePower()
{
    // if (ParamHTA_ChBehaviorPowerRegain > 0)
    //     doSwitchInternal(ParamHTA_ChBehaviorPowerRegain == 2);

    return true;
}

const std::string HeatingActuatorChannel::name()
{
    return "HeatingChannel";
}