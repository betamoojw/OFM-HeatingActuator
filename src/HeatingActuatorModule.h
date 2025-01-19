#pragma once
#include "OpenKNX.h"
#include "GPIOModule.h"
#include "HeatingActuatorChannel.h"
#include "hardware.h"
#include "knxprod.h"
#include "INA219.h"

#define OPENKNX_SWA_FLASH_VERSION 0
#define OPENKNX_SWA_FLASH_MAGIC_WORD 3441922009

#define CH_SWITCH_DEBOUNCE 250

const uint8_t MOTOR_PINS[OPENKNX_HTA_MOT_COUNT] = {OPENKNX_HTA_MOT_PINS};

#define MOT_PWR_ON    OPENKNX_HTA_MOT_PWR_PIN_ACTIVE_ON == HIGH ? HIGH : LOW
#define MOT_PWR_OFF   OPENKNX_HTA_MOT_PWR_PIN_ACTIVE_ON == HIGH ? LOW : HIGH
#define MOT_HIGH1_ON  OPENKNX_HTA_MOT_HIGH1_PIN_ACTIVE_ON == HIGH ? HIGH : LOW
#define MOT_HIGH1_OFF OPENKNX_HTA_MOT_HIGH1_PIN_ACTIVE_ON == HIGH ? LOW : HIGH
#define MOT_HIGH2_ON  OPENKNX_HTA_MOT_HIGH2_PIN_ACTIVE_ON == HIGH ? HIGH : LOW
#define MOT_HIGH2_OFF OPENKNX_HTA_MOT_HIGH2_PIN_ACTIVE_ON == HIGH ? LOW : HIGH
#define MOT_LOW1_ON   OPENKNX_HTA_MOT_LOW1_PIN_ACTIVE_ON == HIGH ? HIGH : LOW
#define MOT_LOW1_OFF  OPENKNX_HTA_MOT_LOW1_PIN_ACTIVE_ON == HIGH ? LOW : HIGH
#define MOT_LOW2_ON   OPENKNX_HTA_MOT_LOW2_PIN_ACTIVE_ON == HIGH ? HIGH : LOW
#define MOT_LOW2_OFF  OPENKNX_HTA_MOT_LOW2_PIN_ACTIVE_ON == HIGH ? LOW : HIGH
#define MOT_ON        OPENKNX_HTA_ACTIVE_ON == HIGH ? HIGH : LOW
#define MOT_OFF       OPENKNX_HTA_ACTIVE_ON == HIGH ? LOW : HIGH

class HeatingActuatorModule : public OpenKNX::Module
{
  public:
    HeatingActuatorModule();
    ~HeatingActuatorModule();

    void processInputKo(GroupObject &iKo);
    void setup(bool configured);
    void loop();

    void runMotor(uint8_t channelIndex, bool ccw);
    void stopMotor();

    void writeFlash() override;
    void readFlash(const uint8_t* data, const uint16_t size) override;
    uint16_t flashSize() override;

    void savePower() override;
    bool restorePower() override;
    void showHelp() override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;

    const std::string name() override;
    const std::string version() override;

  private:
    HeatingActuatorChannel *channel[OPENKNX_HTA_MOT_COUNT];
    uint32_t chSwitchLastTrigger[OPENKNX_HTA_MOT_COUNT] = {};

    bool _motorRunning = false;
    bool _motorRunningCcw = false;
    float _currentAvg = 0;
    float _currentAvgLast = 0;
    uint32_t _currentCount = 0;

    uint32_t _debugOutputTimer = 0;

#ifdef OPENKNX_HTA_CURRENT_INA_ADDR
    INA219 ina = INA219(OPENKNX_HTA_CURRENT_INA_ADDR, &OPENKNX_GPIO_WIRE);
#endif
};

extern HeatingActuatorModule openknxHeatingActuatorModule;