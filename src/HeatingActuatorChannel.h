#pragma once
#include "OpenKNX.h"
#include "PID_RT.h"
#include "FormatHelper.cpp"

#define HTA_POSITION_INVALID -1
#define HTA_TEMPERATUR_INVALID -127
#define HTA_SCENE_INVALID 255

#define HTA_MOT_RESTART_DELAY 1000 // avoid power spikes, especially when changing motor direction

#define HTA_OUTPUT_LED_PHASE 3000

#define HTA_CONTROL_MODE_EXTERN 0
#define HTA_CONTROL_MODE_INTERN 1

#define HTA_OPERATION_MODE_HEATING 0
#define HTA_OPERATION_MODE_COOLING 1
#define HTA_OPERATION_MODE_HEATCOOLING 2

#define HTA_OPERATION_MODE_CHANGE_OBJECT_HEATING_COOLING 0
#define HTA_OPERATION_MODE_CHANGE_OBJECT_SUMMER_WINTER 1

#define HTA_MANUAL_MODE_CHANGE_TO_AUTO_DISABLED 0
#define HTA_MANUAL_MODE_CHANGE_TO_AUTO_TIME 1
#define HTA_MANUAL_MODE_CHANGE_TO_AUTO_BUTTON 2
#define HTA_MANUAL_MODE_CHANGE_TO_AUTO_BUTTON_TIME 3
#define HTA_MANUAL_MODE_CHANGE_TO_AUTO_TIME_DELAY 3000

class HeatingActuatorChannel : public OpenKNX::Channel
{
  public:
    HeatingActuatorChannel(uint8_t channelNumber);
    ~HeatingActuatorChannel();

    void processInputKo(GroupObject &ko);
    void setup(bool configured);
    void loop(bool motorPower, uint32_t currentCount, float current, float currentLast);

    void runMotor(bool open);
    void stopMotor();
    bool considerForRequestAndMaxSetValue();
    bool isOperationModeHeating();
    uint8_t getSetValueTarget();
    void startCalibration();
    bool moveValveToPosition(float targetPositionPercent);

    void savePower();
    bool restorePower();
    void writeChannelData();
    void readChannelData();

    void logChannelInfo(bool diagnoseKo);

  protected:

  private:
    enum MotorState : uint8_t
    {
        MOT_IDLE,
        MOT_OPENING,
        MOT_CLOSING
    };

    enum CalibrationState : uint8_t
    {
        CAL_NONE,
        CAL_INIT,
        CAL_OPENING,
        CAL_CLOSING,
        CAL_COMPLETE,
        CAL_ERROR
    };

    enum HvacMode : uint8_t
    {
        HVAC_NONE,
        HVAC_COMFORT,
        HVAC_STANDBY,
        HVAC_NIGHT,
        HVAC_PROTECT
    };

    const std::string name() override;

    void checkOperationMode();
    void checkHvacMode();
    void checkTargetTempShift(float newTargetTempShift);
    void processScene(uint8_t sceneNumber);

    void setHvacMode(HvacMode hvacMode);
    void setTargetTemp(float newTargetTemp);
    void setTargetTempShift(float newTargetTempShift);

    void calculateNewSetValue();
    void processInput();
    void processOutput();
    void setOutputLed(bool on);

    uint32_t _setValueCyclicSendTimer = 0;
    uint32_t _targetTempCyclicSendTimer = 0;
    uint32_t _manualModeCyclicSendTimer = 0;

    MotorState _motorState = MotorState::MOT_IDLE;
    uint32_t _motorStarted = 0;
    uint32_t _motorStopped = 0;

    CalibrationState _calibrationState = CalibrationState::CAL_NONE;
    uint32_t _calibratedDriveOpenTime = 0;
    uint32_t _calibratedDriveCloseTime = 0;

    // 0 % = fully closed, 1 = fully open
    float _currentPositionPercent = HTA_POSITION_INVALID;
    float _targetPositionPercent = HTA_POSITION_INVALID;

    bool _externEnforcedPosition = false;

    float _externSetValuePercent = HTA_POSITION_INVALID;
    float _externRoomTemp = HTA_TEMPERATUR_INVALID;
    uint32_t _lastExternValue = 0;

    float _externTargetTemp = HTA_TEMPERATUR_INVALID;
    float _externTargetTempShift = 0;

    float _currentTargetTemp = HTA_TEMPERATUR_INVALID;
    PID_RT pid;

    bool _currentOperationModeHeating = true;
    HvacMode _currentHvacMode = HvacMode::HVAC_NONE;
    bool _currentManualMode = false;
    bool _currentManualModeOn = false;
    uint32_t _currentManualModeStarted = 0;

    bool _currentButtonPressed = false;
    uint32_t _currentButtonPressedStarted = 0;

    bool _currentLedOn = false;
    uint32_t _currentLedOnTime = 0;
    uint32_t _currentLedChangeStarted = 0;

    std::string _lastDebugLogMessage = "";
};