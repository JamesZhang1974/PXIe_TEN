/*!
 \file   LMXFrequencyProfile.cpp
 \brief  Register definitions to represent one clock frequency setting on an LMX Clock IC
 \author J Cole-Baker (For Smartest)
 \date   Nov 2018
*/

#include <QDebug>

#include "LMXFrequencyProfile.h"


LMXFrequencyProfile::LMXFrequencyProfile()
{ }

LMXFrequencyProfile::LMXFrequencyProfile(const int registerCount)
{
     this->registerCount = registerCount;
}

LMXFrequencyProfile::~LMXFrequencyProfile()
{}


/*!
 \brief Set register value
 Sets a profile register to a specified value. If the register
 hasn't been added to the profile yet, it is created.
 \param address  Register address to set a value for (0 to max address)
 \param value    Value for register
 \return globals::OK        Success
 \return globals::OVERFLOW  Address out of range
*/
int LMXFrequencyProfile::setRegisterValue(const uint8_t address, const uint16_t value)
{
    Q_ASSERT(address < registerCount);
    if (address > registerCount) return globals::OVERFLOW;
    registers.insert(address, value);
    return globals::OK;
 }


/*!
 \brief Get register value
 Returns the value of a register loaded from a profile
 (see constructor)

 If the register isn't in the internal list (i.e. no value
 was found for the register when parsing the profile data),
 0 is returned, and *registerFound is set to false.

 \param address        Register address to get a value for (0-64)
 \param registerFound  Pointer to a bool, or null. If specified, the target will be
                       set to true if the register was found in the frequency profile,
                       or false if not.
 \return Register value (0 if register not found in profile)
*/
uint16_t LMXFrequencyProfile::getRegisterValue(const uint8_t address, bool *registerFound) const
{
    Q_ASSERT(address < registerCount);

    if (registers.contains(address))
    {
        if (registerFound) *registerFound = true;
        return registers.value(address);
    }
    else
    {
        if (registerFound) *registerFound = false;
        return 0;
    }
}



