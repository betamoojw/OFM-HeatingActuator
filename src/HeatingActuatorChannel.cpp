#include "HeatingActuatorChannel.h"
#include "HeatingActuatorModule.h"

HeatingActuatorChannel::HeatingActuatorChannel(uint8_t channelNumber)
{
    _channelIndex = channelNumber;
}

HeatingActuatorChannel::~HeatingActuatorChannel() {}

void HeatingActuatorChannel::processInputKo(GroupObject &ko)
{
}

void HeatingActuatorChannel::runMotor()
{
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_ON);
    _motorStarted = millis();
}

void HeatingActuatorChannel::stopMotor()
{
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_OFF);
    _motorStopped = millis();
}

void HeatingActuatorChannel::loop(bool motorPower, float current)
{
    if (motorPower)
    {
        // motor should be running, check if motor actually connected
        if (current < OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT)
        {
            if (calibrationState != CalibrationState::NONE &&
                calibrationState != CalibrationState::ERROR)
            {
                calibrationState = CalibrationState::ERROR;
                logDebugP("STOP: calibration failed, no motor connected (current: %.2f, min: %.2f)", current, OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT);
                openknxHeatingActuatorModule.stopMotor();
            }
        }
    }
    else
    {
        // motor has stopped, move to next state
        switch (calibrationState)
        {
            case CalibrationState::INIT:
                calibrationState = CalibrationState::CLOSING;
                openknxHeatingActuatorModule.runMotor(_channelIndex, false);
                break;
            case CalibrationState::CLOSING:
                calibratedDriveCloseTime = _motorStopped - _motorStarted;

                calibrationState = CalibrationState::OPENING;
                openknxHeatingActuatorModule.runMotor(_channelIndex, true);
                break;
            case CalibrationState::OPENING:
                calibratedDriveOpenTime = _motorStopped - _motorStarted;

                calibrationState = CalibrationState::COMPLETE;
                logDebugP("Calibration complete (calibratedDriveCloseTime: %u ms, calibratedDriveOpenTime: %u ms)", calibratedDriveCloseTime, calibratedDriveOpenTime);
                break;
        }
    }
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
    //         _statusCyclicSendTimer = delayTimerInit();
    // }
}

void HeatingActuatorChannel::startCalibration()
{
    logDebugP("Start calibration motor index %u", _channelIndex);
    calibrationState = CalibrationState::INIT;

    openknxHeatingActuatorModule.runMotor(_channelIndex, true);
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