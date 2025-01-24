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

// should not be called by channel itself,
// always call openknxHeatingActuatorModule.runMotor
void HeatingActuatorChannel::runMotor(bool open)
{
    logDebugP("Run motor (open: %u)", open);

    digitalWrite(MOTOR_PINS[_channelIndex], MOT_ON);
    _motorStarted = millis();

    motorState = open ? MotorState::MOT_OPENING : MotorState::MOT_CLOSING;
}

// should not be called by channel itself,
// always call openknxHeatingActuatorModule.stopMotor
void HeatingActuatorChannel::stopMotor()
{
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_OFF);
    _motorStopped = millis();
    motorState = MotorState::MOT_IDLE;
}

void HeatingActuatorChannel::startCalibration()
{
    logDebugP("Start calibration");
    calibrationState = CalibrationState::CAL_INIT;

    openknxHeatingActuatorModule.runMotor(_channelIndex, true);
}

void HeatingActuatorChannel::moveValveToPosition(float targetPositionPercent)
{
    if (calibrationState != CalibrationState::CAL_COMPLETE)
    {
        logInfoP("Moving to position only possible after calibration!");
        return;
    }
    
    logInfoP("Moving to position (targetPositionPercent=%.2f)", targetPositionPercent);
    _targetPositionPercent = targetPositionPercent;

    bool open = _currentPositionPercent < _targetPositionPercent;
    openknxHeatingActuatorModule.runMotor(_channelIndex, open);
}

void HeatingActuatorChannel::loop(bool motorPower, float current)
{
    if (motorPower)
    {
        // motor should be running, check if motor actually connected
        if (current < OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT)
        {
            if (calibrationState != CalibrationState::CAL_NONE)
                calibrationState = CalibrationState::CAL_ERROR;

            logDebugP("STOP: no motor connected (current: %.2f, min: %.2f)", current, OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT);
            openknxHeatingActuatorModule.stopMotor();
        }

        if (calibrationState == CalibrationState::CAL_COMPLETE)
        {
            float newCurrentPositionPercent;
            u_int32_t motorRunTime = millis() - _motorStarted;
            switch (motorState)
            {
                case MotorState::MOT_OPENING:
                    newCurrentPositionPercent = _currentPositionPercent + motorRunTime / _calibratedDriveOpenTime;
                    if (newCurrentPositionPercent >= _targetPositionPercent)
                    {
                        openknxHeatingActuatorModule.stopMotor();
                        _currentPositionPercent = newCurrentPositionPercent;

                        logDebugP("New position reached (newCurrentPositionPercent: %.4f)", newCurrentPositionPercent);
                    }
                    break;
                case MotorState::MOT_CLOSING:
                    newCurrentPositionPercent = _currentPositionPercent - motorRunTime / _calibratedDriveOpenTime;
                    if (newCurrentPositionPercent <= _targetPositionPercent)
                    {
                        openknxHeatingActuatorModule.stopMotor();
                        _currentPositionPercent = newCurrentPositionPercent;

                        logDebugP("New position reached (newCurrentPositionPercent: %.4f)", newCurrentPositionPercent);
                    }
                    break;
            }
        }
    }
    else
    {
        // motor has stopped, move to next state
        switch (calibrationState)
        {
            case CalibrationState::CAL_INIT:
                calibrationState = CalibrationState::CAL_OPENING;
                openknxHeatingActuatorModule.runMotor(_channelIndex, true);
                break;
            case CalibrationState::CAL_OPENING:
                _calibratedDriveOpenTime = _motorStopped - _motorStarted;
                _currentPositionPercent = 1;

                calibrationState = CalibrationState::CAL_CLOSING;
                openknxHeatingActuatorModule.runMotor(_channelIndex, false);
                break;
            case CalibrationState::CAL_CLOSING:
                _calibratedDriveCloseTime = _motorStopped - _motorStarted;
                _currentPositionPercent = 0;

                calibrationState = CalibrationState::CAL_COMPLETE;
                logDebugP("Calibration complete (calibratedDriveOpenTime: %u ms, calibratedDriveCloseTime: %u ms)", _calibratedDriveOpenTime, _calibratedDriveCloseTime);
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