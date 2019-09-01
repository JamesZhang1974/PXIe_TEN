/*!
 \file   M24M02.cpp
 \brief  ST M24M02 I2C Serial EEPROM Hardware Interface
         This class provides an interface to control an M24M02 I2C Serial EEPROM
 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <QDebug>
#include <QFile>
#include <QString>
#include <QIODevice>
#include <QDataStream>
#include <QByteArray>
#include <QMap>
#include <cstdlib>
#include <BertFile.h>
#include <QTime>  // For testing / profiling


#include "M24M02.h"

// Debug Macro for EEPROM Read / Write:
// #define BERT_EEPROM_DEBUG
#ifdef BERT_EEPROM_DEBUG
  #define DEBUG_EEPROM(MSG) qDebug() << "\t\t" << MSG;
#endif
#ifndef DEBUG_EEPROM
  #define DEBUG_EEPROM(MSG)
#endif

// For EXTRA debug:
#define DEBUG_EEPROM_EXTRA(MSG)

// String length table definition: Maps string identifier to length
const QMap<M24M02::StringID, uint16_t> M24M02::STRING_LENGTHS =
    QMap<M24M02::StringID, uint16_t>(
        {
           // STRING                       LENGTH
            { M24M02::MODEL,                20  },
            { M24M02::SERIAL,               50  },
            { M24M02::PROD_DATE,            20  },
            { M24M02::CAL_DATE,             20  },
            { M24M02::WARRANTY_START,       20  },
            { M24M02::WARRANTY_END,         20  },
            { M24M02::SYNTH_CONFIG_VERSION, 50  }
        }
    );


/*!
 * \brief Constructor - Stores a ref to the comms module and the I2C address for the M24M02
 *        Also builds a map of memory contents based on static data
 *
 * \param comms        Reference to a BertComms object representing the connection to an
 *                     instrument. Used to carry out I2C operations
 * \param i2cAddress  I2C slave address of this M24M02 component
 * \param deviceID    Unique ID for this device in systems where there may be
 *                    several M24M02 ICs. Assigned by the instantiator, probably
 *                    starting from 0.
 */
M24M02::M24M02(I2CComms *comms, const uint8_t i2cAddress, const int deviceID)
 : comms(comms), i2cAddress(i2cAddress), deviceID(deviceID)
{
    uint16_t address = 0;
    foreach(StringID stringID, STRING_LENGTHS.keys())
    {
        strings.insert(stringID, { address, STRING_LENGTHS.value(stringID) } );
        // qDebug() << "Mapping string " << stringID << ": Addr: " << address << "; Len: " << STRING_LENGTHS.value(stringID);
        address += STRING_LENGTHS.value(stringID);
    }
}


/*!
 \brief Check an I2C address to see whether there is a M24M02 present.

 \param comms       Pointer to I2C Comms object (created elsewhere and already connected)
 \param i2cAddress  Address to check on
 \return true   M24M02 found
 \return false  No response or unexpected response
*/
bool M24M02::ping(I2CComms *comms, const uint8_t i2cAddress)
{
    qDebug() << "M24M02: Searching on address " << INT_AS_HEX(i2cAddress,2) << "...";
    Q_ASSERT(comms->portIsOpen());
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;

    int result;

    result = comms->pingAddress(i2cAddress);
    if (result != globals::OK)
    {
        qDebug() << "M24M02 not found (no ACK on I2C address; result: " << result << ")";
        return false;
    }

    // EEPROM responded.
    qDebug() << "M24M02 found.";
    return true;
}


/*!
 \brief Get Options for M24M02
        Requests that this module emit signals describing its available options lists to client
*/
void M24M02::getOptions()
{
    // Nothing to do; No options lists for M24M02.
}




 /*!
 \brief M24M02  Initialisation
 \return globals::OK    Success!
 \return [error code]   Error connecting or downloading macros. Comms problem or invalid I2C address?
 SIGNALS:
  emits ShowMessage(...) to show progress messages to user
*/
int M24M02::init()
{
    qDebug() << "M24M02: Init for M24M02 with ID " << deviceID << "; I2C Address " << INT_AS_HEX(i2cAddress,2);
    Q_ASSERT(comms->portIsOpen());
    // Nothing to do... no init required.
    // FOR TESTING: Clear contents: clearEEPROM();  // Nb: Default WC must also be set to write enable in PCA9557.cpp!
    return globals::OK;
}



/*!
 \brief Read Model Code from EEPROM strings
 If the code can't be found, returns "NONE"
 \return Model code (String)
*/
QString M24M02::ReadModelCode()
{
    DEBUG_EEPROM("M24M02: EEPROM Read Model Code")
    QString model;
    int result;
    result = loadString(MODEL, model);
    if (result == globals::OK)
    {
        return model;
    }
    else
    {
        qDebug() << "M24M02: Error reading model code: " << result;
        return "NONE";
    }
}




/*!
 \brief Read Frequency Profiles from M24M02 EEPROM
 \param   deviceID           Device ID check: If ths doesn't match the device, INVALID_BOARD is returned.
 \param   frequencyProfiles  Reference to list of frequency profiles. Any existing profiles are deleted;
                             Used to return the NEW profiles read from EEPROM (if any)
 \return globals::OK
 \return [error code]
*/
int M24M02::readFrequencyProfiles(int deviceID, QList<LMXFrequencyProfile> &frequencyProfiles)
{
    if (deviceID != this->deviceID) return globals::INVALID_BOARD;  // Not for us!
    DEBUG_EEPROM("M24M02: EEPROM Read Frequency Profiles - Device " << deviceID)

    frequencyProfiles.clear();   // Remove any old profiles

    //###### Time Recording - for testing ##########
    QTime t;
    t.start();
    //##############################################

    uint16_t address = 0;
    int result = globals::OK;

    static const int MAX_PROFILES = 255;  // For sanity check... Max number of profiles which will fit in one page.

    // -- Read profile count from first 2 bytes: --
    uint16_t profileCount = 0;
    address = 0;
    result = loadUInt16(PAGE_FREQ_PROFILES, &address, &profileCount, nullptr);
    if (result != globals::OK)
    {
        DEBUG_EEPROM("M24M02: Error reading Frequency Profile count (" << result << ")")
        return globals::INVALID_DATA;
    }
    DEBUG_EEPROM("M24M02: Found " << profileCount << " frequency profiles in EEPROM")
    if (profileCount > MAX_PROFILES)  // Sanity check; should pick up invalid EEPROM data.
    {
        DEBUG_EEPROM("M24M02: ERROR: Unrealistic number of frequency profiles! (shouldn't be more than " << MAX_PROFILES << "). EEPROM probably contains stale or invalid data.")
        return globals::INVALID_DATA;
    }

    // Profiles are stored one per 256 byte sub-page, starting at sub-page 1
    // Sub-page 0 stores number of profiles.
    int profilePageIndex = 1;

    for (int profileIndex = 0; profileIndex < profileCount; profileIndex++)
    {
        LMXFrequencyProfile newProfile;
        address = static_cast<uint16_t>(profilePageIndex * 256);
        result = loadFrequencyProfile(&address, newProfile);
        if (result == globals::OK)
        {
            frequencyProfiles.append(newProfile);
        }
        else
        {
            DEBUG_EEPROM("M24M02: Error reading frequency profile at address " << address << " (" << result << ")");
            if (result != globals::BAD_CHECKSUM) return globals::INVALID_DATA;
              // If we got a bad checksum, other profiles are probably OK so Keep going.
              // Other errors are probably more fatal, so give up.
        }
        profilePageIndex++;
    }

    //##############################################
    qDebug("EEPROM Read time elapsed: %d ms", t.elapsed());
    //##############################################

    DEBUG_EEPROM("M24M02: Frequency profiles read OK")
    return globals::OK;
}


/*!
 \brief Write Frequency Profiles to M24M02 EEPROM
 \param   deviceID           Device ID check: If ths doesn't match the device, INVALID_BOARD is returned.
 \param   frequencyProfiles  Reference to list of frequency profiles to write to EEPROM
 \return globals::OK
 \return [error code]
*/
int M24M02::writeFrequencyProfiles(int deviceID, QList<LMXFrequencyProfile> &frequencyProfiles)
{
    if (deviceID != this->deviceID) return globals::INVALID_BOARD;  // Not for us!
    if (frequencyProfiles.count() > 255)
    {
        DEBUG_EEPROM("M24M02: WARNING: Maximum of 255 frequency profiles can be stored! (have " << frequencyProfiles.count() << "; extras will be dropped.");
    }

    DEBUG_EEPROM("M24M02: EEPROM Write " << frequencyProfiles.length() << " Frequency Profiles - Device " << deviceID);

    uint16_t address = 0;
    int result = globals::OK;
/*
    result = clearPROFILES();
    if (result != globals::OK)
    {
        DEBUG_EEPROM("M24M02: Error delete Frequency Profiles (" << result << ")")
        return result;
    }
*/
    //###### Time Recording - for testing ##########
    QTime t;
    t.start();
    //##############################################

    // -- Write profile count as first 2 bytes: --
    result = storeUInt16(PAGE_FREQ_PROFILES, &address, static_cast<uint16_t>(frequencyProfiles.length()), nullptr);
    if (result != globals::OK)
    {
        DEBUG_EEPROM("M24M02: Error writing Frequency Profile count (" << result << ")")
        return result;
    }

    // -- Write each profile: --
    // Profiles are written one per 256 byte sub-page, starting at sub-page 1
    // Sub-page 0 stores number of profiles.
    int profilePageIndex = 1;
    foreach(LMXFrequencyProfile profile, frequencyProfiles)
    {
        address = static_cast<uint16_t>(profilePageIndex * 256);
        result = storeFrequencyProfile(&address, profile);
        if (result != globals::OK)
        {
            DEBUG_EEPROM("M24M02: Error writing Frequency Profile for frequency " << profile.getFrequency() << " at address " << address << "(" << result << ")")
            return result;
        }
        profilePageIndex++;
    }

    //##############################################
    qDebug("EEPROM Write time elapsed: %d ms", t.elapsed());
    //##############################################

    DEBUG_EEPROM("M24M02: Frequency profiles written OK")
    return globals::OK;
}



/*!
 \brief Write Firmware to M24M02 EEPROM
 \param   deviceID           Device ID check: If ths doesn't match the device, INVALID_BOARD is returned.
 \param   firmware           Reference to a firmware to write to EEPROM
 \return globals::OK
 \return [error code]
 */


void M24M02::WriteFirmware(int deviceID)
{
    if (deviceID != this->deviceID)
    {
        DEBUG_EEPROM("M24M02: Wrong device ID" << device ID);
        return;
    }

    QString firmwarePath = globals::getAppPath() + QString("/firmwares/GT1706-rev-3-5-2-B.bin");
    QFile firmwareFile(firmwarePath);

    if(firmwareFile.open(QIODevice::ReadOnly))
    {
       qDebug() <<" Firmware found!"<<firmwareFile.size();
    };

    const uint16_t firmwareLength = firmwareFile.size();


    uint8_t *dataBuffer = new uint8_t[firmwareLength];
    uint16_t i = 0;

    QDataStream in(&firmwareFile);
    while(i < firmwareLength)
    {
        in >> dataBuffer[i];
        qDebug() <<" Firmware data address is " << i <<"data value is " <<INT_AS_HEX(dataBuffer[i],2);
        i ++;
    };


    qDebug() <<" Firmware data is ready";

    //###### Time Recording - for testing ##########
    QTime t;
    t.start();
    //##############################################
/*
    uint16_t dataAddress = 0;
    int result = storeBlock(PAGE_Firmware, &dataAddress, dataBuffer, firmwareLength);


    if(result!=globals::OK)
    {
         qDebug()<<"M24M02: Firmware written failed!";
         return;
    }
    else
    {
         qDebug() <<"M24M02: Firmware written OK";

    };
*/
    //Generate ramdon_addresses to read back the data from EEPROM, then compare them with dataBuffer.
 //   globals::sleep(3000);
    uint16_t readAddress=0;
    uint8_t readData = 0;

    for(int j=0; j < firmwareLength; j++)
    {
        int result = loadBytes(PAGE_Firmware, &readAddress, &readData, 1);
        if(result==globals::OK)
        {
           qDebug()<<"Read data from EEPROM address"<< readAddress<< "data value" << INT_AS_HEX(readData,2) << "OK!";
        }
        else
        {
            qDebug() <<"Data readback from EEPROM is wrong!!!";
        };

    };

    firmwareFile.close();
    delete [] dataBuffer;


    //##############################################
    qDebug("EEPROM Write time elapsed: %d ms", t.elapsed());
    //##############################################


    return;

}




//---------------------------------------------------------
// SLOTS
//---------------------------------------------------------

/*!
 * \brief SLOT: Read string data from the EEPROM
 *  EMITS EEPROMStringData
 *        Result
 */
void M24M02::EEPROMReadStrings(int deviceID)
{
    if (deviceID != this->deviceID) return;  // Not for us!
    DEBUG_EEPROM("M24M02: EEPROM Read Strings - Device " << deviceID)
    QString model;
    QString serial;
    QString productionDate;
    QString calibrationDate;
    QString warrantyStart;
    QString warrantyEnd;
    QString synthConfigVersion;

    //###### Time Recording - for testing ##########
    QTime t;
    t.start();
    //##############################################

    int result;
    result                            = loadString( MODEL,                model              );
    if (result == globals::OK) result = loadString( SERIAL,               serial             );
    if (result == globals::OK) result = loadString( PROD_DATE,            productionDate     );
    if (result == globals::OK) result = loadString( CAL_DATE,             calibrationDate    );
    if (result == globals::OK) result = loadString( WARRANTY_START,       warrantyStart      );
    if (result == globals::OK) result = loadString( WARRANTY_END,         warrantyEnd        );
    if (result == globals::OK) result = loadString( SYNTH_CONFIG_VERSION, synthConfigVersion );

    //##############################################
    qDebug("EEPROM Read time elapsed: %d ms", t.elapsed());
    //##############################################

    if (result == globals::OK) emit EEPROMStringData(this->deviceID, model, serial, productionDate, calibrationDate, warrantyStart, warrantyEnd, synthConfigVersion);
    emit Result(result, globals::ALL_LANES);
}


/*!
 * \brief SLOT: Write string data to the EEPROM
 * Parameters are strings to write to EEPROM.
 * \param model
 * \param serial
 * \param productionDate
 * \param calibrationDate
 * \param warrantyStart
 * \param warrantyEnd
 * \param synthConfigVersion
 *  EMITS Result
 */
void M24M02::EEPROMWriteStrings(int deviceID,
                                QString model,
                                QString serial,
                                QString productionDate,
                                QString calibrationDate,
                                QString warrantyStart,
                                QString warrantyEnd,
                                QString synthConfigVersion)
{
    if (deviceID != this->deviceID) return;  // Not for us!
    DEBUG_EEPROM("M24M02: EEPROM Write Strings - Device " << deviceID)

    //###### Time Recording - for testing ##########
    QTime t;
    t.start();
    //##############################################

    int result;
    result                            = storeString( MODEL,                model              );
    if (result == globals::OK) result = storeString( SERIAL,               serial             );
    if (result == globals::OK) result = storeString( PROD_DATE,            productionDate     );
    if (result == globals::OK) result = storeString( CAL_DATE,             calibrationDate    );
    if (result == globals::OK) result = storeString( WARRANTY_START,       warrantyStart      );
    if (result == globals::OK) result = storeString( WARRANTY_END,         warrantyEnd        );
    if (result == globals::OK) result = storeString( SYNTH_CONFIG_VERSION, synthConfigVersion );

    //##############################################
    qDebug("EEPROM Write time elapsed: %d ms", t.elapsed());
    //##############################################

    emit Result(result, globals::ALL_LANES);
}




/**********************************************************************************/
/*  PRIVATE Methods                                                               */
/**********************************************************************************/

// NOTE: From experimentation, it looks like EEPROM has some kind of 256 byte 'sub page',
// and block writes past a 256 byte sub-page boundary cause an address wrap - i.e.
// the remaining bytes are written at the start of the sub-page. This conflicts with
// the M24M02 documentation, which specifies such behaviour will occur at 64KB page
// boundaries during block write operations (although there is a limit of 256 bytes
// per single write operation).
// We don't know if this is some weird behaviour in our I2C code (seems unlikely),
// an error in the M24M02 documentation, or an error with the part (clone part which
// isn't 100% compatible?)
// Work-around is to make sure that write operations don't cross 265 byte boundaries.
//
// ALSO:
// Writing clock profiles (i.e. lists of register data) across 256 byte sub-page
// boundaries caused data corruption EVEN when split into two write operations as below.
// Storage of frequency profiles seems OK when each is stored in its own 256 byte
// sub-page. However this behaviour is weird and mysterious and should be investigated...
//  * Possible error in multi-byte read or write code?
//  * Error in raw I2C transaction? (Examine with protocol analyser!)
//  * EEPROM timing weirdness?
//
// Jeremy Cole-Baker 18-Nov-2018


/*!
 * \brief Store Bytes to EEPROM
 * Multple bytes may be sent (up to 59); however, the EEPROM does not allow sequential
 * writes past the end of one page. If (address + nBytes) > 64kB, address will wrap
 * and remaining bytes will be written at the start of the same page!
 * \param page     Page number (0 - 3)
 * \param address  EEPROM address to store to (lower 16 bits; i.e. address within page)
 *                 On SUCCESS, address is automatically incremented by the number of bytes stored.
 * \param data     Data to store (8 bit)
 * \param nBytes   Number of bytes to store (1 to 59)
 * \return globals::OK  Success
 * \return [error code]
 */
int M24M02::storeBytes(uint8_t page, uint16_t *address, uint8_t *data, uint8_t nBytes)
{
    Q_ASSERT(nBytes > 0);
    Q_ASSERT(page <= 3);
    if (nBytes < 1 || page > 3) return globals::OVERFLOW;

    int result = globals::OK;
    int offsetInPageA = (*address) % 256;
    int offsetInPageB = ((*address) + nBytes - 1) % 256;
    if (offsetInPageA <= offsetInPageB)
    {
        // This write DOESN'T cross 256 byte sub-page. Single write OK!
        qDebug() << "NORMAL WRITE! Start @ " << INT_AS_HEX(*address, 4);
        result = comms->write(i2cAddress + page, *address, data, nBytes);
        if (result == globals::OK)
        {
            *address += nBytes;
            waitStoreAck();
        }
    }
    else
    {
        // This write crosses 256 byte sub-page. Split into two!
        uint8_t nBytesA = static_cast<uint8_t>(256 - offsetInPageA);
        uint8_t nBytesB = nBytes - nBytesA;

        qDebug() << "CROSS-SUBPAGE WRITE! Start @ " << INT_AS_HEX(*address, 4) << "(" << offsetInPageA << ")"
                 << "; Last Byte At: " << INT_AS_HEX((*address) + nBytes - 1, 4) << "(" << offsetInPageB << ")"
                 << "; Split after: " << INT_AS_HEX((*address) + nBytesA - 1, 4)
                 << " leaving " << nBytesB << " bytes @ " << INT_AS_HEX((*address) + nBytesA, 4);

        result = comms->write(i2cAddress + page, *address, data, nBytesA);
        if (result == globals::OK)
        {
            *address += nBytesA;
            waitStoreAck();
            result = comms->write(i2cAddress + page, *address, data, nBytesB);
            if (result == globals::OK)
            {
                *address += nBytesB;
                waitStoreAck();
            }
        }
    }

    DEBUG_EEPROM("[EEPROM Write Bytes]: Page: " << page << "; Addr AFTER: " << INT_AS_HEX(*address, 4) << "; Count: " << nBytes << "; Data: " << INT_AS_HEX(data[0], 2) << "... Result: " << result)
    return result;
}


/*!
 * \brief Load Bytes From EEPROM
 * Multple bytes may be read (up to 64); however, the EEPROM does not allow sequential
 * reads past the end of one page. If (address + nBytes) > 64kB, address will wrap
 * and remaining bytes will be read from the start of the same page!
 * \param page     Page number (0 - 3)
 * \param address  EEPROM address to load from (lower 16 bits; i.e. address within page)
 *                 On SUCCESS, address is automatically incremented by the number of bytes loaded.
 * \param data     Receives the loaded data (8 bit)
 * \param nBytes   Number of bytes to read (1 to 64)
 * \return globals::OK  Success
 * \return [error code]
 */
int M24M02::loadBytes(uint8_t page, uint16_t *address, uint8_t *data, uint8_t nBytes)
{
    Q_ASSERT(nBytes > 0);
    Q_ASSERT(page <= 3);
    if (nBytes < 1 || page > 3) return globals::OVERFLOW;

    int result = globals::OK;
    int offsetInPageA = (*address) % 256;
    int offsetInPageB = ((*address) + nBytes - 1) % 256;
    if (offsetInPageA <= offsetInPageB)
    {
        // This read DOESN'T cross 256 byte sub-page. Single read OK!
        int result = comms->read(i2cAddress + page, *address, data, nBytes);
        if (result == globals::OK) *address += nBytes;
    }
    else
    {
        // This read crosses 256 byte sub-page. Split into two!
        uint8_t nBytesA = static_cast<uint8_t>(256 - offsetInPageA);
        uint8_t nBytesB = nBytes - nBytesA;
        /* DEBUG
        qDebug() << "CROSS-SUBPAGE READ! Start @ " << INT_AS_HEX(*address, 4) << "(" << offsetInPageA << ")"
                 << "; Last Byte At: " << INT_AS_HEX((*address) + nBytes - 1, 4) << "(" << offsetInPageB << ")"
                 << "; Split after: " << INT_AS_HEX((*address) + nBytesA - 1, 4)
                 << " leaving " << nBytesB << " bytes @ " << INT_AS_HEX((*address) + nBytesA, 4);
        */
        result = comms->read(i2cAddress + page, *address, data, nBytesA);
        if (result == globals::OK)
        {
            *address += nBytesA;
            result = comms->read(i2cAddress + page, *address, data, nBytesB);
            if (result == globals::OK) *address += nBytesB;
        }
    }

    DEBUG_EEPROM("[EEPROM Read Bytes]: Page: " << page << "; Addr AFTER: " << INT_AS_HEX(*address,4) << "; Count: " << nBytes << "; Data: " << INT_AS_HEX(data[0], 2) << "... Result: " << result)
    return result;
}


/*!
 * \brief Wait for store operation to finish
 * The EEPROM takes some time (up to 10 ms) after a write operation,
 * for the data to be written to the memory cells. During this time
 * it ignores I2C requests. This method repeatedly sends a request
 * with no data, until it receives an ACK (indicating the write
 * has finished).
 * \return globals::OK  Success - store has completed
 * \return [error code]
 */
int M24M02::waitStoreAck()
{
    DEBUG_EEPROM("EEPROM Write: Wait for ACK...")
    int result = globals::OK;
    int retryCount = 0;
    while (retryCount < 5)
    {
        DEBUG_EEPROM("-Try " << retryCount << "...")
        result = comms->writeRaw(i2cAddress, 0, 0);
        if (result == globals::OK)
        {
            DEBUG_EEPROM("--ACK!")
            break;
        }
        else
        {
            DEBUG_EEPROM("--No ACK (" << result << "). Waiting...")
        }
        retryCount++;
        globals::sleep(2);
    }
    if (result != globals::OK)
    {
        DEBUG_EEPROM("--WARNING: TIMED OUT without ACK!")
    }

    return result;
}



/*!
 \brief Store a UInt 16 value to M24M02 EEPROM as two bytes in LE format
 \param page     Page number (0 - 3)
 \param address  EEPROM address to store to (lower 16 bits; i.e. address within page)
                 On SUCCESS, address is automatically incremented by 2
 \param value    Value to store
 \param checkSum Pointer to a uint16 checksum. If specified, each of the bytes from
                 value is added to the checksum (only on success).
                 OPTIONAL; may be nullptr
 \return globals::OK  Success
 \return [error code]
*/
int M24M02::storeUInt16(uint8_t page, uint16_t *address, uint16_t value, uint16_t *checkSum)
{
    uint8_t dataBuffer[2] = { 0 };
    dataBuffer[0] = static_cast<uint8_t>(value & 0x00FF);  // Little endian format.
    dataBuffer[1] = static_cast<uint8_t>(value >> 8);      //

    int result = storeBytes(page, address, dataBuffer, 2);
    if (result == globals::OK &&
        checkSum != nullptr) (*checkSum) += dataBuffer[0] + dataBuffer[1];
    return result;
}




/*!
 \brief Load a UInt 16 value from M24M02 EEPROM as two bytes in LE format
 \param page     Page number (0 - 3)
 \param address  EEPROM address to load from (lower 16 bits; i.e. address within page)
                 On SUCCESS, address is automatically incremented by 2
 \param value    Recieves the value
 \param checkSum Pointer to a uint16 checksum. If specified, each of the bytes from
                 value is added to the checksum (only on success).
                 OPTIONAL; may be nullptr
 \return globals::OK  Success
 \return [error code]
*/
int M24M02::loadUInt16(uint8_t page, uint16_t *address, uint16_t *value, uint16_t *checkSum)
{
    uint8_t dataBuffer[2] = { 0 };
    int result = loadBytes(page, address, dataBuffer, 2);

    if (result == globals::OK)
    {
        *value = static_cast<uint16_t>(dataBuffer[1] << 8) | dataBuffer[0];  // Little endian format
        if (checkSum != nullptr) (*checkSum) += dataBuffer[0] + dataBuffer[1];
    }
    return result;
}




/*!
 \brief Store a block of byte data to M24M02 EEPROM
 \param page      Page to use (0 - 3)
 \param address   Starting address for write (0 = start of page)
                  On SUCCESS, address is automatically incremented by the number of bytes stored.
 \param data      Pointer to buffer of data to write
 \param nBytes    Number of bytes to write. If address + nBytes exceeds
                  the page size of 65536 bytes, nothing will be written and an
                  error will be returned.
 \return globals::OK        Bytes writtten
 \return globals::OVERFLOW  Insufficient space - write would overflow the end of the page
 \return [error code]       Comms error, etc.
*/
int M24M02::storeBlock(uint8_t page, uint16_t *address, uint8_t *data, uint16_t nBytes)
{
    // Check for write past end of page:
    Q_ASSERT( (static_cast<int>(*address) + static_cast<int>(nBytes)) <= 65536 );
    if ((static_cast<int>(*address) + static_cast<int>(nBytes)) > 65536) return globals::OVERFLOW;

    int bytesLeftToWrite = nBytes; // Number of bytes left to write
    int srcOffset = 0;
    int result = globals::OK;

    while (bytesLeftToWrite > 0)
    {
        int bytesThisWrite = (bytesLeftToWrite <= WRITE_BLK_SIZE) ? bytesLeftToWrite : WRITE_BLK_SIZE;

        result = storeBytes(page, address, data + srcOffset, static_cast<uint8_t>(bytesThisWrite));
        if (result != globals::OK) return result;   // EEPROM write error!
        srcOffset += bytesThisWrite;
        bytesLeftToWrite -= bytesThisWrite;
    }
    return result;
}



/*!
 \brief Load a block of byte data from M24M02 EEPROM
 \param page      Page to use (0 - 3)
 \param address   Starting address for read (0 = start of page)
                  On SUCCESS, address is automatically incremented by the number of bytes stored.
 \param data      Pointer to buffer to store data (created by caller)
 \param nBytes    Number of bytes to read. If address + nBytes exceeds
                  the page size of 65536 bytes, nothing will be read and an
                  error will be returned.
 \return globals::OK        Bytes writtten
 \return globals::OVERFLOW  Insufficient space - read would overflow the end of the page
 \return [error code]       Comms error, etc.
*/
int M24M02::loadBlock(uint8_t page, uint16_t *address, uint8_t *data, uint16_t nBytes)
{
    // Check for read past end of page:
    Q_ASSERT( (static_cast<int>(*address) + static_cast<int>(nBytes)) <= 65536 );
    if ((static_cast<int>(*address) + static_cast<int>(nBytes)) > 65536) return globals::OVERFLOW;

    int bytesLeftToRead = nBytes;
    int destOffset = 0;
    int result = globals::OK;

    while (bytesLeftToRead > 0)
    {
        int bytesThisRead = (bytesLeftToRead <= READ_BLK_SIZE) ? bytesLeftToRead : READ_BLK_SIZE;

        result = loadBytes(page, address, data + destOffset, static_cast<uint8_t>(bytesThisRead));
        if (result != globals::OK) return result;   // EEPROM read error!

        destOffset += bytesThisRead;
        bytesLeftToRead -= bytesThisRead;
    }
    return globals::OK;
}




/*!
 * \brief Store a String
 *   The string is stored to the address specified by the StringInfo data. If it is longer than
 *   maxLength characters, it is truncated to fit. It may be shorter than maxLength, and an empty
 *   string ("") is acceptable.
 * \param stringID    Identifier for string meta data in the string info table - see StringID enum
 * \param stringData  String to store
 * \return globals::OK   Stored OK
 * \return [error code]
 */
int M24M02::storeString(StringID stringID, const QString &stringData)
{
    Q_ASSERT(strings.contains(stringID));
    if (!strings.contains(stringID)) return globals::OVERFLOW; // String not found! Invalid string ID.

    StringInfo stringInfo = strings.value(stringID);
    int result = globals::OK;

    uint16_t address = stringInfo.address;
    uint8_t NUL = 0x00;
    DEBUG_EEPROM("EEPROM: Storing string " << stringID << " at " << address << ": '" << stringData << "'")

    uint8_t data[WRITE_BLK_SIZE];  // Temp buffer for reading blocks from eeprom; Max 59 bytes per write (I2C adaptor limit)

    int bytesLeftToWrite = (stringData.length() <= stringInfo.maxLength) ? stringData.length() : stringInfo.maxLength;
      // Number of bytes left to write; If string is longer than max length for this location, truncate.

    int srcOffset = 0;
    while (bytesLeftToWrite > 0)
    {
        int bytesThisWrite = (bytesLeftToWrite <= WRITE_BLK_SIZE) ? bytesLeftToWrite : WRITE_BLK_SIZE;

        // Add characters to the temp buffer, stopping if we get to the end of the string:
        for (int i = 0; i < bytesThisWrite; i++)
        {
            data[i] = static_cast<uint8_t>(stringData[i+srcOffset].toLatin1());
        }
        result = storeBytes(PAGE_STRINGS, &address, data, static_cast<uint8_t>(bytesThisWrite));
        if (result != globals::OK) return result;   // EEPROM write error!

        srcOffset += bytesThisWrite;
        bytesLeftToWrite -= bytesThisWrite;
    }

    // If we didn't fill the max space allocated for this string, add a NUL:
    if (address < (stringInfo.address + stringInfo.maxLength)) result = storeBytes(PAGE_STRINGS, &address, &NUL, 1);
    return result;
}



/*!
 * \brief Load a String
 *   The string is loaded from the address specified by the StringInfo data.
 * \param stringID    Identifier for string meta data in the string info table - see StringID enum
 * \param stringData  String reference to store the loaded string into
 * \return globals::OK   Stored OK
 * \return [error code]
 */
int M24M02::loadString(StringID stringID, QString &stringData)
{
    Q_ASSERT(strings.contains(stringID));
    stringData.clear();   // Erase any existing string data
    if (!strings.contains(stringID)) return globals::OVERFLOW; // String not found! Invalid string ID.

    StringInfo stringInfo = strings.value(stringID);
    if (stringInfo.maxLength <= 0) return globals::OK;   // Zero length string! Nothing to do.

    int result;
    DEBUG_EEPROM("EEPROM: Loading string " << stringID << " from " << stringInfo.address)

    uint8_t data[READ_BLK_SIZE];  // Temp buffer for reading blocks from eeprom; Max 64 bytes per read (I2C adaptor limit)
    uint16_t address = stringInfo.address;
    int bytesLeftToRead = stringInfo.maxLength;
    while (true)
    {
        int bytesThisRead = (bytesLeftToRead <= READ_BLK_SIZE) ? bytesLeftToRead : READ_BLK_SIZE;

        result = loadBytes(PAGE_STRINGS, &address, data, static_cast<uint8_t>(bytesThisRead));
        if (result != globals::OK) return result;   // EEPROM read error!

        // Add characters to the returned string, stopping if we get a NUL:
        for (int i = 0; i < READ_BLK_SIZE; i++)
        {
            if (data[i] == 0x00 || data[i] == 0xFF) return globals::OK;
              // NUL terminator or no data: End of string reached (C-style string).
              // Note that 0xFF is the default content of a new EEPROM.
            QChar qCh = QChar(static_cast<char>(data[i]));
            stringData.append(qCh);
        }
        bytesLeftToRead -= bytesThisRead;
        if (bytesLeftToRead <= 0) break;
    }
    return globals::OK;
}


/*!
 \brief Store an LMX Frequency Profile
 \param address  Address to store to
                 On SUCCESS, address is automatically incremented by the number of bytes stored.
 \param profile  Profile to store
 \return globals::OK  Stored OK
 \return [Error code]
*/
int M24M02::storeFrequencyProfile(uint16_t *address, LMXFrequencyProfile &profile)
{
    int result;
    uint16_t checkSum = 0;
    uint16_t value = 0;
    uint8_t registerAddress = 0;

    DEBUG_EEPROM_EXTRA("storeFrequencyProfile: Address: " << *address << "; Frequency: " << profile.getFrequency())

    // -- Frequency, converted to array of 4 bytes: --
    float frequency = profile.getFrequency();
    Q_ASSERT(sizeof frequency == 4);
    if (sizeof frequency != 4) return globals::GEN_ERROR; // Paranoid.

    // Create a temp buffer in memory, to make writing to EEPROM faster:
    uint16_t profileSize = 4    // Frequency
                         + 2    // Register Count
                         + static_cast<uint16_t>(profile.getRegisterCount() * 2)  // Register Values
                         + 2;   // Checksum
    uint8_t *dataBuffer = new uint8_t[profileSize];
    if (!dataBuffer) return globals::MALLOC_ERROR;
    int bufferAddress = 0;

    DEBUG_EEPROM_EXTRA("-N Registers:  " << profile.getRegisterCount() << "; N Bytes: " << profileSize)
    DEBUG_EEPROM_EXTRA("Checksum:++0:" << checkSum)
    memcpy(dataBuffer + bufferAddress, &frequency, 4);
    checkSum += dataBuffer[0] + dataBuffer[1] + dataBuffer[2] + dataBuffer[3];
    bufferAddress += 4;
    DEBUG_EEPROM_EXTRA("Checksum:++C:" << checkSum)

    // -- Number of registers: --
    value = static_cast<uint16_t>(profile.getRegisterCount());
    dataBuffer[bufferAddress]   = static_cast<uint8_t>(value & 0x00FF);
    dataBuffer[bufferAddress+1] = static_cast<uint8_t>(value >> 8);
    checkSum += dataBuffer[bufferAddress] + dataBuffer[bufferAddress+1];
    bufferAddress += 2;

    DEBUG_EEPROM_EXTRA("Checksum:++N:" << checkSum)

    // -- Register Values: --
    for (registerAddress = 0; registerAddress < profile.getRegisterCount(); registerAddress++)
    {
        value = static_cast<uint16_t>(profile.getRegisterValue(registerAddress, nullptr));
        dataBuffer[bufferAddress]   = static_cast<uint8_t>(value & 0x00FF);
        dataBuffer[bufferAddress+1] = static_cast<uint8_t>(value >> 8);
        DEBUG_EEPROM_EXTRA("  ->Write:" << INT_AS_HEX(dataBuffer[bufferAddress],2) << INT_AS_HEX(dataBuffer[bufferAddress + 1],2) << ": " << value)
        checkSum += dataBuffer[bufferAddress] + dataBuffer[bufferAddress + 1];
        bufferAddress += 2;
        DEBUG_EEPROM_EXTRA("Checksum:  ++R:" << checkSum)
    }

    DEBUG_EEPROM_EXTRA("Checksum Final: " << checkSum)
    // -- Checksum: --
    dataBuffer[bufferAddress]   = static_cast<uint8_t>(checkSum & 0x00FF);
    dataBuffer[bufferAddress+1] = static_cast<uint8_t>(checkSum >> 8);
    bufferAddress += 2;

    result = storeBlock(PAGE_FREQ_PROFILES, address, dataBuffer, profileSize);

    delete [] dataBuffer;
    DEBUG_EEPROM_EXTRA("-Result:  " << result << "; Checksum: " << checkSum << "; Address now: " << *address)
    return result;
}





int M24M02::loadFrequencyProfile(uint16_t *address, LMXFrequencyProfile &profile)
{
    int result = globals::OK;
    uint16_t checksumCalculated = 0;

    DEBUG_EEPROM_EXTRA("loadFrequencyProfile: From Address: " << *address)

    uint8_t frequencyData[4] = { 0 };  // Frequency as a byte array
    result = loadBlock(PAGE_FREQ_PROFILES, address, frequencyData, 4);
    if (result != globals::OK) return result;

    if (frequencyData[0] == 0xFF
     && frequencyData[1] == 0xFF
     && frequencyData[2] == 0xFF
     && frequencyData[3] == 0xFF)
    {
        // End of data marker!
        // NB: This system is deprecated in favour of having a profile count at the start of the data.
        // However, Unused EEPROM generally reads as 0xFF so this functions as a sanity check.
        // Frequency should never be 0xFFFFFFFF.
        DEBUG_EEPROM_EXTRA("-Found END OF DATA")
        return globals::END_OF_DATA;
    }

    DEBUG_EEPROM_EXTRA("Checksum:--0:" << checksumCalculated)

    float frequency = 0.0f;
    Q_ASSERT(sizeof frequency == 4);
    if (sizeof frequency != 4) return globals::GEN_ERROR;  // Paranoid.
    memcpy(&frequency, frequencyData, 4);
    profile.setFrequency(frequency);
    checksumCalculated += frequencyData[0] + frequencyData[1] + frequencyData[2] + frequencyData[3];

    DEBUG_EEPROM_EXTRA("-Found Frequency: " << frequency << "; Address now: " << *address)
    DEBUG_EEPROM_EXTRA("Checksum:--C:" << checksumCalculated)

    // -- Number of Registers: --
    uint16_t registerCount = 0;
    result = loadUInt16(PAGE_FREQ_PROFILES, address, &registerCount, &checksumCalculated);
    if (result != globals::OK) return result;
    if (registerCount > 255) return globals::INVALID_DATA;  // Sanity check
    profile.setRegisterCount(registerCount);
    uint16_t readSize = (registerCount * 2) + 2;   // Data for registers, plus checksum

    DEBUG_EEPROM_EXTRA("-Found Register Count: " << registerCount << "; Address now: " << *address << "; Reading " << readSize << " more bytes.")
    DEBUG_EEPROM_EXTRA("Checksum:--N:" << checksumCalculated)

    // -- Register data and checksum: --
    uint8_t *dataBuffer = new uint8_t[readSize];
    if (!dataBuffer) return globals::MALLOC_ERROR;

    uint16_t bufferAddress = 0;
    uint16_t checkSumStored = 0;

    result = loadBlock(PAGE_FREQ_PROFILES, address, dataBuffer, readSize);
    if (result != globals::OK) goto cleanup;

    DEBUG_EEPROM_EXTRA("-Data read. Address now: " << *address)

    // Add registers to the profile, and calculate the checksum:
    for (uint8_t registerAddress = 0; registerAddress < registerCount; registerAddress++)
    {
        uint16_t value = static_cast<uint16_t>(dataBuffer[bufferAddress + 1] << 8) | dataBuffer[bufferAddress];
        profile.setRegisterValue(registerAddress, value);
        DEBUG_EEPROM_EXTRA("  ->Read:" << INT_AS_HEX(dataBuffer[bufferAddress],2) << INT_AS_HEX(dataBuffer[bufferAddress + 1],2) << ": " << value)
        checksumCalculated += dataBuffer[bufferAddress] + dataBuffer[bufferAddress + 1];
        bufferAddress += 2;
        DEBUG_EEPROM_EXTRA("Checksum:  --R:" << checksumCalculated)
    }

    DEBUG_EEPROM_EXTRA("Checksum Final: " << checksumCalculated)
    // Extract the stored checksum:
    checkSumStored = static_cast<uint16_t>(dataBuffer[bufferAddress + 1] << 8) | dataBuffer[bufferAddress];

    DEBUG_EEPROM_EXTRA("-Extracted Checksum from read data. Checksum test: Stored = " << checkSumStored << "; Calculated = " << checksumCalculated)

    if (checkSumStored == checksumCalculated)
    {
        DEBUG_EEPROM_EXTRA("-Checksum Match.")
        profile.setValid();
    }
    else
    {
        DEBUG_EEPROM("WARNING: loadFrequencyProfile: Checksum Match!")
        result = globals::BAD_CHECKSUM;
    }

  cleanup:
    delete [] dataBuffer;
    DEBUG_EEPROM_EXTRA("-Result:  " << result << "; Address now: " << *address)
    return result;
}





/*!
 * \brief Clear EEPROM Memory
 * This method resets the contents of the string table to 0xFF (factory default).
 * This is mostly for debugging purposes.
 * \return globals::OK   EEPROM responded OK; string table cleared
 * \return [Error code]  Error occurred (comms not connected, etc).
 */
int M24M02::clearEEPROM()
{
    int result;
    uint8_t BLANK = 0xFF;
    foreach(StringID id, strings.keys())
    {
        const uint16_t startAddr = strings[id].address;
        const uint16_t endAddr = startAddr + static_cast<uint16_t>(strings[id].maxLength);
        DEBUG_EEPROM("CLEAR String " << id << ": Addr " << startAddr << " to " << endAddr-1)
        for (uint16_t address = startAddr;
                      address < endAddr;
                      address++)
        {
            result = storeBytes(PAGE_STRINGS, &address, &BLANK, 1);
            if (result != globals::OK) return result;
        }
    }
    return globals::OK;
}

/*!
 * \brief Clear EEPROM PAGE_FREQ_PROFILES Memory
 * This method resets the contents of the string table to 0xFF (factory default).
 * Before write new Freq profiles, delete the previous profiles in EEPROM.
 * \return globals::OK   EEPROM responded OK; profiles cleared
 * \return [Error code]  Error occurred (comms not connected, etc).
 */
int M24M02::clearPROFILES()
{
    int result;
    uint8_t BLANK = 0xFF;
    const uint16_t startAddr = 0x0000;
    const uint16_t endAddr = 0xFFFF;
    DEBUG_EEPROM("CLEAR Freq Profiles: Addr " << startAddr << " to " << endAddr-1)
    for (uint16_t address = startAddr;
                      address < endAddr;
                      address++)
        {
            result = storeBytes(PAGE_FREQ_PROFILES, &address, &BLANK, 1);
            if (result != globals::OK) return result;
        }

    return globals::OK;
}


