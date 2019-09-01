/*!
 \file   LMXFrequencyProfile.h
 \brief  Register definitions to represent one clock frequency setting on an LMX Clock IC
 \author J Cole-Baker (For Smartest)
 \date   Nov 2018
*/


#ifndef LMXFREQUENCYPROFILE_H
#define LMXFREQUENCYPROFILE_H

#include <QMap>
#include <stdint.h>

#include "globals.h"

/*!
  \brief LMX Frequency Profile Class
  Stores the register values for a frequency setting
*/
class LMXFrequencyProfile
{
public:

    LMXFrequencyProfile();
    explicit LMXFrequencyProfile(const int registerCount);
    ~LMXFrequencyProfile();

    void setRegisterCount(int registerCount) { this->registerCount = registerCount; }
    int getRegisterCount() const { return registerCount; }

    void setValid()            { valid = true;       }
    bool isValid()       const {  return valid;      }

    void setFrequency(float frequency) { this->frequency = frequency; }
    float getFrequency() const {  return frequency;  }

    int setRegisterValue(const uint8_t address, const uint16_t value);
    uint16_t getRegisterValue(const uint8_t address, bool *registerFound) const;

    int getUsedRegisterCount() const { return registers.count(); }

    void clear();

private:

    int registerCount = 0;
       // Maximum number of registers which can be stored. We assume they will be for addresses 0 to (registerCount-1)
       // and enforce this limit. Note address is uint8_t so maximum possible number of registers is 256.
       // Note that data will only be stored for registers which have been set with setRegisterValue.
       // getUsedRegisterCount stores the actual number of registers we have stored.
       // Defaults to 0, so register valuess can't be set unless a count is supplied to the constructor,
       // or set with setRegisterCount().

    bool valid = false;
    float frequency = 0.0;

    QMap<uint8_t, uint16_t> registers;

};



#endif // LMXFREQUENCYPROFILE_H
