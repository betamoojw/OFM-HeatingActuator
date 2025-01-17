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

    void doSwitchInternal(bool active, bool syncSwitch = true);
    void processSwitchInput(bool newActive);
    void relaisOff();

  protected:

  public:
    HeatingActuatorChannel(uint8_t iChannelNumber);
    ~HeatingActuatorChannel();

    void processInputKo(GroupObject &iKo);
    void setup(bool configured);
    void loop();

    void runMotor();
    void stopMotor();
    void doSwitch(bool active, bool syncSwitch = true);
    bool isRelayActive();
    void savePower();
    bool restorePower();
};