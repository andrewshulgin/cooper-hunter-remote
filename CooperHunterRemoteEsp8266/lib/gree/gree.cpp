#include "gree.h"
#include <algorithm>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>
#include <ir_Kelvinator.h>

const uint16_t kGreeHdrMark = 9000;
const uint16_t kGreeHdrSpace = 4500; // See #684 and real example in unit tests
const uint16_t kGreeBitMark = 620;
const uint16_t kGreeOneSpace = 1600;
const uint16_t kGreeZeroSpace = 540;
const uint16_t kGreeMsgSpace = 19000;
const uint8_t kGreeBlockFooter = 0b010;
const uint8_t kGreeBlockFooterBits = 3;

Gree::Gree(uint16_t pin) : _irsend(pin, true, false) { _pin = pin; stateReset(); }

void Gree::stateReset()
{
    // This resets to a known-good state to Power Off, Fan Auto, Mode Auto, 25C.
    for (uint8_t i = 0; i < kGreeStateLength; i++)
        remote_state[i] = 0x0;
    remote_state[1] = 0x09;
    remote_state[2] = 0x20;
    remote_state[3] = 0x50;
    remote_state[5] = 0x20;
    remote_state[7] = 0x50;
}

void Gree::fixup()
{
    checksum(); // Calculate the checksums
}

void Gree::begin() { _irsend.begin(); }

#if SEND_GREE
void Gree::send(const uint16_t repeat)
{
    fixup(); // Ensure correct settings before sending.
    _irsend.sendGree(remote_state, kGreeStateLength, repeat);
}
#endif // SEND_GREE

uint8_t *Gree::getRaw()
{
    fixup(); // Ensure correct settings before sending.
    return remote_state;
}

void Gree::setRaw(uint8_t new_code[])
{
    for (uint8_t i = 0; i < kGreeStateLength; i++)
    {
        remote_state[i] = new_code[i];
    }
}

void Gree::checksum(const uint16_t length)
{
    // Gree uses the same checksum alg. as Kelvinator's block checksum.
    uint8_t sum = IRKelvinatorAC::calcBlockChecksum(remote_state, length);
    remote_state[length - 1] = (sum << 4) | (remote_state[length - 1] & 0xFU);
}

// Verify the checksum is valid for a given state.
// Args:
//   state:  The array to verify the checksum of.
//   length: The size of the state.
// Returns:
//   A boolean.
bool Gree::validChecksum(const uint8_t state[], const uint16_t length)
{
    // Top 4 bits of the last byte in the state is the state's checksum.
    if (state[length - 1] >> 4 ==
        IRKelvinatorAC::calcBlockChecksum(state, length))
        return true;
    else
        return false;
}

void Gree::on()
{
    remote_state[0] |= kGreePower1Mask;
    remote_state[2] |= kGreePower2Mask;
}

void Gree::off()
{
    remote_state[0] &= ~kGreePower1Mask;
    remote_state[2] &= ~kGreePower2Mask;
}

void Gree::setPower(const bool state)
{
    if (state)
        on();
    else
        off();
}

bool Gree::getPower()
{
    return remote_state[0] & kGreePower1Mask;
}

// Set the temp. in deg C
void Gree::setTemp(const uint8_t temp)
{
    uint8_t new_temp = std::max((uint8_t)kGreeMinTemp, temp);
    new_temp = std::min((uint8_t)kGreeMaxTemp, new_temp);
    if (getMode() == kGreeAuto)
        new_temp = 25;
    remote_state[1] = (remote_state[1] & 0xF0U) | (new_temp - kGreeMinTemp);
}

// Return the set temp. in deg C
uint8_t Gree::getTemp()
{
    return ((remote_state[1] & 0xFU) + kGreeMinTemp);
}

// Set the speed of the fan, 0-3, 0 is auto, 1-3 is the speed
void Gree::setFan(const uint8_t speed)
{
    uint8_t fan = std::min((uint8_t)kGreeFanMax, speed); // Bounds check

    if (getMode() == kGreeDry)
        fan = 1; // DRY mode is always locked to fan 1.
    // Set the basic fan values.
    remote_state[0] &= ~kGreeFanMask;
    remote_state[0] |= (fan << 4);
}

uint8_t Gree::getFan() { return ((remote_state[0] & kGreeFanMask) >> 4); }

void Gree::setMode(const uint8_t new_mode)
{
    uint8_t mode = new_mode;
    switch (mode)
    {
    case kGreeAuto:
        // AUTO is locked to 25C
        setTemp(25);
        break;
    case kGreeDry:
        // DRY always sets the fan to 1.
        setFan(1);
        break;
    case kGreeCool:
    case kGreeFan:
    case kGreeHeat:
        break;
    default:
        // If we get an unexpected mode, default to AUTO.
        mode = kGreeAuto;
    }
    remote_state[0] &= ~kGreeModeMask;
    remote_state[0] |= mode;
}

uint8_t Gree::getMode() { return (remote_state[0] & kGreeModeMask); }

void Gree::setLight(const bool state)
{
    remote_state[2] &= ~kGreeLightMask;
    remote_state[2] |= (state << 5);
}

bool Gree::getLight() { return remote_state[2] & kGreeLightMask; }

void Gree::setXFan(const bool state)
{
    remote_state[2] &= ~kGreeXfanMask;
    remote_state[2] |= (state << 7);
}

bool Gree::getXFan() { return remote_state[2] & kGreeXfanMask; }

void Gree::setSleep(const bool state)
{
    remote_state[0] &= ~kGreeSleepMask;
    remote_state[0] |= (state << 7);
}

bool Gree::getSleep() { return remote_state[0] & kGreeSleepMask; }

void Gree::setTurbo(const bool state)
{
    remote_state[2] &= ~kGreeTurboMask;
    remote_state[2] |= (state << 4);
}

bool Gree::getTurbo() { return remote_state[2] & kGreeTurboMask; }

void Gree::setSwingVertical(const bool automatic, const uint8_t position)
{
    remote_state[0] &= ~kGreeSwingAutoMask;
    remote_state[0] |= (automatic << 6);
    uint8_t new_position = position;
    if (!automatic)
    {
        switch (position)
        {
        case kGreeSwingUp:
        case kGreeSwingMiddleUp:
        case kGreeSwingMiddle:
        case kGreeSwingMiddleDown:
        case kGreeSwingDown:
            break;
        default:
            new_position = kGreeSwingLastPos;
        }
    }
    else
    {
        switch (position)
        {
        case kGreeSwingAuto:
        case kGreeSwingDownAuto:
        case kGreeSwingMiddleAuto:
        case kGreeSwingUpAuto:
            break;
        default:
            new_position = kGreeSwingAuto;
        }
    }
    remote_state[4] &= ~kGreeSwingPosMask;
    remote_state[4] |= new_position;
}

bool Gree::getSwingVerticalAuto()
{
    return remote_state[0] & kGreeSwingAutoMask;
}

uint8_t Gree::getSwingVerticalPosition()
{
    return remote_state[4] & kGreeSwingPosMask;
}

// Convert a standard A/C mode into its native mode.
uint8_t Gree::convertMode(const stdAc::opmode_t mode)
{
    switch (mode)
    {
    case stdAc::opmode_t::kCool:
        return kGreeCool;
    case stdAc::opmode_t::kHeat:
        return kGreeHeat;
    case stdAc::opmode_t::kDry:
        return kGreeDry;
    case stdAc::opmode_t::kFan:
        return kGreeFan;
    default:
        return kGreeAuto;
    }
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t Gree::convertFan(const stdAc::fanspeed_t speed)
{
    switch (speed)
    {
    case stdAc::fanspeed_t::kMin:
        return kGreeFanMin;
    case stdAc::fanspeed_t::kLow:
    case stdAc::fanspeed_t::kMedium:
        return kGreeFanMax - 1;
    case stdAc::fanspeed_t::kHigh:
    case stdAc::fanspeed_t::kMax:
        return kGreeFanMax;
    default:
        return kGreeFanAuto;
    }
}

// Convert a standard A/C Vertical Swing into its native version.
uint8_t Gree::convertSwingV(const stdAc::swingv_t swingv)
{
    switch (swingv)
    {
    case stdAc::swingv_t::kHighest:
        return kGreeSwingUp;
    case stdAc::swingv_t::kHigh:
        return kGreeSwingMiddleUp;
    case stdAc::swingv_t::kMiddle:
        return kGreeSwingMiddle;
    case stdAc::swingv_t::kLow:
        return kGreeSwingMiddleDown;
    case stdAc::swingv_t::kLowest:
        return kGreeSwingDown;
    default:
        return kGreeSwingAuto;
    }
}

// Convert the internal state into a human readable string.
#ifdef ARDUINO
String Gree::toString()
{
    String result = "";
#else
std::string Gree::toString()
{
    std::string result = "";
#endif // ARDUINO
    result += F("Power: ");
    if (getPower())
        result += F("On");
    else
        result += F("Off");
    result += F(", Mode: ");
    result += uint64ToString(getMode());
    switch (getMode())
    {
    case kGreeAuto:
        result += F(" (AUTO)");
        break;
    case kGreeCool:
        result += F(" (COOL)");
        break;
    case kGreeHeat:
        result += F(" (HEAT)");
        break;
    case kGreeDry:
        result += F(" (DRY)");
        break;
    case kGreeFan:
        result += F(" (FAN)");
        break;
    default:
        result += F(" (UNKNOWN)");
    }
    result += F(", Temp: ");
    result += uint64ToString(getTemp());
    result += F("C, Fan: ");
    result += uint64ToString(getFan());
    switch (getFan())
    {
    case 0:
        result += F(" (AUTO)");
        break;
    case kGreeFanMax:
        result += F(" (MAX)");
        break;
    }
    result += F(", Turbo: ");
    if (getTurbo())
        result += F("On");
    else
        result += F("Off");
    result += F(", XFan: ");
    if (getXFan())
        result += F("On");
    else
        result += F("Off");
    result += F(", Light: ");
    if (getLight())
        result += F("On");
    else
        result += F("Off");
    result += F(", Sleep: ");
    if (getSleep())
        result += F("On");
    else
        result += F("Off");
    result += F(", Swing Vertical Mode: ");
    if (getSwingVerticalAuto())
        result += F("Auto");
    else
        result += F("Manual");
    result += F(", Swing Vertical Pos: ");
    result += uint64ToString(getSwingVerticalPosition());
    switch (getSwingVerticalPosition())
    {
    case kGreeSwingLastPos:
        result += F(" (Last Pos)");
        break;
    case kGreeSwingAuto:
        result += F(" (Auto)");
        break;
    }
    return result;
}
