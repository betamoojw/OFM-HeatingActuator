#pragma once
#include "OpenKNX.h"

#define POSITION_INVALID -1
#define TEMPERATUR_INVALID -127

#define MOT_RESTART_DELAY 1000 // avoid power spikes, especially when changing motor direction

#define HTA_CONTROL_MODE_EXTERN 0
#define HTA_CONTROL_MODE_INTERN 1
#define HTA_OPERATION_MODE_HEATING 0
#define HTA_OPERATION_MODE_COOLING 1
#define HTA_OPERATION_MODE_HEATCOOLING 2

class HeatingActuatorChannel : public OpenKNX::Channel
{
  public:
    HeatingActuatorChannel(uint8_t channelNumber);
    ~HeatingActuatorChannel();

    void processInputKo(GroupObject &ko);
    void setup(bool configured);
    void loop(bool motorPower, float current);

    void runMotor(bool open);
    void stopMotor();
    uint8_t getMotorMaxCurrent(bool open);
    void startCalibration();
    void moveValveToPosition(float targetPositionPercent);

    void savePower();
    bool restorePower();
    void writeChannelData();
    void readChannelData();

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

    MotorState motorState = MotorState::MOT_IDLE;
    CalibrationState calibrationState = CalibrationState::CAL_NONE;
  protected:

  private:
    enum HvacMode : uint8_t
    {
        HVAC_NONE,
        HVAC_COMFORT,
        HVAC_STANDBY,
        HVAC_NIGHT,
        HVAC_PROTECT
    };

    const std::string name() override;
    void checkHvacMode();
    void checkTargetTempShift(float newTargetTempShift);

    uint32_t _setValueCyclicSendTimer = 0;
    uint32_t _targetTempCyclicSendTimer = 0;
    uint32_t _manualModeCyclicSendTimer = 0;

    uint32_t _motorStarted = 0;
    uint32_t _motorStopped = 0;

    uint32_t _calibratedDriveOpenTime = 0;
    uint32_t _calibratedDriveCloseTime = 0;

    // 0 % = fully closed, 1 = fully open
    float _currentPositionPercent = POSITION_INVALID;
    float _targetPositionPercent = POSITION_INVALID;

    bool _externEnforcedPosition = false;

    float _externSetValuePercent = POSITION_INVALID;
    float _externRoomTemp = TEMPERATUR_INVALID;
    uint32_t _lastExternValue = 0;

    float _externTargetTemp = TEMPERATUR_INVALID;
    float _externTargetTempShift = TEMPERATUR_INVALID;

    float _currentTargetTemp = TEMPERATUR_INVALID;
    HvacMode _currentHvacMode = HvacMode::HVAC_NONE;
    bool _currentManualMode = false;
};