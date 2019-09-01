/*!
 \file   SI5340.h
 \brief  Functional commands to control a Silicon Labs SI5340 low jitter clock generator

 \author J Cole-Baker (For Smartest)
 \date   Dec 2018
*/

#ifndef SI5340_H
#define SI5340_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QMap>

#include "globals.h"
#include "BertComponent.h"
#include "I2CComms.h"

class SI5340 : public BertComponent
{
    Q_OBJECT

public:
    SI5340(I2CComms *comms, const uint8_t i2cAddress, const int deviceID);

    static bool ping(I2CComms *comms, const uint8_t i2cAddress);
    void getOptions();
    int init();

#define SI5340_SIGNALS \
    void RefClockInfo(int deviceID, int indexProfile, float frequencyIn, float frequencyOut, QString descriptionIn, QString descriptionOut); \
    void RefClockSettingsChanged(int deviceID);

#define SI5340_SLOTS \
    void GetRefClockInfo(); \
    void RefClockSelectProfile(int indexProfile, bool triggerResync = true);

#define SI5340_CONNECT_SIGNALS(CLIENT, SI5340) \
    connect(SI5340, SIGNAL(RefClockInfo(int, int, float, float, QString, QString)), CLIENT, SLOT(RefClockInfo(int, int, float, float, QString, QString)));  \
    connect(SI5340, SIGNAL(RefClockSettingsChanged(int)),                           CLIENT, SLOT(RefClockSettingsChanged(int)));                            \
    connect(CLIENT, SIGNAL(GetRefClockInfo()),                                      SI5340, SLOT(GetRefClockInfo()));                                       \
    connect(CLIENT, SIGNAL(RefClockSelectProfile(int, bool)),                       SI5340, SLOT(RefClockSelectProfile(int, bool)));                        \
    BERT_COMPONENT_CONNECT_SIGNALS(CLIENT, SI5340)

signals:
    // Signals which are specific to a SI5340:
    SI5340_SIGNALS

    // Public Slots for BERT Commands:
public slots:
    SI5340_SLOTS


private:

    typedef struct SI5340Register_t
    {
        uint16_t address;    // 16-bit register address (includes page as upper 8 bits)
        uint8_t  value;      // 8-bit register data
        // SI5340Register_t(uint16_t address, uint8_t value) : address(address), value(value) {}
    } SI5340Register_t;

    static const QStringList PROFILE_LIST;
      // List of 'profiles' the user can select

    static const int DEFAULT_PROFILE = 0;
      // Default profiles at startup

    static const QList<SI5340Register_t> CONFIG_PREAMBLE;
      // Registers to set first when starting to set up the device for a profile.
      // These are the same for ALL profiles.

    static const int PREAMBLE_SLEEP_MS = 300;
      // Sleep this many milliseconds after setting CONFIG_PREAMBLE registers

    static const QMap<int, QList<SI5340Register_t>> CONFIG_PROFILES;
      // Maps a profile index to a list of register settings for that profile.
      // Must be one profile index for each profile in PROFILE_LIST.

    static const QList<SI5340Register_t> CONFIG_POSTAMBLE;
      // Registers to set after loading config regusters for a profile.
      // These are the same for ALL profiles.


    int selectProfile(int index);

    int selectPage(uint8_t page);
    int writeRegister(uint8_t page, uint8_t address, uint8_t data);
    int readRegister(uint8_t page, uint8_t address, uint8_t *data);

    I2CComms *comms;
    const uint8_t i2cAddress;
    const int deviceID;

    // Current Reference Clock State:
    int selectedProfileIndex = DEFAULT_PROFILE;  // Current profile
    double frequencyIn;        // Input frequency, MHz (0 = Unknown)
    double frequencyOut;       // Output frequency, MHz (0 = Unknown)
    QString descriptionIn;    // Text description of input source, e.g. "10 MHz"
    QString descriptionOut;   // Text description of output, e.g. "100 MHz"

    uint8_t previousPage = 0;  // Register page used for last read / write operation.
    bool pageValid = true;  // Assume page 0 is selected and ready at power on.

};

#endif // SI5340_H
