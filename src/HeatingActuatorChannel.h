#pragma once
#include "OpenKNX.h"

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
    void startCalibration();
    void moveValveToPosition(float targetPositionPercent);

    void savePower();
    bool restorePower();

    enum MotorState
    {
        MOT_IDLE,
        MOT_OPENING,
        MOT_CLOSING
    };

    enum CalibrationState
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
    const std::string name() override;

    bool _statusDuringLock;
    uint32_t _statusCyclicSendTimer = 0;

    uint32_t _motorStarted = 0;
    uint32_t _motorStopped = 0;

    uint32_t _calibratedDriveOpenTime = 0;
    uint32_t _calibratedDriveCloseTime = 0;

    // 0 % = fully closed, 1 = fully open
    float _currentPositionPercent = 0;
    float _targetPositionPercent = 0;
};