#pragma once
#include "OpenKNX.h"
#include "HeatingActuatorChannel.h"
#include "hardware.h"
#include "knxprod.h"
#include "INA219.h"

#ifdef AB_HTA_OUT_TCA1_ADDR
  #include "TCA9555.h"
#endif

#define OPENKNX_SWA_FLASH_VERSION 0
#define OPENKNX_SWA_FLASH_MAGIC_WORD 3441922009

#define CH_SWITCH_DEBOUNCE 250

const uint8_t MOTOR_PINS[OPENKNX_HTA_GPIO_COUNT] = {OPENKNX_HTA_GPIO_PINS};

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

    void runMotor(uint8_t channelIndex, bool ccw);
    void stopMotor();
    void doSwitchChannel(uint8_t channelIndex, bool active, bool syncSwitch = true);

    void writeFlash() override;
    void readFlash(const uint8_t* data, const uint16_t size) override;
    uint16_t flashSize() override;

    void savePower() override;
    bool restorePower() override;
    bool processCommand(const std::string cmd, bool diagnoseKo);

    const std::string name() override;
    const std::string version() override;

  private:
    HeatingActuatorChannel *channel[SWA_ChannelCount];
    uint32_t chSwitchLastTrigger[8] = {};

#ifdef AB_HTA_OUT_TCA1_ADDR
    TCA9555 tca = TCA9555(AB_HTA_OUT_TCA1_ADDR, &AB_HTA_I2C_WIRE);
#endif

#ifdef AB_HTA_CUR_INA_ADDR
    INA219 ina = INA219(AB_HTA_CUR_INA_ADDR, &AB_HTA_I2C_WIRE);
#endif
};

extern HeatingActuatorModule openknxHeatingActuatorModule;