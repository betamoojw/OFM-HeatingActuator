#pragma once
#include "OpenKNX.h"
#include "HeatingActuatorChannel.h"
#include "hardware.h"
#include "knxprod.h"

#ifdef OPENKNX_SWA_IO_TCA_WIRE
  #include "TCA9555.h"
#endif

#define OPENKNX_SWA_FLASH_VERSION 0
#define OPENKNX_SWA_FLASH_MAGIC_WORD 3441922009

#define CH_SWITCH_DEBOUNCE 250

#ifdef OPENKNX_HTA_GPIO_PINS
  const uint8_t RELAY_SET_PINS[OPENKNX_HTA_GPIO_COUNT] = {OPENKNX_HTA_GPIO_PINS};
#else
  const uint8_t RELAY_SET_PINS[OPENKNX_HTA_GPIO_COUNT] = {};
#endif

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
#define MOT_ON        OPENKNX_HTA_ONLEVEL == HIGH ? HIGH : LOW
#define MOT_OFF       OPENKNX_HTA_ONLEVEL == HIGH ? LOW : HIGH

class HeatingActuatorModule : public OpenKNX::Module
{
  public:
    HeatingActuatorModule();
    ~HeatingActuatorModule();

    void processInputKo(GroupObject &iKo);
    void setup(bool configured);
    void loop();

    void doSwitchChannel(uint8_t channelIndex, bool active, bool syncSwitch = true);

    void writeFlash() override;
    void readFlash(const uint8_t* data, const uint16_t size) override;
    uint16_t flashSize() override;

    void savePower() override;
    bool restorePower() override;

    const std::string name() override;
    const std::string version() override;

  private:
    HeatingActuatorChannel *channel[SWA_ChannelCount];
    uint32_t chSwitchLastTrigger[8] = {};

#ifdef OPENKNX_SWA_IO_TCA_WIRE
    TCA9555 tca = TCA9555(OPENKNX_SWA_IO_TCA_ADDR, &OPENKNX_SWA_IO_TCA_WIRE);
#endif
};

extern HeatingActuatorModule openknxHeatingActuatorModule;