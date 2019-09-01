/*!
 \file   PCA9557A.h
 \brief  Texas Instruments PCA9557 I/O Expander Hardware Interface Header
         This class provides an interface to control a PCA9557 I/O expander

         Private methods implement all low-level configuration of the device.
         Client access to the device is provided by signals and slots which
         implement "meta" functions, i.e. some action in the context
         of the BERT instrument, such as setting Trigger Divide Ratio
         (actually sets two I/O pins which are wired to the clock divider).

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018

//(AUG27,2019) James: Remove trigger divide function to PCA9557B.

*/


#ifndef PCA9557A_H
#define PCA9557A_H

#include <stdint.h>

#include "globals.h"
#include "BertComponent.h"
#include "I2CComms.h"


class PCA9557A : public BertComponent
{
    Q_OBJECT

public:

    PCA9557A(I2CComms *comms, const uint8_t i2cAddress, const int deviceID);

    static bool ping(I2CComms *comms, const uint8_t i2cAddress);
    void getOptions();
    int init();

#define PCA9557A_SIGNALS \
    void LMXLockDetect(int deviceId, bool isLocked);

#define PCA9557A_SLOTS \
    void SetEEPROMWriteEnable(bool enable); \
    void ReadLMXLockDetect();

#define PCA9557A_CONNECT_SIGNALS(CLIENT, PCA9557A) \
    connect(PCA9557A, SIGNAL(LMXLockDetect(int, bool)),    CLIENT,  SLOT(LMXLockDetect(int, bool)));    \
    connect(CLIENT,  SIGNAL(SetEEPROMWriteEnable(bool)),  PCA9557A, SLOT(SetEEPROMWriteEnable(bool)));  \
    connect(CLIENT,  SIGNAL(ReadLMXLockDetect()),         PCA9557A, SLOT(ReadLMXLockDetect()));         \
    BERT_COMPONENT_CONNECT_SIGNALS(CLIENT, PCA9557A)

signals:
    // Signals which are specific to a PCA9775A:
    PCA9557A_SIGNALS

    // Public Slots for PCA9557 commands:
public slots:
    PCA9557A_SLOTS


private:

    // Define some enums used to specify properties of an IO Expander Pin:
    enum PinNumber
    {
        P0 = 0,
        P1 = 1,
        P2 = 2,
        P3 = 3,
        P4 = 4,
        P5 = 5,
        P6 = 6,
        P7 = 7
    };

    enum PinDirection
    {
      NORMAL_INPUT,     // IN:  High voltage level = 1
      INVERTED_INPUT,   // IN:  Low voltage level = 1
      OUTPUT            // OUT: 1 to set high, 0 to set low
    };


    int configurePins(const PinDirection p0Dir,
                      const PinDirection p1Dir,
                      const PinDirection p2Dir,
                      const PinDirection p3Dir,
                      const PinDirection p4Dir,
                      const PinDirection p5Dir,
                      const PinDirection p6Dir,
                      const PinDirection p7Dir);

    // DEPRECATED void    setPin(const PinNumber pin, const uint8_t value);
    // DEPRECATED uint8_t getPin(const PinNumber pin);

    int setPins(const uint8_t value);
    int getPins(uint8_t *value);
    int updatePins(const uint8_t mask, const uint8_t value);

    int writePins();
    int readPins();
    int test(bool loopBackTest);

    // Register Addresses:
    static const uint8_t REG_INPUT    = 0x00;
    static const uint8_t REG_OUPUT    = 0x01;
    static const uint8_t REG_POLARITY = 0x02;
    static const uint8_t REG_CONFIG   = 0x03;


    I2CComms *comms;
    const uint8_t i2cAddress;
    const int deviceID;

    // PCA9557 Registers:
    // Default values reflect power-on defaults for device registers.
    uint8_t regInput    = 0x00;  // Input buffer; One bit per I/O pin
    uint8_t regOutput   = 0x00;  // Output buffer; One bit per I/O pin
    uint8_t regPolarity = 0xF0;  // Polarity Inversion Register; One bit per I/O pin, inverts INPUT value; 0 = Normal; 1 = Inverted
    uint8_t regConfig   = 0xFF;  // Configuration Register; One bit per I/O pin; 0 = Output; 1 = Input

 //   static const uint8_t TRIGGER_DIVIDE_BITMASK;
 //   static const QList<uint8_t> TRIGGER_DIVIDE_LOOKUP;
 //   static const QStringList TRIGGER_DIVIDE_LIST;
 //   static const size_t TRIGGER_DIVIDE_DEFAULT_INDEX;

    static const uint8_t EEPROM_WC_BITMASK;
    static const uint8_t EEPROM_WRITE_ENABLE;
    static const uint8_t EEPROM_WRITE_DISABLE;

};



#endif // PCA9557A_H
