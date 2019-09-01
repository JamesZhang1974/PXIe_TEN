/*!
 \file   M24M02.h
 \brief  ST M24M02 I2C Serial EEPROM Hardware Interface
         This class provides an interface to control an M24M02 I2C Serial EEPROM

         This implementation supports storing and retreiving strings only.
         Space is allocated in a simple table format where each string
         starts at a fixed address and has a fixed maximum size - see
         defn. of "STRINGS" constant which contains meta data about the
         string table.

         Strings may be shorter than the allocated size, in which case a
         NUL (0x00) is stored after the last character to mark the end
         of the string. Note all IO is via QString objects so the NUL is
         not included in the string data sent to or from the class via
         signals; it is only used internally.

         Only 8 bit characters are supported!

         The implementation only supports the 1st page of the EEPROM
         (64 KB out of the total 256 KB). This is because the upper two
         address bits (=EEPROM page) are included as part of the I2C address
         byte, which is inconvenient for this implementation as we use a
         pre-defined I2C address. If more than 64 KB of EEPROM storage are
         needed, the page number will need to be added to the I2C address
         for reads and writes to the device.

         The following string items will be stored in the EEPROM:

            Model Number            - "PG2304"       - 20  chars
            Serial Number           - "00001234"     - 100 chars
            Production Date         - "2018-10-23"   - 20  chars
            Calibration date        - "2018-10-23"   - 20  chars
            Warranty starting date  - "2018-10-23"   - 20  chars
            Warranty ending date    - "2018-10-23"   - 20  chars

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018

*/


#ifndef M24M02_H
#define M24M02_H

#include <stdint.h>
#include <QStringList>
#include <QMap>
#include <string.h>

#include "globals.h"
#include "BertComponent.h"
#include "I2CComms.h"
#include "LMXFrequencyProfile.h"



class M24M02 : public BertComponent
{
    Q_OBJECT

public:

    M24M02(I2CComms *comms, const uint8_t i2cAddress, const int deviceID);

    static bool ping(I2CComms *comms, const uint8_t i2cAddress);
    void getOptions();
    int init();

    int readFrequencyProfiles(int deviceID, QList<LMXFrequencyProfile> &frequencyProfiles);
    int writeFrequencyProfiles(int deviceID, QList<LMXFrequencyProfile> &frequencyProfiles);
  //  void WriteFirmare(int deviceID);




    QString ReadModelCode();

#define M24M02_SIGNALS \
    void EEPROMStringData(int deviceID, QString model, QString serial, QString productionDate, QString calibrationDate, QString warrantyStart, QString warrantyEnd, QString synthConfigVersion);

#define M24M02_SLOTS \
    void EEPROMReadStrings(int deviceID); \
    void EEPROMWriteStrings(int deviceID, QString model, QString serial, QString productionDate, QString calibrationDate, QString warrantyStart, QString warrantyEnd, QString synthConfigVersion);\
    void WriteFirmware(int deviceID);

#define M24M02_CONNECT_SIGNALS(CLIENT, M24M02) \
    connect(M24M02, SIGNAL(EEPROMStringData(int, QString, QString, QString, QString, QString, QString, QString)),   CLIENT, SLOT(EEPROMStringData(int, QString, QString, QString, QString, QString, QString, QString)));    \
    connect(CLIENT, SIGNAL(EEPROMReadStrings(int)),                                                                 M24M02, SLOT(EEPROMReadStrings(int)));                                                                  \
    connect(CLIENT, SIGNAL(EEPROMWriteStrings(int, QString, QString, QString, QString, QString, QString, QString)), M24M02, SLOT(EEPROMWriteStrings(int, QString, QString, QString, QString, QString, QString, QString)));  \
    connect(CLIENT, SIGNAL(WriteFirmware(int)),                                                                     M24M02, SLOT(WriteFirmware(int)));                                                                  \
    BERT_COMPONENT_CONNECT_SIGNALS(CLIENT, M24M02)

signals:
    // Signals which are specific to a PCA9775:
    M24M02_SIGNALS

public slots:
    // Public Slots for M24M02 commands:
    M24M02_SLOTS

private:

    // Meta data about a string block in the EEPROM:
    struct StringInfo
    {
        uint16_t address;
        int maxLength;
    };

    // List of recognised string identifiers:
    enum StringID
    {
        MODEL,
        SERIAL,
        PROD_DATE,
        CAL_DATE,
        WARRANTY_START,
        WARRANTY_END,
        SYNTH_CONFIG_VERSION
    };

    // List of known strings (maps string ID to string length):
    static const QMap<StringID, uint16_t> STRING_LENGTHS;

    // String Info - Built by constructor (maps string ID to string info):
    QMap<StringID, StringInfo>strings;

    // Page Ids:
    static const uint8_t PAGE_STRINGS       = 0;
    static const uint8_t PAGE_FREQ_PROFILES = 2;
    static const uint8_t PAGE_Firmware      = 3;


    // Max block sizes
    static const int WRITE_BLK_SIZE = 59;  // Limit of USB-I2C Adaptor write buffer
    static const int READ_BLK_SIZE = 64;   // Limit of USB-I2C Adaptor read buffer

    I2CComms *comms;
    const uint8_t i2cAddress;
    const int deviceID;

    int storeBytes(uint8_t page, uint16_t *address, uint8_t *data, uint8_t nBytes);
    int loadBytes(uint8_t page, uint16_t *address, uint8_t *data, uint8_t nBytes);

    int storeUInt16(uint8_t page, uint16_t *address, uint16_t value, uint16_t *checkSum);
    int loadUInt16(uint8_t page, uint16_t *address, uint16_t *value, uint16_t *checkSum);

    int waitStoreAck();

    int storeBlock(uint8_t page, uint16_t *address, uint8_t *data, uint16_t nBytes);
    int loadBlock(uint8_t page, uint16_t *address, uint8_t *data, uint16_t nBytes);

    int storeString(StringID stringID, const QString &stringData);
    int loadString(StringID stringID, QString &stringData);

    int storeFrequencyProfile(uint16_t *address, LMXFrequencyProfile &profile);
    int loadFrequencyProfile(uint16_t *address, LMXFrequencyProfile &profile);


    int clearEEPROM();
    int clearPROFILES();

};



#endif // M24M02_H
