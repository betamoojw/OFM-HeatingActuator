#include "HeatingActuatorChannel.h"
#include "HeatingActuatorModule.h"

HeatingActuatorChannel::HeatingActuatorChannel(uint8_t channelNumber)
{
    _channelIndex = channelNumber;
}

HeatingActuatorChannel::~HeatingActuatorChannel() {}

void HeatingActuatorChannel::processInputKo(GroupObject &ko)
{
    if (!ParamHTA_ChActive)
        return;

    float newTargetTempShift = 0;
    bool targetTempShiftStep = false;
    float targetTempShiftStepSize = 0;
    switch (HTA_KoCalcIndex(ko.asap()))
    {
        case HTA_KoChEnforcedPosition:
            _externEnforcedPosition = ko.value(DPT_Switch);
            logDebugP("HTA_KoChEnforcedPosition: %u", _externEnforcedPosition);
            break;
        case HTA_KoChSetValueInput:
            _externSetValuePercent = (uint8_t)(ko.value(DPT_Scaling)) / 100.0;
            _lastExternValue = delayTimerInit();
            logDebugP("HTA_KoChSetValueInput: %.2f", _externSetValuePercent);
            break;
        case HTA_KoChRoomTempInput:
            _externRoomTemp = ko.value(DPT_Value_Temp);
            _lastExternValue = delayTimerInit();
            logDebugP("HTA_KoChRoomTempInput: %.2f", _externRoomTemp);
            break;
        case HTA_KoChTargetTempInput:
            _externTargetTemp = ko.value(DPT_Value_Temp);
            logDebugP("HTA_KoChTargetTempInput: %.2f", _externTargetTemp);

            if (ParamHTA_ChTargetTempShiftResetOnNewTargetTemp)
                _externTargetTempShift = TEMPERATUR_INVALID;

            break;
        case HTA_KoChTargetTempShiftInput:
            newTargetTempShift = ko.value(DPT_Value_Temp);
            logDebugP("HTA_KoChTargetTempShiftInput: %.2f", _externTargetTempShift);
            checkTargetTempShift(newTargetTempShift);
            break;
        case HTA_KoChTargetTempShiftStep:
            targetTempShiftStep = ko.value(DPT_Switch);
            logDebugP("HTA_KoChTargetTempShiftStep: %u", targetTempShiftStep);

            switch (ParamHTA_ChTargetTempShift)
            {
                case 0:
                    targetTempShiftStepSize = 0.1;
                    break;
                case 1:
                    targetTempShiftStepSize = 0.2;
                    break;
                case 2:
                    targetTempShiftStepSize = 0.5;
                    break;
                case 3:
                    targetTempShiftStepSize = 1;
                    break;
            }

            if (targetTempShiftStep)
                newTargetTempShift = _externTargetTempShift + targetTempShiftStepSize;
            else
                newTargetTempShift = _externTargetTempShift - targetTempShiftStepSize;

            checkTargetTempShift(newTargetTempShift);
            break;
        case HTA_KoChHvacModeInput:
        case HTA_KoChHvacModeInputComfort:
        case HTA_KoChHvacModeInputNight:
        case HTA_KoChHvacModeInputProtect:
            checkHvacMode();
            break;
    }
}

void HeatingActuatorChannel::checkHvacMode()
{
    HvacMode externHvacMode = static_cast<HvacMode>((uint8_t)KoHTA_ChHvacModeInput.value(DPT_HVACMode));
    bool externHvacComfort = KoHTA_ChHvacModeInputComfort.value(DPT_Switch);
    bool externHvacNight = KoHTA_ChHvacModeInputNight.value(DPT_Switch);
    bool externHvacProtect = KoHTA_ChHvacModeInputProtect.value(DPT_Switch);

    logDebugP("checkHvacMode (externHvacMode=%u, externHvacMode=%u, externHvacMode=%u, externHvacMode=%u)", externHvacMode, externHvacComfort, externHvacNight, externHvacProtect);

    HvacMode newHvacMode = HvacMode::HVAC_NONE;

    if (externHvacMode == HvacMode::HVAC_PROTECT || externHvacProtect)
        newHvacMode = HvacMode::HVAC_PROTECT;
    else if (ParamHTA_ChHvacModePriority == 0)
    {
        // Protect>Comfort>Night>Standby

        if (externHvacMode == HvacMode::HVAC_COMFORT || externHvacComfort)
            newHvacMode = HvacMode::HVAC_COMFORT;
        else if (externHvacMode == HvacMode::HVAC_NIGHT || externHvacNight)
            newHvacMode = HvacMode::HVAC_NIGHT;
    }
    else
    {
        // Protect>Night>Comfort>Standby

        if (externHvacMode == HvacMode::HVAC_NIGHT || externHvacNight)
            newHvacMode = HvacMode::HVAC_NIGHT;
        else if (externHvacMode == HvacMode::HVAC_COMFORT || externHvacComfort)
            newHvacMode = HvacMode::HVAC_COMFORT;
    }

    if (newHvacMode == HvacMode::HVAC_NONE)
        newHvacMode = HvacMode::HVAC_STANDBY;
    
    if (_currentHvacMode != newHvacMode)
    {
        if (ParamHTA_ChTargetTempResetOnHvacModeChange)
            _externTargetTemp = TEMPERATUR_INVALID;
        if (ParamHTA_ChTargetTempShiftResetOnHvacModeChange)
            _externTargetTempShift = TEMPERATUR_INVALID;

        _currentHvacMode = newHvacMode;
        logDebugP("checkHvacMode (_currentHvacMode=%u, newHvacMode=%u)", _currentHvacMode, newHvacMode);
    }
}

void HeatingActuatorChannel::checkTargetTempShift(float newTargetTempShift)
{
    if (newTargetTempShift > ParamHTA_ChTargetTempShiftMax)
        newTargetTempShift = ParamHTA_ChTargetTempShiftMax;
    else if (newTargetTempShift < -ParamHTA_ChTargetTempShiftMax)
        newTargetTempShift = -ParamHTA_ChTargetTempShiftMax;

    switch (_currentHvacMode)
    {
        case HvacMode::HVAC_COMFORT:
            if (ParamHTA_ChTargetTempShiftApplyToComfort)
                _externTargetTempShift = newTargetTempShift;
            break;
        case HvacMode::HVAC_NIGHT:
            if (ParamHTA_ChTargetTempShiftApplyToNight || ParamHTA_ChTargetTempShiftActionNight)
                _externTargetTempShift = newTargetTempShift;

            if (ParamHTA_ChTargetTempShiftActionNight)
                _currentHvacMode = HvacMode::HVAC_COMFORT;
            
            break;
        case HvacMode::HVAC_STANDBY:
            if (ParamHTA_ChTargetTempShiftApplyToStandby || ParamHTA_ChTargetTempShiftActionStandby)
                _externTargetTempShift = newTargetTempShift;

            if (ParamHTA_ChTargetTempShiftActionStandby)
                _currentHvacMode = HvacMode::HVAC_COMFORT;
            
            break;
    }
}

// should not be called by channel itself,
// always call openknxHeatingActuatorModule.runMotor
void HeatingActuatorChannel::runMotor(bool open)
{
    logDebugP("Run motor (open: %s)", open ? "opening" : "closing");

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

uint8_t HeatingActuatorChannel::getMotorMaxCurrent(bool open)
{
    return open ? ParamHTA_ChMotorMaxCurrentOpen : ParamHTA_ChMotorMaxCurrentClose;
}

void HeatingActuatorChannel::startCalibration()
{
    logDebugP("Start calibration");
    calibrationState = CalibrationState::CAL_INIT;

    openknxHeatingActuatorModule.runMotor(_channelIndex, false);
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
    if (!ParamHTA_ChActive)
        return;

    if (_targetPositionPercent != POSITION_INVALID &&
        ParamHTA_ChSetValueChangeSend && delayCheck(_setValueCyclicSendTimer, ParamHTA_ChSetValueCyclicTimeMS))
    {
        if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
            KoHTA_ChSetValueStatusHeating.value(_targetPositionPercent, DPT_Scaling);
        else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
            KoHTA_ChSetValueStatusCooling.value(_targetPositionPercent, DPT_Scaling);
        else
        {
            //###ToDo: check heating or cooling
        }

        _setValueCyclicSendTimer = delayTimerInit();
    }
    
    if (_currentTargetTemp != TEMPERATUR_INVALID &&
        ParamHTA_ChTargetTempChangeSend && delayCheck(_targetTempCyclicSendTimer, ParamHTA_ChTargetTempCyclicTimeMS))
    {
        KoHTA_ChTargetTempStatus.value(_currentTargetTemp, DPT_Value_Temp);
        _targetTempCyclicSendTimer = delayTimerInit();
    }
    
    if (ParamHTA_ChManualModeChangeSend && delayCheck(_manualModeCyclicSendTimer, ParamHTA_ChManualModeCyclicTimeMS))
    {
        KoHTA_ChManualModeStatus.value(_currentManualMode, DPT_Switch);
        _manualModeCyclicSendTimer = delayTimerInit();
    }

    // motor of current channel is running
    if (motorState != MotorState::MOT_IDLE)
    {
        // motor should be running, check if motor actually connected
        if (current != MOT_CURRENT_INVALID &&
            current < OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT)
        {
            if (calibrationState != CalibrationState::CAL_NONE)
                calibrationState = CalibrationState::CAL_ERROR;

            logDebugP("STOP: no motor connected (current: %.2f, min: %.2f)", current, OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT);
            openknxHeatingActuatorModule.stopMotor();
        }

        // moving valve to position
        if (calibrationState == CalibrationState::CAL_COMPLETE &&
            _targetPositionPercent != POSITION_INVALID)
        {
            float newCurrentPositionPercent;
            u_int32_t motorRunTime = millis() - _motorStarted;
            switch (motorState)
            {
                case MotorState::MOT_OPENING:
                    newCurrentPositionPercent = _currentPositionPercent + motorRunTime / (float)_calibratedDriveOpenTime;
                    if (newCurrentPositionPercent >= _targetPositionPercent)
                    {
                        openknxHeatingActuatorModule.stopMotor();
                        _currentPositionPercent = newCurrentPositionPercent;

                        logDebugP("New position reached (newCurrentPositionPercent: %.4f)", newCurrentPositionPercent);
                    }
                    break;
                case MotorState::MOT_CLOSING:
                    newCurrentPositionPercent = _currentPositionPercent - motorRunTime / (float)_calibratedDriveCloseTime;
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
        // motor of current channel is not running, but motor of another channel
        if (motorPower)
            return;

        // if we are calibrating, move to next calibration step
        switch (calibrationState)
        {
            case CalibrationState::CAL_INIT:
                if (delayCheck(_motorStopped, MOT_RESTART_DELAY))
                {
                    calibrationState = CalibrationState::CAL_OPENING;
                    openknxHeatingActuatorModule.runMotor(_channelIndex, true);
                }
                break;
            case CalibrationState::CAL_OPENING:
                if (delayCheck(_motorStopped, MOT_RESTART_DELAY))
                {
                    _calibratedDriveOpenTime = _motorStopped - _motorStarted;
                    _currentPositionPercent = 1;

                    calibrationState = CalibrationState::CAL_CLOSING;
                    openknxHeatingActuatorModule.runMotor(_channelIndex, false);
                }
                break;
            case CalibrationState::CAL_CLOSING:
                _calibratedDriveCloseTime = _motorStopped - _motorStarted;
                _currentPositionPercent = 0;

                calibrationState = CalibrationState::CAL_COMPLETE;
                logDebugP("Calibration complete (calibratedDriveOpenTime: %u ms, calibratedDriveCloseTime: %u ms)", _calibratedDriveOpenTime, _calibratedDriveCloseTime);
                break;
        }

        // first check for possible enforced position
        float setValuePercent = POSITION_INVALID;
        if (ParamHTA_ChEnforcedPosition &&
            _externEnforcedPosition)
        {
            if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN)
                setValuePercent = ParamHTA_ChEnforcedSetValueHeatingOrExtern;
            else
            {
                if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                    setValuePercent = ParamHTA_ChEnforcedSetValueHeatingOrExtern;
                else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                    setValuePercent = ParamHTA_ChEnforcedSetValueCooling;
                else
                {
                    //###ToDo: check heating or cooling
                }
            }
        }
        else
        {
            // check if emergency mode should be active
            if (ParamHTA_ChEmergencyMode &&
                delayCheck(_lastExternValue, ParamHTA_ChEmergencyModeDelayTimeMS))
            {
                if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN)
                    setValuePercent = ParamHTA_ChEmergencyModeSetValueHeatingOrExtern;
                else
                {
                    if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                        setValuePercent = ParamHTA_ChEmergencyModeSetValueHeatingOrExtern;
                    else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                        setValuePercent = ParamHTA_ChEmergencyModeSetValueCooling;
                    else
                    {
                        //###ToDo: check heating or cooling
                    }
                }
            }
            else
            {
                if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN)
                    setValuePercent = _externSetValuePercent;
                else
                {
                    float targetTemp = _externTargetTemp;
                    if (targetTemp == TEMPERATUR_INVALID)
                    {
                        switch (_currentHvacMode)
                        {
                            case HvacMode::HVAC_COMFORT:
                                if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                                    targetTemp = ParamHTA_ChTargetTempHeatingComfort;
                                else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                                    targetTemp = ParamHTA_ChTargetTempCoolingComfort;
                                else
                                {
                                    //###ToDo: check heating or cooling
                                }
                                break;
                            case HvacMode::HVAC_NIGHT:
                                if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                                    targetTemp = ParamHTA_ChTargetTempHeatingNight;
                                else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                                    targetTemp = ParamHTA_ChTargetTempCoolingNight;
                                else
                                {
                                    //###ToDo: check heating or cooling
                                }
                                break;
                            case HvacMode::HVAC_PROTECT:
                                if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                                    targetTemp = ParamHTA_ChTargetTempHeatingProtect;
                                else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                                    targetTemp = ParamHTA_ChTargetTempCoolingProtect;
                                else
                                {
                                    //###ToDo: check heating or cooling
                                }
                                break;
                            default:
                                if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                                    targetTemp = ParamHTA_ChTargetTempHeatingStandby;
                                else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                                    targetTemp = ParamHTA_ChTargetTempCoolingStandby;
                                else
                                {
                                    //###ToDo: check heating or cooling
                                }
                                break;
                        }
                    }

                    if (_externTargetTempShift != TEMPERATUR_INVALID)
                        targetTemp += _externTargetTempShift;

                    if (_currentTargetTemp != targetTemp)
                    {
                        _currentTargetTemp = targetTemp;

                        if (ParamHTA_ChTargetTempChangeSend)
                            KoHTA_ChTargetTempStatus.value(_currentTargetTemp, DPT_Value_Temp);
                    }

                    //###ToDo: calculate PID value for _externRoomTemp vs. targetTemp
                }
            }
        }

        // check if we need to move valve
        if (setValuePercent != POSITION_INVALID &&
            abs(_targetPositionPercent - setValuePercent) >= 0.01)
        {
            if (calibrationState != CalibrationState::CAL_COMPLETE)
            {
                if (calibrationState != CalibrationState::CAL_ERROR)
                    startCalibration();
            }
            else
            {
                moveValveToPosition(_externSetValuePercent);

                if (ParamHTA_ChSetValueChangeSend)
                {
                    if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
                        KoHTA_ChSetValueStatusHeating.value(_targetPositionPercent, DPT_Scaling);
                    else if (ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
                        KoHTA_ChSetValueStatusCooling.value(_targetPositionPercent, DPT_Scaling);
                    else
                    {
                        //###ToDo: check heating or cooling
                    }
                }
            }
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

    if (configured)
    {
        if (ParamHTA_ChSetValueChangeSend && ParamHTA_ChSetValueCyclicTimeMS > 0)
            _setValueCyclicSendTimer = delayTimerInit();
        if (ParamHTA_ChTargetTempChangeSend && ParamHTA_ChTargetTempCyclicTimeMS > 0)
            _targetTempCyclicSendTimer = delayTimerInit();
        if (ParamHTA_ChManualModeChangeSend && ParamHTA_ChManualModeCyclicTimeMS > 0)
            _manualModeCyclicSendTimer = delayTimerInit();
    }
}

void HeatingActuatorChannel::savePower()
{
    if (motorState == MotorState::MOT_IDLE)
        return;
    
    openknxHeatingActuatorModule.stopMotor();

    // reset calibration in case running
    if (calibrationState != CalibrationState::CAL_NONE &&
        calibrationState != CalibrationState::CAL_ERROR)
        calibrationState = CalibrationState::CAL_NONE;
}

bool HeatingActuatorChannel::restorePower()
{
    return true;
}

void HeatingActuatorChannel::writeChannelData()
{
    openknx.flash.writeByte(calibrationState);
    openknx.flash.writeInt(_calibratedDriveOpenTime);
    openknx.flash.writeInt(_calibratedDriveCloseTime);
    openknx.flash.writeFloat(_currentPositionPercent);
}

void HeatingActuatorChannel::readChannelData()
{
    calibrationState = static_cast<CalibrationState>(openknx.flash.readByte());
    _calibratedDriveOpenTime = openknx.flash.readInt();
    _calibratedDriveCloseTime = openknx.flash.readInt();
    _currentPositionPercent = openknx.flash.readFloat();
}

const std::string HeatingActuatorChannel::name()
{
    return "HeatingChannel";
}