#pragma once
#include "OpenKNX.h"

class HeatingActuatorChannel : public OpenKNX::Channel
{
  private:
    const std::string name() override;
    bool statusDuringLock;
    uint32_t statusCyclicSendTimer = 0;
    uint32_t relayBistableImpulsTimer = 0;
    uint32_t turnOnDelayTimer = 0;
    uint32_t turnOffDelayTimer = 0;

  protected:

  public:
    HeatingActuatorChannel(uint8_t iChannelNumber);
    ~HeatingActuatorChannel();

    void processInputKo(GroupObject &iKo);
    void setup(bool configured);
    void loop();

    void runMotor();
    void stopMotor();
    void savePower();
    bool restorePower();
};