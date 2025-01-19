#include "HeatingActuatorModule.h"
#include "OpenKNX.h"
#include "ModuleVersionCheck.h"

HeatingActuatorModule openknxHeatingActuatorModule;

HeatingActuatorModule::HeatingActuatorModule()
{
}

HeatingActuatorModule::~HeatingActuatorModule()
{
}

const std::string HeatingActuatorModule::name()
{
    return "Heating";
}

const std::string HeatingActuatorModule::version()
{
    return MODULE_HeatingActuator_Version;
}

void HeatingActuatorModule::processInputKo(GroupObject &iKo)
{
    // if (iKo.asap() != SWA_KoCentralFunction &&
    //     (iKo.asap() < SWA_KoBlockOffset ||
    //      iKo.asap() > SWA_KoBlockOffset + ParamSWA_VisibleChannels * SWA_KoBlockSize - 1))
    //     return;

    // logDebugP("processInputKo");
    // logIndentUp();

    // for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_HTA_MOT_COUNT); i++)
    //     channel[i]->processInputKo(iKo);

    // logIndentDown();
}

void HeatingActuatorModule::setup(bool configured)
{
    for (uint8_t i = 0; i < OPENKNX_HTA_MOT_COUNT; i++)
    {
        openknxGPIOModule.pinMode(0x0100 + i, OUTPUT);
        openknxGPIOModule.digitalWrite(0x0100 + i, LOW);

        openknxGPIOModule.pinMode(0x0200 + i, INPUT);
    }

#ifdef OPENKNX_GPIO_WIRE
    if (ina.begin())
    {
        logDebugP("INA219 setup done with address %u", ina.getAddress());

        ina.setBusVoltageRange(16);
        ina.setGain(1);
        ina.setMaxCurrentShunt(0.5, 0.1);
        ina.setModeShuntContinuous();
        delay(1000);

        logDebugP("getBusVoltageRange %u", ina.getBusVoltageRange());
        logDebugP("getGain %u", ina.getGain());
        logDebugP("getBusADC %u", ina.getBusADC());
        logDebugP("getShuntADC %u", ina.getShuntADC());
        logDebugP("getMode %u", ina.getMode());

        logDebugP("isCalibrated %u", ina.isCalibrated());
        logDebugP("getCurrentLSB %.4f", ina.getCurrentLSB());
        logDebugP("getShunt %.4f", ina.getShunt());
        logDebugP("getMaxCurrent %.4f", ina.getMaxCurrent());
    }
    else
        logDebugP("INA219 not found at address %u", ina.getAddress());
#endif

    pinMode(OPENKNX_HTA_MOT_PWR_PIN, OUTPUT);
    digitalWrite(OPENKNX_HTA_MOT_PWR_PIN, MOT_PWR_OFF);

    pinMode(OPENKNX_HTA_MOT_HIGH1_PIN, OUTPUT);
    digitalWrite(OPENKNX_HTA_MOT_HIGH1_PIN, MOT_HIGH1_OFF);
    pinMode(OPENKNX_HTA_MOT_HIGH2_PIN, OUTPUT);
    digitalWrite(OPENKNX_HTA_MOT_HIGH2_PIN, MOT_HIGH2_OFF);
    pinMode(OPENKNX_HTA_MOT_LOW1_PIN, OUTPUT);
    digitalWrite(OPENKNX_HTA_MOT_LOW1_PIN, MOT_LOW1_OFF);
    pinMode(OPENKNX_HTA_MOT_LOW2_PIN, OUTPUT);
    digitalWrite(OPENKNX_HTA_MOT_LOW2_PIN, MOT_LOW2_OFF);

    for (uint8_t i = 0; i < OPENKNX_HTA_MOT_COUNT; i++)
    {
        channel[i] = new HeatingActuatorChannel(i);
        channel[i]->setup(configured);
    }
}

void HeatingActuatorModule::loop()
{
    //for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT); i++)
    for (uint8_t i = 0; i < OPENKNX_HTA_MOT_COUNT; i++)
        channel[i]->loop();

    if (_motorRunning)
    {
        _currentAvg -= _currentAvg / 10;
        _currentAvg += ina.getCurrent_mA() / 10;
        _currentCount++;

        if (_currentCount >= 10) 
        {
            if (_currentAvg > OPENKNX_HTA_CURRENT_MOT_MAX_LIMIT)
            {
                logDebugP("STOP MAX: current: %.2f", _currentAvg);
                stopMotor();
            }

            if (_currentCount % 10 == 0)
            {
                if (delayCheck(_debugOutputTimer, 1000)) 
                {
                    logDebugP("current: %.2f, last: %.2f", _currentAvg, _currentAvgLast);
                    _debugOutputTimer = delayTimerInit();
                }

                if (_currentCount >= 100 &&
                    _currentAvgLast > 0)
                {
                    if (_currentAvg > _currentAvgLast &&
                        ((!_motorRunningCcw && (_currentAvg > OPENKNX_HTA_CURRENT_MOT_CW_LIMIT)) ||
                        (_motorRunningCcw && (_currentAvg > OPENKNX_HTA_CURRENT_MOT_CCW_LIMIT))))
                    {
                        logDebugP("STOP: current: %.2f, last: %.2f, limit: %.2f", _currentAvg, _currentAvgLast, _motorRunningCcw ? OPENKNX_HTA_CURRENT_MOT_CCW_LIMIT : OPENKNX_HTA_CURRENT_MOT_CW_LIMIT);
                        stopMotor();
                    }
                }

                _currentAvgLast = _currentAvg;
            }
        }
    }

    for (uint8_t i = 0; i < OPENKNX_HTA_MOT_COUNT; i++)
    {
        if (openknxGPIOModule.digitalRead(0x0200 + i) == LOW)
            openknxGPIOModule.digitalWrite(0x0100 + i, HIGH);
    }

// #ifdef OPENKNX_SWA_IO_TCA_ADDR
//     uint8_t channelIndex = 0;
//     for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT); i++)
//     {
//         channelIndex = 7 - i;
//         if (delayCheck(chSwitchLastTrigger[channelIndex], CH_SWITCH_DEBOUNCE) && tca.read1(i + 8))
//         {
//             chSwitchLastTrigger[channelIndex] = delayTimerInit();
//             channel[channelIndex]->doSwitch(!channel[channelIndex]->isRelayActive());
//         }
//     }

//     for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT); i++)
//         tca.write1(i, channel[i]->isRelayActive());
// #endif
}

void HeatingActuatorModule::runMotor(uint8_t channelIndex, bool ccw)
{
    stopMotor();

    logDebugP("Run motor index %u, ccw: %u", channelIndex, ccw);

    if (!ccw)
    {
        digitalWrite(OPENKNX_HTA_MOT_HIGH1_PIN, MOT_HIGH1_ON);
        digitalWrite(OPENKNX_HTA_MOT_HIGH2_PIN, MOT_HIGH2_OFF);
        digitalWrite(OPENKNX_HTA_MOT_LOW1_PIN, MOT_LOW1_OFF);
        digitalWrite(OPENKNX_HTA_MOT_LOW2_PIN, MOT_LOW2_ON);
    }
    else
    {
        digitalWrite(OPENKNX_HTA_MOT_HIGH1_PIN, MOT_HIGH1_OFF);
        digitalWrite(OPENKNX_HTA_MOT_HIGH2_PIN, MOT_HIGH2_ON);
        digitalWrite(OPENKNX_HTA_MOT_LOW1_PIN, MOT_LOW1_ON);
        digitalWrite(OPENKNX_HTA_MOT_LOW2_PIN, MOT_LOW2_OFF);
    }

    channel[channelIndex]->runMotor();
    digitalWrite(OPENKNX_HTA_MOT_PWR_PIN, MOT_PWR_ON);

    _currentCount = 0;
    _currentAvg = 0;
    _currentAvgLast = 0;
    _motorRunningCcw = ccw;
    _motorRunning = true;
}

void HeatingActuatorModule::stopMotor()
{
    logDebugP("Stop motor");

    _motorRunning = false;

    digitalWrite(OPENKNX_HTA_MOT_PWR_PIN, MOT_PWR_OFF);

    for (uint8_t i = 0; i < OPENKNX_HTA_MOT_COUNT; i++)
        channel[i]->stopMotor();
}

void HeatingActuatorModule::readFlash(const uint8_t *data, const uint16_t size)
{
    // if (size == 0)
    //     return;

    // logDebugP("Reading state from flash");
    // logIndentUp();

    // uint8_t version = openknx.flash.readByte();
    // if (version != OPENKNX_SWA_FLASH_VERSION)
    // {
    //     logDebugP("Invalid flash version %u", version);
    //     return;
    // }

    // uint32_t magicWord = openknx.flash.readInt();
    // if (magicWord != OPENKNX_SWA_FLASH_MAGIC_WORD)
    // {
    //     logDebugP("Flash content invalid");
    //     return;
    // }

    // uint8_t relayChannelsStored = openknx.flash.readByte();

    // uint8_t byteValue = 0;
    // for (uint8_t i = 0; i < MIN(relayChannelsStored, MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT)); i++)
    // {
    //     uint8_t bitIndex = i % 8;
    //     if (bitIndex == 0)
    //         byteValue = openknx.flash.readByte();

    //     channel[i]->doSwitch(bitRead(byteValue, bitIndex), false);
    // }
    
    // logIndentDown();
}

void HeatingActuatorModule::writeFlash()
{
    // openknx.flash.writeByte(OPENKNX_SWA_FLASH_VERSION);
    // openknx.flash.writeInt(OPENKNX_SWA_FLASH_MAGIC_WORD);

    // openknx.flash.writeByte(MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT));

    // uint8_t byteValue = 0;
    // for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT); i++)
    // {
    //     uint8_t bitIndex = i % 8;
    //     if (bitIndex == 0)
    //     {
    //         if (i > 0)
    //             openknx.flash.writeByte(byteValue);

    //         byteValue = 0;
    //     }

    //     bitWrite(byteValue, bitIndex, channel[i]->isRelayActive());
    // }
    // openknx.flash.writeByte(byteValue);

    // logDebugP("Relays state written to flash");
}

uint16_t HeatingActuatorModule::flashSize()
{
    //return 6 + ceil(MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT) / 8.0);
    return 0;
}

void HeatingActuatorModule::savePower()
{
    // for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT); i++)
    //     channel[i]->savePower();
}

bool HeatingActuatorModule::restorePower()
{
    bool success = true;
    // for (uint8_t i = 0; i < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT); i++)
    //     success &= channel[i]->restorePower();
    
    return success;
}

void HeatingActuatorModule::showHelp()
{
    openknx.console.printHelpLine("hta mot NN cw", "Turn motor with channel index NN (zero-based) clockwise.");
    openknx.console.printHelpLine("hta mot NN ccw", "Turn motor with channel index NN (zero-based) counter-clockwise.");
    openknx.console.printHelpLine("hta mot stop", "Stop motor.");
}

bool HeatingActuatorModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    bool result = false;

    if (cmd.substr(0, 3) != "hta" || cmd.length() < 5)
        return result;

    if (cmd.length() == 5 && cmd.substr(4, 1) == "h")
    {
        openknx.console.writeDiagenoseKo("-> mot NN cw");
        openknx.console.writeDiagenoseKo("");
        openknx.console.writeDiagenoseKo("-> mot NN ccw");
        openknx.console.writeDiagenoseKo("");
        openknx.console.writeDiagenoseKo("-> mot stop");
        openknx.console.writeDiagenoseKo("");
    }
    else if (cmd.length() > 8 && cmd.substr(4, 3) == "mot")
    {
        if (cmd.length() == 13 && cmd.substr(11, 2) == "cw")
        {
            uint8_t channelIndex = stoi(cmd.substr(8, 2));
            runMotor(channelIndex, false);
            result = true;
        }
        if (cmd.length() == 14 && cmd.substr(11, 3) == "ccw")
        {
            uint8_t channelIndex = stoi(cmd.substr(8, 2));
            runMotor(channelIndex, true);
            result = true;
        }
        if (cmd.length() == 12 && cmd.substr(8, 4) == "stop")
        {
            stopMotor();
            result = true;
        }
    }

    return result;
}