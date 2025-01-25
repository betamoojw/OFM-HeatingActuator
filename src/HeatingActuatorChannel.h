#pragma once
#include "OpenKNX.h"

#define POSITION_INVALID -1

#define MOT_RESTART_DELAY 1000 // avoid power spikes, especially when changing motor direction

class HeatingActuatorChannel : public OpenKNX::Channel
{
  public:
    HeatingActuatorChannel(uint8_t channelNumber);
    ~HeatingActuatorChannel();

    void processInputKo(GroupObject &ko);
    void setup(bool configured);
    void loop(float current);

    void runMotor(bool open);
    void stopMotor();
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
    const std::string name() override;

    bool _statusDuringLock;
    uint32_t _statusCyclicSendTimer = 0;

    uint32_t _motorStarted = 0;
    uint32_t _motorStopped = 0;

    uint32_t _calibratedDriveOpenTime = 0;
    uint32_t _calibratedDriveCloseTime = 0;

    // 0 % = fully closed, 1 = fully open
    float _currentPositionPercent = POSITION_INVALID;
    float _targetPositionPercent = POSITION_INVALID;
};