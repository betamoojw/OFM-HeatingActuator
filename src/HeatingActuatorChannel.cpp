#include "HeatingActuatorChannel.h"
#include "HeatingActuatorModule.h"

HeatingActuatorChannel::HeatingActuatorChannel(uint8_t iChannelNumber)
{
    _channelIndex = iChannelNumber;
}

HeatingActuatorChannel::~HeatingActuatorChannel() {}

void HeatingActuatorChannel::processInputKo(GroupObject &iKo)
{
    if (ParamSWA_ChActive != 1)
    {
        logTraceP("processInputKo: channel not active (%u)", ParamSWA_ChActive);
        return;
    }

    logDebugP("processInputKo: channel %u", _channelIndex);
    logIndentUp();

    bool newActive;
    switch (iKo.asap())
    {
        case SWA_KoCentralFunction:
            if (ParamSWA_ChCentralFunction)
            {
                newActive = iKo.value(DPT_Switch);
                logDebugP("SWA_KoCentralFunction: %u", newActive);

                processSwitchInput(newActive);
            }
            break;
    }

    switch (SWA_KoCalcIndex(iKo.asap()))
    {
        case SWA_KoChSwitch:
            newActive = iKo.value(DPT_Switch);
            logDebugP("SWA_KocSwitch: %u", newActive);

            processSwitchInput(newActive);
            break;
        case SWA_KoChLock:
            newActive = iKo.value(DPT_Switch);
            logDebugP("SWA_KocLock: %u", newActive);

            if (newActive == (bool)KoSWA_ChLockStatus.value(DPT_Switch))
            {
                logDebugP("New value equals current status");
                break;
            }

            KoSWA_ChLockStatus.value(newActive, DPT_Switch);

            if (newActive)
            {
                statusDuringLock = (bool)KoSWA_ChStatus.value(DPT_Switch);

                if (ParamSWA_ChBehaviorLock > 0)
                    doSwitch(ParamSWA_ChBehaviorLock == 2);
            }
            else
            {
                switch (ParamSWA_ChBehaviorUnlock)
                {
                    case 1:
                        doSwitch(false);
                        break;
                    case 2:
                        doSwitch(true);
                        break;
                    case 3:
                    case 4:
                        doSwitch(statusDuringLock);
                        break;
                }
            }

            break;
    }

    logIndentDown();
}

void HeatingActuatorChannel::processSwitchInput(bool newActive)
{
    if (KoSWA_ChLockStatus.value(DPT_Switch))
    {
        logDebugP("Channel is locked");

        if (ParamSWA_ChBehaviorUnlock == 3)
        {
            statusDuringLock = newActive;
            logDebugP("statusDuringLock: %u", newActive);
        }

        return;
    }

    if (newActive == (bool)KoSWA_ChStatus.value(DPT_Switch))
    {
        logDebugP("New value equals current status");
        return;
    }

    doSwitch(newActive);
}

void HeatingActuatorChannel::doSwitch(bool active, bool syncSwitch)
{
    if (active && ParamSWA_ChTurnOnDelayTimeMS > 0)
    {
        turnOnDelayTimer = delayTimerInit();

        // if switch on during switch off delay, we won't turn off anymore
        turnOffDelayTimer = 0;
    }
    else if (!active && ParamSWA_ChTurnOffDelayTimeMS > 0)
    {
        turnOffDelayTimer = delayTimerInit();

        // if switch off during switch on delay, we won't turn on anymore
        turnOnDelayTimer = 0;
    }
    else
        doSwitchInternal(active, syncSwitch);
}

void HeatingActuatorChannel::doSwitchInternal(bool active, bool syncSwitch)
{
    if (ParamSWA_ChActive != 1)
    {
        logDebugP("Channel not active (%u)", ParamSWA_ChActive);
        return;
    }

    if (_channelIndex >= OPENKNX_SWA_CHANNEL_COUNT)
    {
        logErrorP("Channel index %u is too high for hardware channel count %u", _channelIndex, OPENKNX_SWA_CHANNEL_COUNT);
        return;
    }

    // if current channel should be switch in sync with other channel
    // ignore current channel switch and switch main channel instead
    if (syncSwitch && ParamSWA_ChSyncSwitch == 1)
    {
        if ((_channelIndex) % 3 == 1)
            openknxHeatingActuatorModule.doSwitchChannel(_channelIndex - 1, active);
        else if ((_channelIndex) % 3 == 2)
            openknxHeatingActuatorModule.doSwitchChannel(_channelIndex - 2, active);

        return;
    }

    // ensure both coils are off first
    relaisOff();

    // invert in case of operation mode is opening contact
    bool activeSet = ParamSWA_ChOperationMode ? !active : active;
    if (activeSet)
    {
        logDebugP("Write relay state activeSet=%u to GPIO %u with value %u", activeSet, RELAY_SET_PINS[_channelIndex], RELAY_GPIO_SET_ON);
        digitalWrite(RELAY_SET_PINS[_channelIndex], RELAY_GPIO_SET_ON);
    }
    else
    {
        logDebugP("Write relay state activeSet=%u to GPIO %u with value %u", activeSet, RELAY_RESET_PINS[_channelIndex], RELAY_GPIO_RESET_ON);
        digitalWrite(RELAY_RESET_PINS[_channelIndex], RELAY_GPIO_RESET_ON);
    }
    relayBistableImpulsTimer = delayTimerInit();

    logDebugP("Set channel status %u", active);
    KoSWA_ChStatus.value(active, DPT_Switch);

    if (ParamSWA_ChStatusInverted)
    {
        logDebugP("Set channel inverted status %u", !active);
        KoSWA_ChStatusInverted.value(!active, DPT_Switch);
    }

    // every third channel can be a sync main channel the next two channels depend on if sync is enabled
    if (syncSwitch && ((_channelIndex) % 3 == 0))
    {
        uint8_t channelUp1Index = _channelIndex + 1;
        if (channelUp1Index < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT))
        {
            bool syncChannelUp1 = (bool)(knx.paramByte(SWA_ChSyncSwitch + SWA_ParamBlockOffset + channelUp1Index * SWA_ParamBlockSize) & SWA_ChSyncSwitchMask);
            if (syncChannelUp1)
                openknxHeatingActuatorModule.doSwitchChannel(channelUp1Index, active, false);
        }

        uint8_t channelUp2Index = _channelIndex + 2;
        if (channelUp2Index < MIN(ParamSWA_VisibleChannels, OPENKNX_SWA_CHANNEL_COUNT))
        {
            bool syncChannelUp2 = (bool)(knx.paramByte(SWA_ChSyncSwitch + SWA_ParamBlockOffset + channelUp2Index * SWA_ParamBlockSize) & SWA_ChSyncSwitchMask);
            if (syncChannelUp2)
                openknxHeatingActuatorModule.doSwitchChannel(channelUp2Index, active, false);
        }
    }
}

void HeatingActuatorChannel::loop()
{
    if (relayBistableImpulsTimer > 0 && delayCheck(relayBistableImpulsTimer, OPENKNX_SWA_BISTABLE_IMPULSE_LENGTH))
    {
        relaisOff();
        relayBistableImpulsTimer = 0;
    }

    if (turnOnDelayTimer > 0 && delayCheck(turnOnDelayTimer, ParamSWA_ChTurnOnDelayTimeMS))
    {
        doSwitchInternal(true);
        turnOnDelayTimer = 0;
    }
    if (turnOffDelayTimer > 0 && delayCheck(turnOffDelayTimer, ParamSWA_ChTurnOffDelayTimeMS))
    {
        doSwitchInternal(false);
        turnOffDelayTimer = 0;
    }

    if (statusCyclicSendTimer > 0 && delayCheck(statusCyclicSendTimer, ParamSWA_ChStatusCyclicTimeMS))
    {
        KoSWA_ChStatus.objectWritten();
        statusCyclicSendTimer = delayTimerInit();
    }
}

void HeatingActuatorChannel::setup(bool configured)
{
    // preset PIN state before changing PIN mode
    digitalWriteFast(RELAY_SET_PINS[_channelIndex], RELAY_GPIO_SET_OFF);
    digitalWriteFast(RELAY_RESET_PINS[_channelIndex], RELAY_GPIO_RESET_OFF);

    pinMode(RELAY_SET_PINS[_channelIndex], OUTPUT);
    pinMode(RELAY_RESET_PINS[_channelIndex], OUTPUT);

    // set it again the standard way, just in case
    relaisOff();

    if (configured)
    {
        if (ParamSWA_ChStatusCyclicTimeMS > 0)
            statusCyclicSendTimer = delayTimerInit();
    }
}

void HeatingActuatorChannel::relaisOff()
{
    logDebugP("Write relay state off to GPIO %u with value %u", RELAY_SET_PINS[_channelIndex], RELAY_GPIO_SET_OFF);
    digitalWrite(RELAY_SET_PINS[_channelIndex], RELAY_GPIO_SET_OFF);

    logDebugP("Write relay state off to GPIO %u with value %u", RELAY_RESET_PINS[_channelIndex], RELAY_GPIO_RESET_OFF);
    digitalWrite(RELAY_RESET_PINS[_channelIndex], RELAY_GPIO_RESET_OFF);
}

bool HeatingActuatorChannel::isRelayActive()
{
    return (bool)KoSWA_ChStatus.value(DPT_Switch);
}

void HeatingActuatorChannel::savePower()
{
    if (ParamSWA_ChBehaviorPowerLoss > 0)
        doSwitchInternal(ParamSWA_ChBehaviorPowerLoss == 2);
}

bool HeatingActuatorChannel::restorePower()
{
    if (ParamSWA_ChBehaviorPowerRegain > 0)
        doSwitchInternal(ParamSWA_ChBehaviorPowerRegain == 2);

    return true;
}

const std::string HeatingActuatorChannel::name()
{
    return "SwitchChannel";
}