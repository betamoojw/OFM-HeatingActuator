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
    
    switch (ko.asap())
    {
        case HTA_OperationMode:
        case HTA_KoSummerWinter:
            checkOperationMode();
            break;
    }

    float newTargetTemp = 0;
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
            newTargetTemp = ko.value(DPT_Value_Temp);
            logDebugP("HTA_KoChTargetTempInput: %.2f", newTargetTemp);
            setTargetTemp(newTargetTemp);
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
        case HTA_KoChTargetTempLockHeating:
            logDebugP("HTA_KoChLockHeating: %u", ko.value(DPT_Switch));
            if ((bool)KoHTA_ChTargetTempLockHeatingStatus.value(DPT_Switch) != (bool)ko.value(DPT_Switch))
                KoHTA_ChTargetTempLockHeatingStatus.value(ko.value(DPT_Switch), DPT_Switch);
            break;
        case HTA_KoChTargetTempLockCooling:
            logDebugP("HTA_KoChLockCooling: %u", ko.value(DPT_Switch));
            if ((bool)KoHTA_ChTargetTempLockCoolingStatus.value(DPT_Switch) != (bool)ko.value(DPT_Switch))
                KoHTA_ChTargetTempLockCoolingStatus.value(ko.value(DPT_Switch), DPT_Switch);
            break;
        case HTA_KoChManualMode:
            _currentManualMode = ko.value(DPT_Switch);
            logDebugP("HTA_KoChManualMode: %u", _currentManualMode);
            break;
        case HTA_KoChScene:
            if ((uint8_t)ko.value(Dpt(18, 1, 0)) == 0)
                processScene(ko.value(Dpt(18, 1, 1)));
            break;
    }
}

void HeatingActuatorChannel::checkOperationMode()
{
    bool newOperationModeHeating = false;
    if (ParamHTA_OperationMode == HTA_OPERATION_MODE_HEATING ||
        ParamHTA_ChOperationMode == HTA_OPERATION_MODE_HEATING)
        newOperationModeHeating = true;
    else if (ParamHTA_OperationMode == HTA_OPERATION_MODE_COOLING ||
             ParamHTA_ChOperationMode == HTA_OPERATION_MODE_COOLING)
             newOperationModeHeating = false;
    else
    {
        if (ParamHTA_OperationModeChange == HTA_OPERATION_MODE_CHANGE_OBJECT_HEATING_COOLING)
            newOperationModeHeating = KoHTA_OperationMode.value(DPT_Switch);
        else
            newOperationModeHeating = !KoHTA_SummerWinter.value(DPT_Switch);
    }

    setOperationMode(newOperationModeHeating);
}

void HeatingActuatorChannel::setOperationMode(bool newOperationModeHeating)
{
    if (_currentOperationModeHeating != newOperationModeHeating)
    {
        if (pid.isRunning())
        {
            pid.stop();
            pid.reset();
        }

        _currentOperationModeHeating = newOperationModeHeating;
        logDebugP("setOperationMode (_currentOperationModeHeating=%u, newOperationModeHeating=%u)", _currentOperationModeHeating, newOperationModeHeating);
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
    
    setHvacMode(newHvacMode);
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
                setTargetTempShift(newTargetTempShift);
            break;
        case HvacMode::HVAC_NIGHT:
            if (ParamHTA_ChTargetTempShiftApplyToNight || ParamHTA_ChTargetTempShiftActionNight)
                setTargetTempShift(newTargetTempShift);

            if (ParamHTA_ChTargetTempShiftActionNight)
                _currentHvacMode = HvacMode::HVAC_COMFORT;
            
            break;
        case HvacMode::HVAC_STANDBY:
            if (ParamHTA_ChTargetTempShiftApplyToStandby || ParamHTA_ChTargetTempShiftActionStandby)
                setTargetTempShift(newTargetTempShift);

            if (ParamHTA_ChTargetTempShiftActionStandby)
                _currentHvacMode = HvacMode::HVAC_COMFORT;
            
            break;
    }
}

void HeatingActuatorChannel::checkEmergencyMode()
{
    bool newEmergencyMode =
        ParamHTA_ChEmergencyMode &&
        delayCheck(_lastExternValue, ParamHTA_ChEmergencyModeDelayTimeMS);

    if (_currentEmergencyMode != newEmergencyMode)
    {
        _currentEmergencyMode = newEmergencyMode;
        logDebugP("checkEmergencyMode (_currentEmergencyMode=%u, newEmergencyMode=%u)", _currentEmergencyMode, newEmergencyMode);

        KoHTA_ChEmergencyModeStatus.value(_currentEmergencyMode, DPT_Switch);
    }
}

void HeatingActuatorChannel::processScene(uint8_t sceneNumber)
{
    if (ParamHTA_ChSceneAActive &&
        ParamHTA_ChSceneANumber == sceneNumber)
    {
        if (ParamHTA_ChSceneAChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneAHvacMode + 1));
        if (ParamHTA_ChSceneAChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneAChangeTargetTempInput);
        if (ParamHTA_ChSceneAChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneAChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneBActive &&
             ParamHTA_ChSceneBNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneBChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneBHvacMode + 1));
        if (ParamHTA_ChSceneBChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneBChangeTargetTempInput);
        if (ParamHTA_ChSceneBChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneBChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneCActive &&
             ParamHTA_ChSceneCNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneCChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneCHvacMode + 1));
        if (ParamHTA_ChSceneCChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneCChangeTargetTempInput);
        if (ParamHTA_ChSceneCChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneCChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneDActive &&
             ParamHTA_ChSceneDNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneDChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneDHvacMode + 1));
        if (ParamHTA_ChSceneDChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneDChangeTargetTempInput);
        if (ParamHTA_ChSceneDChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneDChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneEActive &&
             ParamHTA_ChSceneENumber == sceneNumber)
    {
        if (ParamHTA_ChSceneEChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneEHvacMode + 1));
        if (ParamHTA_ChSceneEChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneEChangeTargetTempInput);
        if (ParamHTA_ChSceneEChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneEChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneFActive &&
             ParamHTA_ChSceneFNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneFChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneFHvacMode + 1));
        if (ParamHTA_ChSceneFChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneFChangeTargetTempInput);
        if (ParamHTA_ChSceneFChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneFChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneGActive &&
             ParamHTA_ChSceneGNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneGChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneGHvacMode + 1));
        if (ParamHTA_ChSceneGChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneGChangeTargetTempInput);
        if (ParamHTA_ChSceneGChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneGChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneHActive &&
             ParamHTA_ChSceneHNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneHChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneHHvacMode + 1));
        if (ParamHTA_ChSceneHChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneHChangeTargetTempInput);
        if (ParamHTA_ChSceneHChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneHChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneIActive &&
             ParamHTA_ChSceneINumber == sceneNumber)
    {
        if (ParamHTA_ChSceneIChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneIHvacMode + 1));
        if (ParamHTA_ChSceneIChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneIChangeTargetTempInput);
        if (ParamHTA_ChSceneIChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneIChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneJActive &&
             ParamHTA_ChSceneJNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneJChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneJHvacMode + 1));
        if (ParamHTA_ChSceneJChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneJChangeTargetTempInput);
        if (ParamHTA_ChSceneJChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneJChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneKActive &&
             ParamHTA_ChSceneKNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneKChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneKHvacMode + 1));
        if (ParamHTA_ChSceneKChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneKChangeTargetTempInput);
        if (ParamHTA_ChSceneKChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneKChangeTargetTempShift);
    }
    else if (ParamHTA_ChSceneLActive &&
             ParamHTA_ChSceneLNumber == sceneNumber)
    {
        if (ParamHTA_ChSceneLChangeHvacMode)
            setHvacMode(static_cast<HvacMode>(ParamHTA_ChSceneLHvacMode + 1));
        if (ParamHTA_ChSceneLChangeTargetTempInput)
            setTargetTemp(ParamHTA_ChSceneLChangeTargetTempInput);
        if (ParamHTA_ChSceneLChangeTargetTempShift)
            setTargetTempShift(ParamHTA_ChSceneLChangeTargetTempShift);
    }
}

void HeatingActuatorChannel::setHvacMode(HvacMode newHvacMode)
{
    if (_currentHvacMode != newHvacMode)
    {
        if (ParamHTA_ChTargetTempResetOnHvacModeChange)
            _externTargetTemp = HTA_TEMPERATUR_INVALID;
        if (ParamHTA_ChTargetTempShiftResetOnHvacModeChange)
            _externTargetTempShift = HTA_TEMPERATUR_INVALID;

        _currentHvacMode = newHvacMode;
        logDebugP("setHvacMode (_currentHvacMode=%u, newHvacMode=%u)", _currentHvacMode, newHvacMode);
    }
}

void HeatingActuatorChannel::setTargetTemp(float newTargetTemp)
{
    if (_currentOperationModeHeating && KoHTA_ChTargetTempLockHeating.value(DPT_Switch) ||
        !_currentOperationModeHeating && KoHTA_ChTargetTempLockCooling.value(DPT_Switch))
    {
        logDebugP("Target temperature locked, ignore setTargetTemp.");
        return;
    }

    if (_externTargetTemp != newTargetTemp)
    {
        _externTargetTemp = newTargetTemp;
        
        if (ParamHTA_ChTargetTempShiftResetOnNewTargetTemp)
            setTargetTempShift(0);
    }
}

void HeatingActuatorChannel::setTargetTempShift(float newTargetTempShift)
{
    if (_currentOperationModeHeating && KoHTA_ChTargetTempLockHeating.value(DPT_Switch) ||
        !_currentOperationModeHeating && KoHTA_ChTargetTempLockCooling.value(DPT_Switch))
    {
        logDebugP("Target temperature locked, ignore setTargetTempShift.");
        return;
    }

    if (_externTargetTempShift != newTargetTempShift)
    {
        _externTargetTempShift = newTargetTempShift;
    }
}

// should not be called by channel itself,
// always call openknxHeatingActuatorModule.runMotor
void HeatingActuatorChannel::runMotor(bool open)
{
    logDebugP("Run motor (open: %s)", open ? "opening" : "closing");

    digitalWrite(MOTOR_PINS[_channelIndex], MOT_ON);
    _motorStarted = millis();

    _motorState = open ? MotorState::MOT_OPENING : MotorState::MOT_CLOSING;
}

// should not be called by channel itself,
// always call openknxHeatingActuatorModule.stopMotor
void HeatingActuatorChannel::stopMotor()
{
    digitalWrite(MOTOR_PINS[_channelIndex], MOT_OFF);
    _motorStopped = millis();
    _motorState = MotorState::MOT_IDLE;
}

bool HeatingActuatorChannel::considerForRequestAndMaxSetValue()
{
    return ParamHTA_ChConsiderForRequestAndMaxSetValue;
}

bool HeatingActuatorChannel::isOperationModeHeating()
{
    return _currentOperationModeHeating;
}

uint8_t HeatingActuatorChannel::getSetValueTarget()
{
    return static_cast<uint8_t>(round(_targetPositionPercent * 100));
}

void HeatingActuatorChannel::startCalibration()
{
    logDebugP("Start calibration");
    _calibrationState = CalibrationState::CAL_INIT;

    openknxHeatingActuatorModule.runMotor(_channelIndex, false);
}

bool HeatingActuatorChannel::moveValveToPosition(float targetPositionPercent)
{
    if (_calibrationState != CalibrationState::CAL_COMPLETE)
    {
        logInfoP("Moving to position only possible after calibration!");
        return false;
    }
    
    logInfoP("Moving to position (_currentPositionPercent=%.2f, targetPositionPercent=%.2f)", _currentPositionPercent, targetPositionPercent);
    setTargetPosition(targetPositionPercent);

    bool open = _currentPositionPercent < _targetPositionPercent;
    openknxHeatingActuatorModule.runMotor(_channelIndex, open);
    
    return true;
}

void HeatingActuatorChannel::setTargetPosition(float targetPositionPercent)
{
    _targetPositionPercent = targetPositionPercent;

    if (ParamHTA_ChSetValueChangeSend)
    {
        if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN || _currentOperationModeHeating)
            KoHTA_ChSetValueStatusHeatingOrExtern.value(_targetPositionPercent * 100, DPT_Scaling);
        else
            KoHTA_ChSetValueStatusCooling.value(_targetPositionPercent * 100, DPT_Scaling);
    }
}

void HeatingActuatorChannel::loop(bool motorPower, uint32_t currentCount, float current, float currentLast)
{
    if (!ParamHTA_ChActive)
        return;

    if (currentCount < 10)
    {
        current = MOT_CURRENT_INVALID;
        currentLast = MOT_CURRENT_INVALID;
    }
    
    processInput();

    if (_targetPositionPercent != HTA_POSITION_INVALID &&
        ParamHTA_ChSetValueChangeSend && _setValueCyclicSendTimer > 0 &&
        delayCheck(_setValueCyclicSendTimer, ParamHTA_ChSetValueCyclicTimeMS))
    {
        if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN || _currentOperationModeHeating)
            KoHTA_ChSetValueStatusHeatingOrExtern.value(_targetPositionPercent * 100, DPT_Scaling);
        else
            KoHTA_ChSetValueStatusCooling.value(_targetPositionPercent * 100, DPT_Scaling);

        _setValueCyclicSendTimer = delayTimerInit();
    }
    
    if (_currentTargetTemp != HTA_TEMPERATUR_INVALID &&
        ParamHTA_ChTargetTempChangeSend && _targetTempCyclicSendTimer > 0 &&
        delayCheck(_targetTempCyclicSendTimer, ParamHTA_ChTargetTempCyclicTimeMS))
    {
        KoHTA_ChTargetTempStatus.value(_currentTargetTemp, DPT_Value_Temp);
        _targetTempCyclicSendTimer = delayTimerInit();
    }
    
    if (ParamHTA_ChEmergencyModeChangeSend &&
        ((bool)KoHTA_ChEmergencyModeStatus.value(DPT_Switch) != _currentEmergencyMode ||
         _EmergencyModeCyclicSendTimer > 0 && delayCheck(_EmergencyModeCyclicSendTimer, ParamHTA_ChEmergencyModeCyclicTimeMS)))
    {
        KoHTA_ChEmergencyModeStatus.value(_currentEmergencyMode, DPT_Switch);
        _EmergencyModeCyclicSendTimer = delayTimerInit();
    }

    
    if (ParamHTA_ChManualModeChangeSend &&
        ((bool)KoHTA_ChManualModeStatus.value(DPT_Switch) != _currentManualMode ||
         _manualModeCyclicSendTimer > 0 && delayCheck(_manualModeCyclicSendTimer, ParamHTA_ChManualModeCyclicTimeMS)))
    {
        KoHTA_ChManualModeStatus.value(_currentManualMode, DPT_Switch);
        _manualModeCyclicSendTimer = delayTimerInit();
    }

    // motor of current channel is running
    if (_motorState != MotorState::MOT_IDLE)
    {
        // motor should be running, check if motor actually connected
        if (current != MOT_CURRENT_INVALID &&
            current < OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT)
        {
            if (_calibrationState != CalibrationState::CAL_NONE)
                _calibrationState = CalibrationState::CAL_ERROR;

            logDebugP("STOP: no motor connected (current: %.2f, min: %.2f)", current, OPENKNX_HTA_CURRENT_MOT_MIN_LIMIT);
            openknxHeatingActuatorModule.stopMotor();
        }

        if (currentCount >= 500 &&
            currentLast > 0)
        {
            uint8_t motorMaxCurrent = _motorState == MotorState::MOT_OPENING ? ParamHTA_ChMotorMaxCurrentOpen : ParamHTA_ChMotorMaxCurrentClose;
            if (current > currentLast + 0.1 &&
                current > motorMaxCurrent)
            {
                if (_motorState == MotorState::MOT_OPENING)
                    _currentPositionPercent = 1;
                else
                    _currentPositionPercent = 0;

                logDebugP("STOP (current: %.2f, last: %.2f, limit: %u, _currentPositionPercent: %.2f)", current, currentLast, motorMaxCurrent, _currentPositionPercent);
                openknxHeatingActuatorModule.stopMotor();
            }
        }

        // moving valve to position
        if (_calibrationState == CalibrationState::CAL_COMPLETE &&
            _targetPositionPercent != HTA_POSITION_INVALID)
        {
            float newCurrentPositionPercent;
            u_int32_t motorRunTime = millis() - _motorStarted;
            switch (_motorState)
            {
                case MotorState::MOT_OPENING:
                    newCurrentPositionPercent = min(_currentPositionPercent + motorRunTime / (float)_calibratedDriveOpenTime, 1);
                    if (newCurrentPositionPercent >= _targetPositionPercent)
                    {
                        openknxHeatingActuatorModule.stopMotor();
                        _currentPositionPercent = newCurrentPositionPercent;

                        logDebugP("New position reached (newCurrentPositionPercent: %.4f, _targetPositionPercent: %.4f)", newCurrentPositionPercent, _targetPositionPercent);
                    }
                    break;
                case MotorState::MOT_CLOSING:
                    newCurrentPositionPercent = max(_currentPositionPercent - motorRunTime / (float)_calibratedDriveCloseTime, 0);
                    if (newCurrentPositionPercent <= _targetPositionPercent)
                    {
                        openknxHeatingActuatorModule.stopMotor();
                        _currentPositionPercent = newCurrentPositionPercent;

                        logDebugP("New position reached (newCurrentPositionPercent: %.4f, _targetPositionPercent: %.4f)", newCurrentPositionPercent, _targetPositionPercent);
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
        switch (_calibrationState)
        {
            case CalibrationState::CAL_INIT:
                if (delayCheck(_motorStopped, HTA_MOT_RESTART_DELAY))
                {
                    _calibrationState = CalibrationState::CAL_OPENING;
                    openknxHeatingActuatorModule.runMotor(_channelIndex, true);
                }
                break;
            case CalibrationState::CAL_OPENING:
                if (delayCheck(_motorStopped, HTA_MOT_RESTART_DELAY))
                {
                    _calibratedDriveOpenTime = _motorStopped - _motorStarted;
                    _currentPositionPercent = 1;

                    _calibrationState = CalibrationState::CAL_CLOSING;
                    openknxHeatingActuatorModule.runMotor(_channelIndex, false);
                }
                break;
            case CalibrationState::CAL_CLOSING:
                _calibratedDriveCloseTime = _motorStopped - _motorStarted;
                _currentPositionPercent = 0;

                _calibrationState = CalibrationState::CAL_COMPLETE;
                logDebugP("Calibration complete (calibratedDriveOpenTime: %u ms, calibratedDriveCloseTime: %u ms)", _calibratedDriveOpenTime, _calibratedDriveCloseTime);
                break;
            default:
                calculateNewSetValue();
                break;
        }
    }

    processOutput();
}

void HeatingActuatorChannel::calculateNewSetValue()
{
    std::string debugLogMessage = "";

    // first check for possible enforced position
    float setValuePercent = HTA_POSITION_INVALID;
    if (ParamHTA_ChEnforcedPosition &&
        _externEnforcedPosition)
    {
        if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN)
            setValuePercent = ParamHTA_ChEnforcedSetValueHeatingOrExtern / (float)100;
        else
        {
            if (_currentOperationModeHeating)
                setValuePercent = ParamHTA_ChEnforcedSetValueHeatingOrExtern / (float)100;
            else
                setValuePercent = ParamHTA_ChEnforcedSetValueCooling / (float)100;
        }

#ifdef OPENKNX_DEBUG
        debugLogMessage = string_format("calculateNewSetValue: enforced position (setValuePercent: %.2f)", setValuePercent);
#endif
    }
    // check if manual mode is active
    else if (ParamHTA_ChManualMode && _currentManualMode)
    {
        if (_currentManualModeOn)
            setValuePercent = ParamHTA_ChManualModeSetValueOn / (float)100;
        else
            setValuePercent = ParamHTA_ChManualModeSetValueOff / (float)100;

#ifdef OPENKNX_DEBUG
        debugLogMessage = string_format("calculateNewSetValue: manual mode (_currentManualModeOn: %u, setValuePercent: %.2f)", _currentManualModeOn, setValuePercent);
#endif
    }
    // check if emergency mode should be active
    else if ()
    {
        _currentEmergencyMode = true;

        if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN)
            setValuePercent = ParamHTA_ChEmergencyModeSetValueHeatingOrExtern / (float)100;
        else
        {
            if (_currentOperationModeHeating)
                setValuePercent = ParamHTA_ChEmergencyModeSetValueHeatingOrExtern / (float)100;
            else
                setValuePercent = ParamHTA_ChEmergencyModeSetValueCooling / (float)100;
        }

#ifdef OPENKNX_DEBUG
        debugLogMessage = string_format("calculateNewSetValue: emergency mode (setValuePercent: %.2f)", setValuePercent);
#endif
    }
    // check for external control
    else if (ParamHTA_ChControlMode == HTA_CONTROL_MODE_EXTERN)
    {
        setValuePercent = _externSetValuePercent;

#ifdef OPENKNX_DEBUG
        debugLogMessage = string_format("calculateNewSetValue: external control (setValuePercent: %.2f)", setValuePercent);
#endif
    }
    // standard internal regulator target temperature calculation
    else
    {
        float targetTemp = _externTargetTemp;
        if (targetTemp == HTA_TEMPERATUR_INVALID)
        {
            switch (_currentHvacMode)
            {
                case HvacMode::HVAC_COMFORT:
                    if (_currentOperationModeHeating)
                        targetTemp = ParamHTA_ChTargetTempHeatingComfort;
                    else
                        targetTemp = ParamHTA_ChTargetTempCoolingComfort;
                    break;
                case HvacMode::HVAC_NIGHT:
                    if (_currentOperationModeHeating)
                        targetTemp = ParamHTA_ChTargetTempHeatingNight;
                    else
                        targetTemp = ParamHTA_ChTargetTempCoolingNight;
                    break;
                case HvacMode::HVAC_PROTECT:
                    if (_currentOperationModeHeating)
                        targetTemp = ParamHTA_ChTargetTempHeatingProtect;
                    else
                        targetTemp = ParamHTA_ChTargetTempCoolingProtect;
                    break;
                default:
                    if (_currentOperationModeHeating)
                        targetTemp = ParamHTA_ChTargetTempHeatingStandby;
                    else
                        targetTemp = ParamHTA_ChTargetTempCoolingStandby;
                    break;
            }
        }

        targetTemp += _externTargetTempShift;

        if (_currentTargetTemp != targetTemp)
        {
            _currentTargetTemp = targetTemp;
            pid.setPoint(targetTemp);

            if (ParamHTA_ChTargetTempChangeSend)
                KoHTA_ChTargetTempStatus.value(_currentTargetTemp, DPT_Value_Temp);
        }

        float newPidPositionPercent = HTA_POSITION_INVALID;
        if (_externRoomTemp != HTA_TEMPERATUR_INVALID)
        {
            if (!pid.isRunning())
            {
                if (_currentOperationModeHeating)
                {
                    pid.setInterval(ParamHTA_ChHeatingPidInterval);
                    pid.setK(ParamHTA_ChHeatingPidP, ParamHTA_ChHeatingPidI / (float)10, ParamHTA_ChHeatingPidD / (float)10);
                }
                else
                {
                    pid.setInterval(ParamHTA_ChCoolingPidInterval);
                    pid.setK(ParamHTA_ChCoolingPidP, ParamHTA_ChCoolingPidI / (float)10, ParamHTA_ChCoolingPidD / (float)10);
                }

                pid.setOutputRange(0, 255);
                pid.start();

                logDebugP("calculateNewSetValue: regulator PID initialized (P: %.2f, I: %.2f, D: %.2f, interval: %u)", pid.getKp(), pid.getKi(), pid.getKd(), pid.getInterval());
            }

            if (pid.compute(_externRoomTemp))
                newPidPositionPercent = pid.getOutput() / 255;
        }

#ifdef OPENKNX_DEBUG
        if (newPidPositionPercent != HTA_POSITION_INVALID)
            debugLogMessage = string_format("calculateNewSetValue: regulator (_currentHvacMode: %u, _externTargetTempShift: %.2f, targetTemp: %.2f, _externRoomTemp: %.2f, _targetPositionPercent: %.2f, newPidPositionPercent: %.2f)", _currentHvacMode, _externTargetTempShift, targetTemp, _externRoomTemp, _targetPositionPercent, newPidPositionPercent);
#endif

        if (newPidPositionPercent != HTA_POSITION_INVALID)
           setValuePercent = newPidPositionPercent;
    }

#ifdef OPENKNX_DEBUG
    if (debugLogMessage != "" &&
        _lastDebugLogMessage != debugLogMessage)
    {
        logDebugP(debugLogMessage);
        _lastDebugLogMessage = debugLogMessage;
    }
#endif

    // check if we need to move valve
    if (setValuePercent != HTA_POSITION_INVALID &&
        (_targetPositionPercent == HTA_POSITION_INVALID || abs(_targetPositionPercent - setValuePercent) >= 0.01))
    {
        if (_calibrationState != CalibrationState::CAL_COMPLETE)
        {
            if (_calibrationState != CalibrationState::CAL_ERROR)
                startCalibration();
        }
        else
            moveValveToPosition(setValuePercent);
    }
}

void HeatingActuatorChannel::processInput()
{
#ifdef OPENKNX_HTA_GPIO_INPUT_OFFSET
    if (!ParamHTA_ChManualMode)
        return;

    bool buttonPressed = openknxGPIOModule.digitalRead(OPENKNX_HTA_GPIO_INPUT_OFFSET + _channelIndex) == GPIO_INPUT_ON;
    if (buttonPressed)
    {
        if (_currentButtonPressed)
        {
            if (_currentManualMode &&
                delayCheck(_currentButtonPressedStarted, HTA_MANUAL_MODE_CHANGE_TO_AUTO_TIME_DELAY) &&
                (ParamHTA_ChManualModeChangeToAuto == HTA_MANUAL_MODE_CHANGE_TO_AUTO_BUTTON || ParamHTA_ChManualModeChangeToAuto == HTA_MANUAL_MODE_CHANGE_TO_AUTO_BUTTON_TIME))
            {
                _currentManualMode = false;
                logDebugP("processInput: manual mode button off (_currentManualMode: %u, buttonPressed: %u, _currentButtonPressed: %u, _currentButtonPressedStarted: %u)", _currentManualMode, buttonPressed, _currentButtonPressed, _currentButtonPressedStarted);
            }
        }
        else
        {
            if (_currentManualMode)
            {
                _currentManualModeOn = !_currentManualModeOn;
                logDebugP("processInput: manual mode button toggle (_currentManualMode: %u, buttonPressed: %u, _currentButtonPressed: %u)", _currentManualMode, buttonPressed, _currentButtonPressed);
            }
            else
            {
                _currentManualMode = true;
                _currentManualModeOn = true;
                _currentManualModeStarted = delayTimerInit();
                logDebugP("processInput: manual mode button on (_currentManualMode: %u, buttonPressed: %u, _currentButtonPressed: %u)", _currentManualMode, buttonPressed, _currentButtonPressed);
            }

            _currentButtonPressed = true;
            _currentButtonPressedStarted = delayTimerInit();
        }
    }
    else
        _currentButtonPressed = false;

    if (_currentManualMode &&
        delayCheck(_currentManualModeStarted, ParamHTA_ChManualModeChangeToAutoTimeMS) &&
        (ParamHTA_ChManualModeChangeToAuto == HTA_MANUAL_MODE_CHANGE_TO_AUTO_TIME || ParamHTA_ChManualModeChangeToAuto == HTA_MANUAL_MODE_CHANGE_TO_AUTO_BUTTON_TIME))
    {
        _currentManualMode = false;
        logDebugP("processInput: manual mode time off (_currentManualMode: %u, _currentManualModeStarted: %u)", _currentManualMode, _currentManualModeStarted);
    }
#endif
}

void HeatingActuatorChannel::processOutput()
{
#ifdef OPENKNX_HTA_GPIO_OUTPUT_OFFSET
    float ledOnPercent = 0;
    uint32_t ledOnTime = 0;

    if (_currentManualMode)
    {
        if (_currentManualModeOn)
        {
            ledOnPercent = 1;
            ledOnTime = HTA_OUTPUT_LED_PHASE;
        }
    }
    else
    {
        if (_targetPositionPercent == HTA_POSITION_INVALID)
            return;

        ledOnPercent = _targetPositionPercent;

        // minimum of 1 % and maximum of 99 % LED on to signal automatic mode
        if (ledOnPercent < 0.01)
            ledOnPercent = 0.01;
        else if (ledOnPercent > 0.99)
            ledOnPercent = 0.99;

        ledOnTime = round(HTA_OUTPUT_LED_PHASE * ledOnPercent);
    }

    if (ledOnTime == HTA_OUTPUT_LED_PHASE)
    {
        if (!_currentLedOn)
            setOutputLed(true);
    }
    else if (ledOnTime == 0)
    {
        if (_currentLedOn)
            setOutputLed(false);
    }
    else if (_currentLedOn && delayCheck(_currentLedChangeStarted, ledOnTime))
        setOutputLed(false);
    else if (!_currentLedOn && delayCheck(_currentLedChangeStarted, HTA_OUTPUT_LED_PHASE - ledOnTime))
        setOutputLed(true);

    if (_currentLedOnTime != ledOnTime)
    {
        _currentLedOnTime = ledOnTime;
        logDebugP("processOutput (ledOnPercent: %.2f, ledOnTime: %u)", ledOnPercent, ledOnTime);
    }
#endif
}

void HeatingActuatorChannel::setOutputLed(bool on)
{
#ifdef OPENKNX_HTA_GPIO_OUTPUT_OFFSET
    openknxGPIOModule.digitalWrite(OPENKNX_HTA_GPIO_OUTPUT_OFFSET + _channelIndex, on ? GPIO_OUTPUT_ON : GPIO_OUTPUT_OFF);
    _currentLedChangeStarted = delayTimerInit();
    _currentLedOn = on;
#endif
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
    if (_motorState == MotorState::MOT_IDLE)
        return;
    
    openknxHeatingActuatorModule.stopMotor();

    // reset calibration in case running
    if (_calibrationState != CalibrationState::CAL_NONE &&
        _calibrationState != CalibrationState::CAL_ERROR)
        _calibrationState = CalibrationState::CAL_NONE;
}

bool HeatingActuatorChannel::restorePower()
{
    return true;
}

void HeatingActuatorChannel::writeChannelData()
{
    openknx.flash.writeByte(_calibrationState);
    openknx.flash.writeInt(_calibratedDriveOpenTime);
    openknx.flash.writeInt(_calibratedDriveCloseTime);
    openknx.flash.writeFloat(_currentPositionPercent);
}

void HeatingActuatorChannel::readChannelData()
{
    _calibrationState = static_cast<CalibrationState>(openknx.flash.readByte());
    _calibratedDriveOpenTime = openknx.flash.readInt();
    _calibratedDriveCloseTime = openknx.flash.readInt();
    _currentPositionPercent = openknx.flash.readFloat();
}

void HeatingActuatorChannel::logChannelInfo(bool diagnoseKo)
{
    logInfoP("Channel info:");
    logIndentUp();

    logInfoP("_currentPositionPercent: %.2f", _currentPositionPercent);
    logInfoP("_targetPositionPercent: %.2f", _targetPositionPercent);
    logInfoP("_calibrationState: %u", _calibrationState);
    logInfoP("_calibratedDriveOpenTime: %u", _calibratedDriveOpenTime);
    logInfoP("_calibratedDriveCloseTime: %u", _calibratedDriveCloseTime);
    logInfoP("_externEnforcedPosition: %u", _externEnforcedPosition);
    logInfoP("_currentManualMode: %u (On=%u)", _currentManualMode, _currentManualModeOn);
    logInfoP("_currentHvacMode: %u", _currentHvacMode);

    if (diagnoseKo)
    {
        openknx.console.writeDiagnoseKo("HTA cur %.2f", _currentPositionPercent);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA tar %.2f", _targetPositionPercent);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA cal %u", _calibrationState);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA calo %u", _calibratedDriveOpenTime);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA calc %u", _calibratedDriveCloseTime);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA enf %u", _externEnforcedPosition);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA man %u %u", _currentManualMode, _currentManualModeOn);
        openknx.console.writeDiagnoseKo("");
        openknx.console.writeDiagnoseKo("HTA hvac %u", _currentHvacMode);
        openknx.console.writeDiagnoseKo("");
    }

    logIndentDown();
}

const std::string HeatingActuatorChannel::name()
{
    return "HeatingChannel";
}