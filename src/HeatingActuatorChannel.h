#pragma once
#include "OpenKNX.h"

class HeatingActuatorChannel : public OpenKNX::Channel
{
  public:
    HeatingActuatorChannel(uint8_t channelNumber);
    ~HeatingActuatorChannel();

    void processInputKo(GroupObject &ko);
    void setup(bool configured);
    void loop();

    void runMotor();
    void stopMotor();
    void savePower();
    bool restorePower();

  protected:

  private:
    const std::string name() override;
    bool _statusDuringLock;
    uint32_t _statusCyclicSendTimer = 0;
};