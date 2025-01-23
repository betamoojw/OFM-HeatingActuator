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

    void runMotor();
    void stopMotor();
    void startCalibration();

    void savePower();
    bool restorePower();

    enum CalibrationState
    {
        NONE,
        INIT,
        CLOSING,
        OPENING,
        COMPLETE,
        ERROR
    };

    CalibrationState calibrationState = CalibrationState::NONE;
    uint32_t calibratedDriveOpenTime = 0;
    uint32_t calibratedDriveCloseTime = 0;

    // 0 % = fully closed, 100 % = fully open
    uint8_t currentPositionPercent = 0;
    uint8_t targetPositionPercent = 0;

  protected:

  private:
    const std::string name() override;

    bool _statusDuringLock;
    uint32_t _statusCyclicSendTimer = 0;

    uint32_t _motorStarted = 0;
    uint32_t _motorStopped = 0;
};