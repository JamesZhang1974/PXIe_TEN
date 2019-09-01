/*!
 \file   SI5340.cpp
 \brief  Functional commands to control a Silicon Labs SI5340 low jitter clock generator - Implementation

 \author J Cole-Baker (For Smartest)
 \date   Dec 2018
*/

#include <QDebug>

#include "globals.h"
#include "SI5340.h"


// Configurable Debug Macro for SI5340: Turn on for extra debug.
#define BERT_SI5340_DEBUG
#define BERT_SI5340_DEBUG_EXTRA

#ifdef BERT_SI5340_DEBUG
  #define DEBUG_SI5340(MSG) qDebug() << "\t\t" << MSG;
  #ifdef BERT_SI5340_DEBUG_EXTRA
    #define DEBUG_SI5340_EXTRA(MSG) qDebug() << "\t\t\t" << MSG;
  #else
    #define DEBUG_SI5340_EXTRA(MSG)
  #endif
#else
  #define DEBUG_SI5340(MSG)
  #define DEBUG_SI5340_EXTRA(MSG)
#endif



SI5340::SI5340(I2CComms *comms, const uint8_t i2cAddress, const int deviceID)
 : comms(comms), i2cAddress(i2cAddress), deviceID(deviceID)
{
    frequencyIn = 0.0;
    frequencyOut = 0.0;
    descriptionIn = "";
    descriptionOut = "";
}


//*****************************************************************************************
//   SI5340 Methods
// *****************************************************************************************

/*!
 \brief Check to see whether there is an SI5340 on the specified I2C address.
 \param comms       I2C Comms (created elsewhere)
 \param i2cAddress  I2C address to check on (7 bit, i.e. no R/W bit)
 \return  true   SI5340 responded OK
 \return  false  No response... clock not found.
*/
bool SI5340::ping(I2CComms *comms, const uint8_t i2cAddress)
{
    qDebug() << "SI5340: Searching for clock synth on address " << INT_AS_HEX(i2cAddress,2) << "...";

    Q_ASSERT(comms->portIsOpen());
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;

    // REMOVE for testing!
    int result;
    result = comms->pingAddress(i2cAddress);
    if (result != globals::OK)
    {
        qDebug() << "SI5340 not found (no ACK on I2C address; result: " << result << ")";
        return false;
    }
    // /

    // SI5340 responded.
    qDebug() << "SI5340 found.";
    return true;
}



/*!
 \brief Get Hardware Options for SI5340
        Requests that this module emit signals describing its available options lists to client
*/
void SI5340::getOptions()
{
    emit ListPopulate("listRefClockProfiles", globals::ALL_LANES, PROFILE_LIST, DEFAULT_PROFILE);
}


/*!
 \brief Initialise SI5340
 Assumes that general comms are already open (bertComms object).
 \return globals::OK   Success
 \return [error code]  Failed to initialise the SI5340
*/
int SI5340::init()
{
    qDebug() << "SI5340: Init for ID " << deviceID << "; I2C Address " << INT_AS_HEX(i2cAddress,2);
    int result = selectProfile(DEFAULT_PROFILE);
    return result;
}


// =================================================================================
//  SLOTS
// =================================================================================

/*!
 \brief Get Reference Clock Info Slot: Request for info about current reference clock settings.
*/
void SI5340::GetRefClockInfo()
{
    emit RefClockInfo(deviceID,
                      selectedProfileIndex,
                      frequencyIn,
                      frequencyOut,
                      descriptionIn,
                      descriptionOut);
}



/*!
 \brief Slot: Select ref clock generator profile
 \param indexProfile   Index of profile to select
 \param triggerResync  OPTIONAL: Should this change trigger a resync of other components?
                       If true (default), a "RefClock Settings Changed" signal is emitted.
                       Set to false if caller is going to do their own resync of other
                       components, or doesn't care.
*/
void SI5340::RefClockSelectProfile(int indexProfile, bool triggerResync)
{
    DEBUG_SI5340("SI5340: Select ref clock generator profile " << indexProfile)
    emit ShowMessage("Changing Reference Clock...");
    int result = selectProfile(indexProfile);
    if (result == globals::OK)
    {
        emit Result(globals::OK, globals::ALL_LANES);
        emit ShowMessage("OK.");
    }
    else
    {
        emit ShowMessage("Error selecting reference clock!");
        emit Result(result, globals::ALL_LANES);
    }
    GetRefClockInfo();                                         // Update the client to reflect the new settings
    if (triggerResync) emit RefClockSettingsChanged(deviceID); // Inform the client that settings have changed - other parts of the system may need to resync.
}



/*****************************************************************************************/
/* PRIVATE Methods                                                                       */
/*****************************************************************************************/

int SI5340::selectProfile(int index)
{
    Q_ASSERT(CONFIG_PROFILES.contains(index));
    if (!CONFIG_PROFILES.contains(index)) return globals::OVERFLOW;

    DEBUG_SI5340("SI5340: Select Profile " << index)

    // Get a reference to the register values for the new profile:
    const QList<SI5340Register_t> newProfile = CONFIG_PROFILES[index];

    int result = globals::OK;

    // PREAMBLE: Load preamble registers:
    DEBUG_SI5340("SI5340: -Load Preamble Registers...")
    foreach (SI5340Register_t preambleRegister, CONFIG_PREAMBLE)
    {
        // Extract 8-bit page and 8-bit address from 16 bit address:
        uint8_t page = static_cast<uint8_t>(preambleRegister.address >> 8);
        uint8_t address = static_cast<uint8_t>(preambleRegister.address & 0xFF);

        result = writeRegister(page, address, preambleRegister.value);
        if (result != globals::OK) return result;
    }

    // SLEEP after preamble (See data sheet!):
    globals::sleep(PREAMBLE_SLEEP_MS);

    // Load Registers:
    DEBUG_SI5340("SI5340: -Load Profile Registers...")
    foreach (SI5340Register_t profileRegister, newProfile)
    {
        // Extract 8-bit page and 8-bit address from 16 bit address:
        uint8_t page = static_cast<uint8_t>(profileRegister.address >> 8);
        uint8_t address = static_cast<uint8_t>(profileRegister.address & 0xFF);

        result = writeRegister(page, address, profileRegister.value);
        if (result != globals::OK) return result;
    }

    // POSTAMBLE: Load postamble registers:
    DEBUG_SI5340("SI5340: -Load Postamble Registers...")
    foreach (SI5340Register_t postambleRegister, CONFIG_POSTAMBLE)
    {
        // Extract 8-bit page and 8-bit address from 16 bit address:
        uint8_t page = static_cast<uint8_t>(postambleRegister.address >> 8);
        uint8_t address = static_cast<uint8_t>(postambleRegister.address & 0xFF);

        result = writeRegister(page, address, postambleRegister.value);
        if (result != globals::OK) return result;
    }

    // Profile selected. Update frequency info and descriptions:
    // HACKY: This hard-coded look-up has been created for testing as we're not yet sure
    // how this will work. Could / Should be improved in the future?
    switch (index)
    {
    case 0:
        // Optionl A: "On board Oscillator - 100.0M"
        frequencyIn = 100.0;
        frequencyOut = 100.0;
        descriptionIn = "On board XO";
        descriptionOut = "100 MHz";
        break;
    case 1:
        // Optionl A: "On board Oscillator - 156.25M"
        frequencyIn = 156.25;
        frequencyOut = 156.25;
        descriptionIn = "On board XO";
        descriptionOut = "156.25 MHz";
        break;
    case 2:
        // Optionl A: "On board Oscillator - 161.13281M"
        frequencyIn = 161.13281;
        frequencyOut = 161.13281;
        descriptionIn = "On board XO";
        descriptionOut = "161.13281 MHz";
        break;
    case 3:
        // Option B: "EXT. 10 MHz"
        frequencyIn = 10.0;
        frequencyOut = 100.0;
        descriptionIn = "10 MHz (External)";
        descriptionOut = "100 MHz";
        break;
    case 4:
        // Option C: "EXT. 100 MHz"
        frequencyIn = 100.0;
        frequencyOut = 100.0;
        descriptionIn = "100 MHz (External)";
        descriptionOut = "100 MHz";
        break;
    default:
        // Unknown profile
        frequencyIn = 0.0;
        frequencyOut = 0.0;
        descriptionIn = "";
        descriptionOut = "";
    }

    selectedProfileIndex = index;
    return globals::OK;
}


/*!
 \brief Select Register Page
 The SI5340 uses register "pages". This method checks whether the specified
 page is currently selected, and selects it if not.
 \param page  Page to select (for subsequent register read / write operations)
 \return globals::OK   Success
 \return [error code]  Failed! Page may not have changed.
*/
int SI5340::selectPage(uint8_t page)
{
    if (pageValid && page == previousPage) return globals::OK;   // Nothing to to; page already selected.

    DEBUG_SI5340("SI5340: Select register page: " << page)

    // Register Address 0x01 on each page is the "Set Page Address" register:
    int result = comms->write8(i2cAddress, 0x01, &page, 1);

    if (result == globals::OK)
    {
        // Success:
        previousPage = page;
        pageValid = true;
        return globals::OK;
    }
    else
    {
        // Error: Set the "pageValid" flag to false - we're not sure which page is selected now.
        DEBUG_SI5340("SI5340: Comms error selecting register page: " << result)
        pageValid = false;
        return result;
    }
}


/*!
 \brief Write to SI5340 register
 \param page      Register page
 \param address   Regster address
 \param data      Data to write (1 byte)
 \return globals::OK  Success
 \return [error code]
*/
int SI5340::writeRegister(uint8_t page, uint8_t address, uint8_t data)
{
    DEBUG_SI5340_EXTRA("SI5340: Register Write"
                       << ": Page=" << INT_AS_HEX(page,2)
                       << "; Address=" << INT_AS_HEX(address,2)
                       << "; Data=" << INT_AS_HEX(data,2))

    int result = selectPage(page);
    if (result != globals::OK) return result;            

    result = comms->write8(i2cAddress, address, &data, 1);

#ifdef BERT_SI5340_DEBUG
    if (result != globals::OK) DEBUG_SI5340("SI5340: Comms error writing to register"
                                             << "; Page=" << INT_AS_HEX(page,2)
                                             << "; Address=" << INT_AS_HEX(address,2)
                                             << "; Data=" << INT_AS_HEX(data,2)
                                             << "; Result: " << result)
#endif

    return result;
}


/*!
 \brief Read from SI5340 register
 \param page      Register page
 \param address   Regster address
 \param data      Pointer to location to store data (1 byte)
 \return globals::OK  Success
 \return [error code]
*/
int SI5340::readRegister(uint8_t page, uint8_t address, uint8_t *data)
{
    DEBUG_SI5340_EXTRA("SI5340: Register Read"
                       << ": Page=" << INT_AS_HEX(page,2)
                       << "; Address=" << INT_AS_HEX(address,2))

    int result = selectPage(page);
    if (result != globals::OK) return result;

    // For read operation: First, we have to do a write with no data to set the
    // register address we want to read from - See SI5341 datasheet Section 9.1:
    result = comms->writeRaw(i2cAddress, &address, 1);

#ifdef BERT_SI5340_DEBUG
    if (result != globals::OK) DEBUG_SI5340("SI5340: Comms error setting up register address for read"
                                            << "; Page=" << INT_AS_HEX(page,2)
                                            << "; Address=" << INT_AS_HEX(address,2)
                                            << "; Result: " << result)
#endif

    if (result != globals::OK) return result;

    // Now do a read operation with no address:
    result = comms->readRaw(i2cAddress, data, 1);

#ifdef BERT_SI5340_DEBUG
    if (result != globals::OK) DEBUG_SI5340("SI5340: Comms error reading from register"
                                            << "; Page=" << INT_AS_HEX(page,2)
                                            << "; Address=" << INT_AS_HEX(address,2)
                                            << "; Result: " << result)
#endif

    return result;
}







// ***************************************************************************
//  Register Config Profiles
//  These are just hard-coded with three profiles generated using the
//  Silicon Labs software tool.
// ***************************************************************************

const QStringList SI5340::PROFILE_LIST =
{
    "On Board Oscillator - 100.00MHz",
    "On Board Oscillator - 156.25MHz",
    "On Board Oscillator - 161.13281MHz",
    "EXT. 10 MHz",
    "EXT. 100 MHz",
};


const QList<SI5340::SI5340Register_t> SI5340::CONFIG_PREAMBLE =
{
    /* Start configuration preamble */
    { 0x0B24, 0xC0 },
    { 0x0B25, 0x00 },
    /* Rev D stuck divider fix */
    { 0x0502, 0x01 },
    { 0x0505, 0x03 },
    { 0x0957, 0x17 },
    { 0x0B4E, 0x1A }
    /* End configuration preamble */
};


const QList<SI5340::SI5340Register_t> SI5340::CONFIG_POSTAMBLE =
{
    /* Start configuration postamble */
    { 0x001C, 0x01 },
    { 0x0B24, 0xC3 },
    { 0x0B25, 0x02 }
    /* End configuration postamble */
};


const QMap<int, QList<SI5340::SI5340Register_t>> SI5340::CONFIG_PROFILES =
  QMap<int, QList<SI5340::SI5340Register_t>>
  (
    {
            // "On Board Oscillator - 100.00M":
            {
              0,  // Profile Index
              {
                          /* Start configuration registers */
                    { 0x0006, 0x00 },
                    { 0x0007, 0x00 },
                    { 0x0008, 0x00 },
                    { 0x000B, 0x74 },
                    { 0x0017, 0xD0 },
                    { 0x0018, 0xFF },
                    { 0x0021, 0x0F },
                    { 0x0022, 0x00 },
                    { 0x002B, 0x02 },
                    { 0x002C, 0x20 },
                    { 0x002D, 0x00 },
                    { 0x002E, 0x00 },
                    { 0x002F, 0x00 },
                    { 0x0030, 0x00 },
                    { 0x0031, 0x00 },
                    { 0x0032, 0x00 },
                    { 0x0033, 0x00 },
                    { 0x0034, 0x00 },
                    { 0x0035, 0x00 },
                    { 0x0036, 0x00 },
                    { 0x0037, 0x00 },
                    { 0x0038, 0x00 },
                    { 0x0039, 0x00 },
                    { 0x003A, 0x00 },
                    { 0x003B, 0x00 },
                    { 0x003C, 0x00 },
                    { 0x003D, 0x00 },
                    { 0x0041, 0x00 },
                    { 0x0042, 0x00 },
                    { 0x0043, 0x00 },
                    { 0x0044, 0x00 },
                    { 0x009E, 0x00 },
                    { 0x0102, 0x01 },
                    { 0x0112, 0x06 },
                    { 0x0113, 0x09 },
                    { 0x0114, 0x3E },
                    { 0x0115, 0x18 },
                    { 0x0117, 0x06 },
                    { 0x0118, 0x09 },
                    { 0x0119, 0x3E },
                    { 0x011A, 0x18 },
                    { 0x0126, 0x06 },
                    { 0x0127, 0x09 },
                    { 0x0128, 0x3E },
                    { 0x0129, 0x18 },
                    { 0x012B, 0x01 },
                    { 0x012C, 0x09 },
                    { 0x012D, 0x3B },
                    { 0x012E, 0x28 },
                    { 0x013F, 0x00 },
                    { 0x0140, 0x00 },
                    { 0x0141, 0x40 },
                    { 0x0206, 0x00 },
                    { 0x0208, 0x00 },
                    { 0x0209, 0x00 },
                    { 0x020A, 0x00 },
                    { 0x020B, 0x00 },
                    { 0x020C, 0x00 },
                    { 0x020D, 0x00 },
                    { 0x020E, 0x00 },
                    { 0x020F, 0x00 },
                    { 0x0210, 0x00 },
                    { 0x0211, 0x00 },
                    { 0x0212, 0x00 },
                    { 0x0213, 0x00 },
                    { 0x0214, 0x00 },
                    { 0x0215, 0x00 },
                    { 0x0216, 0x00 },
                    { 0x0217, 0x00 },
                    { 0x0218, 0x00 },
                    { 0x0219, 0x00 },
                    { 0x021A, 0x00 },
                    { 0x021B, 0x00 },
                    { 0x021C, 0x00 },
                    { 0x021D, 0x00 },
                    { 0x021E, 0x00 },
                    { 0x021F, 0x00 },
                    { 0x0220, 0x00 },
                    { 0x0221, 0x00 },
                    { 0x0222, 0x00 },
                    { 0x0223, 0x00 },
                    { 0x0224, 0x00 },
                    { 0x0225, 0x00 },
                    { 0x0226, 0x00 },
                    { 0x0227, 0x00 },
                    { 0x0228, 0x00 },
                    { 0x0229, 0x00 },
                    { 0x022A, 0x00 },
                    { 0x022B, 0x00 },
                    { 0x022C, 0x00 },
                    { 0x022D, 0x00 },
                    { 0x022E, 0x00 },
                    { 0x022F, 0x00 },
                    { 0x0235, 0x00 },
                    { 0x0236, 0x00 },
                    { 0x0237, 0x00 },
                    { 0x0238, 0x80 },
                    { 0x0239, 0x89 },
                    { 0x023A, 0x00 },
                    { 0x023B, 0x00 },
                    { 0x023C, 0x00 },
                    { 0x023D, 0x00 },
                    { 0x023E, 0x80 },
                    { 0x0250, 0x00 },
                    { 0x0251, 0x00 },
                    { 0x0252, 0x00 },
                    { 0x0253, 0x00 },
                    { 0x0254, 0x00 },
                    { 0x0255, 0x00 },
                    { 0x025C, 0x00 },
                    { 0x025D, 0x00 },
                    { 0x025E, 0x00 },
                    { 0x025F, 0x00 },
                    { 0x0260, 0x00 },
                    { 0x0261, 0x00 },
                    { 0x026B, 0x35 },
                    { 0x026C, 0x33 },
                    { 0x026D, 0x34 },
                    { 0x026E, 0x30 },
                    { 0x026F, 0x45 },
                    { 0x0270, 0x56 },
                    { 0x0271, 0x42 },
                    { 0x0272, 0x31 },
                    { 0x0302, 0x00 },
                    { 0x0303, 0x00 },
                    { 0x0304, 0x00 },
                    { 0x0305, 0x00 },
                    { 0x0306, 0x21 },
                    { 0x0307, 0x00 },
                    { 0x0308, 0x00 },
                    { 0x0309, 0x00 },
                    { 0x030A, 0x00 },
                    { 0x030B, 0x80 },
                    { 0x030C, 0x00 },
                    { 0x030D, 0x00 },
                    { 0x030E, 0x00 },
                    { 0x030F, 0x00 },
                    { 0x0310, 0x00 },
                    { 0x0311, 0x00 },
                    { 0x0312, 0x00 },
                    { 0x0313, 0x00 },
                    { 0x0314, 0x00 },
                    { 0x0315, 0x00 },
                    { 0x0316, 0x00 },
                    { 0x0317, 0x00 },
                    { 0x0318, 0x00 },
                    { 0x0319, 0x00 },
                    { 0x031A, 0x00 },
                    { 0x031B, 0x00 },
                    { 0x031C, 0x00 },
                    { 0x031D, 0x00 },
                    { 0x031E, 0x00 },
                    { 0x031F, 0x00 },
                    { 0x0320, 0x00 },
                    { 0x0321, 0x00 },
                    { 0x0322, 0x00 },
                    { 0x0323, 0x00 },
                    { 0x0324, 0x00 },
                    { 0x0325, 0x00 },
                    { 0x0326, 0x00 },
                    { 0x0327, 0x00 },
                    { 0x0328, 0x00 },
                    { 0x0329, 0x00 },
                    { 0x032A, 0x00 },
                    { 0x032B, 0x00 },
                    { 0x032C, 0x00 },
                    { 0x032D, 0x00 },
                    { 0x0338, 0x00 },
                    { 0x0339, 0x1F },
                    { 0x033B, 0x00 },
                    { 0x033C, 0x00 },
                    { 0x033D, 0x00 },
                    { 0x033E, 0x00 },
                    { 0x033F, 0x00 },
                    { 0x0340, 0x00 },
                    { 0x0341, 0x00 },
                    { 0x0342, 0x00 },
                    { 0x0343, 0x00 },
                    { 0x0344, 0x00 },
                    { 0x0345, 0x00 },
                    { 0x0346, 0x00 },
                    { 0x0347, 0x00 },
                    { 0x0348, 0x00 },
                    { 0x0349, 0x00 },
                    { 0x034A, 0x00 },
                    { 0x034B, 0x00 },
                    { 0x034C, 0x00 },
                    { 0x034D, 0x00 },
                    { 0x034E, 0x00 },
                    { 0x034F, 0x00 },
                    { 0x0350, 0x00 },
                    { 0x0351, 0x00 },
                    { 0x0352, 0x00 },
                    { 0x0359, 0x00 },
                    { 0x035A, 0x00 },
                    { 0x035B, 0x00 },
                    { 0x035C, 0x00 },
                    { 0x035D, 0x00 },
                    { 0x035E, 0x00 },
                    { 0x035F, 0x00 },
                    { 0x0360, 0x00 },
                    { 0x0802, 0x00 },
                    { 0x0803, 0x00 },
                    { 0x0804, 0x00 },
                    { 0x0805, 0x00 },
                    { 0x0806, 0x00 },
                    { 0x0807, 0x00 },
                    { 0x0808, 0x00 },
                    { 0x0809, 0x00 },
                    { 0x080A, 0x00 },
                    { 0x080B, 0x00 },
                    { 0x080C, 0x00 },
                    { 0x080D, 0x00 },
                    { 0x080E, 0x00 },
                    { 0x080F, 0x00 },
                    { 0x0810, 0x00 },
                    { 0x0811, 0x00 },
                    { 0x0812, 0x00 },
                    { 0x0813, 0x00 },
                    { 0x0814, 0x00 },
                    { 0x0815, 0x00 },
                    { 0x0816, 0x00 },
                    { 0x0817, 0x00 },
                    { 0x0818, 0x00 },
                    { 0x0819, 0x00 },
                    { 0x081A, 0x00 },
                    { 0x081B, 0x00 },
                    { 0x081C, 0x00 },
                    { 0x081D, 0x00 },
                    { 0x081E, 0x00 },
                    { 0x081F, 0x00 },
                    { 0x0820, 0x00 },
                    { 0x0821, 0x00 },
                    { 0x0822, 0x00 },
                    { 0x0823, 0x00 },
                    { 0x0824, 0x00 },
                    { 0x0825, 0x00 },
                    { 0x0826, 0x00 },
                    { 0x0827, 0x00 },
                    { 0x0828, 0x00 },
                    { 0x0829, 0x00 },
                    { 0x082A, 0x00 },
                    { 0x082B, 0x00 },
                    { 0x082C, 0x00 },
                    { 0x082D, 0x00 },
                    { 0x082E, 0x00 },
                    { 0x082F, 0x00 },
                    { 0x0830, 0x00 },
                    { 0x0831, 0x00 },
                    { 0x0832, 0x00 },
                    { 0x0833, 0x00 },
                    { 0x0834, 0x00 },
                    { 0x0835, 0x00 },
                    { 0x0836, 0x00 },
                    { 0x0837, 0x00 },
                    { 0x0838, 0x00 },
                    { 0x0839, 0x00 },
                    { 0x083A, 0x00 },
                    { 0x083B, 0x00 },
                    { 0x083C, 0x00 },
                    { 0x083D, 0x00 },
                    { 0x083E, 0x00 },
                    { 0x083F, 0x00 },
                    { 0x0840, 0x00 },
                    { 0x0841, 0x00 },
                    { 0x0842, 0x00 },
                    { 0x0843, 0x00 },
                    { 0x0844, 0x00 },
                    { 0x0845, 0x00 },
                    { 0x0846, 0x00 },
                    { 0x0847, 0x00 },
                    { 0x0848, 0x00 },
                    { 0x0849, 0x00 },
                    { 0x084A, 0x00 },
                    { 0x084B, 0x00 },
                    { 0x084C, 0x00 },
                    { 0x084D, 0x00 },
                    { 0x084E, 0x00 },
                    { 0x084F, 0x00 },
                    { 0x0850, 0x00 },
                    { 0x0851, 0x00 },
                    { 0x0852, 0x00 },
                    { 0x0853, 0x00 },
                    { 0x0854, 0x00 },
                    { 0x0855, 0x00 },
                    { 0x0856, 0x00 },
                    { 0x0857, 0x00 },
                    { 0x0858, 0x00 },
                    { 0x0859, 0x00 },
                    { 0x085A, 0x00 },
                    { 0x085B, 0x00 },
                    { 0x085C, 0x00 },
                    { 0x085D, 0x00 },
                    { 0x085E, 0x00 },
                    { 0x085F, 0x00 },
                    { 0x0860, 0x00 },
                    { 0x0861, 0x00 },
                    { 0x090E, 0x02 },
                    { 0x091C, 0x04 },
                    { 0x0943, 0x00 },
                    { 0x0949, 0x00 },
                    { 0x094A, 0x00 },
                    { 0x094E, 0x49 },
                    { 0x094F, 0x02 },
                    { 0x095E, 0x00 },
                    { 0x0A02, 0x00 },
                    { 0x0A03, 0x01 },
                    { 0x0A04, 0x01 },
                    { 0x0A05, 0x01 },
                    { 0x0A14, 0x00 },
                    { 0x0A1A, 0x00 },
                    { 0x0A20, 0x00 },
                    { 0x0A26, 0x00 },
                    { 0x0B44, 0x0F },
                    { 0x0B4A, 0x0E },
                    { 0x0B57, 0x0E },
                    { 0x0B58, 0x01 },
                          /* End configuration registers */
              }
            },

            /*
            // "On Board Oscillator - 156.25M":
            {
              1,  // Profile Index
              {
                          // Start configuration registers
                              { 0x0006, 0x00 },
                              { 0x0007, 0x00 },
                              { 0x0008, 0x00 },
                              { 0x000B, 0x74 },
                              { 0x0017, 0xD0 },
                              { 0x0018, 0xFF },
                              { 0x0021, 0x0F },
                              { 0x0022, 0x00 },
                              { 0x002B, 0x02 },
                              { 0x002C, 0x20 },
                              { 0x002D, 0x00 },
                              { 0x002E, 0x00 },
                              { 0x002F, 0x00 },
                              { 0x0030, 0x00 },
                              { 0x0031, 0x00 },
                              { 0x0032, 0x00 },
                              { 0x0033, 0x00 },
                              { 0x0034, 0x00 },
                              { 0x0035, 0x00 },
                              { 0x0036, 0x00 },
                              { 0x0037, 0x00 },
                              { 0x0038, 0x00 },
                              { 0x0039, 0x00 },
                              { 0x003A, 0x00 },
                              { 0x003B, 0x00 },
                              { 0x003C, 0x00 },
                              { 0x003D, 0x00 },
                              { 0x0041, 0x00 },
                              { 0x0042, 0x00 },
                              { 0x0043, 0x00 },
                              { 0x0044, 0x00 },
                              { 0x009E, 0x00 },
                              { 0x0102, 0x01 },
                              { 0x0112, 0x06 },
                              { 0x0113, 0x09 },
                              { 0x0114, 0x3E },
                              { 0x0115, 0x18 },
                              { 0x0117, 0x06 },
                              { 0x0118, 0x09 },
                              { 0x0119, 0x3E },
                              { 0x011A, 0x18 },
                              { 0x0126, 0x06 },
                              { 0x0127, 0x09 },
                              { 0x0128, 0x3E },
                              { 0x0129, 0x18 },
                              { 0x012B, 0x01 },
                              { 0x012C, 0x09 },
                              { 0x012D, 0x3B },
                              { 0x012E, 0x28 },
                              { 0x013F, 0x00 },
                              { 0x0140, 0x00 },
                              { 0x0141, 0x40 },
                              { 0x0206, 0x00 },
                              { 0x0208, 0x00 },
                              { 0x0209, 0x00 },
                              { 0x020A, 0x00 },
                              { 0x020B, 0x00 },
                              { 0x020C, 0x00 },
                              { 0x020D, 0x00 },
                              { 0x020E, 0x00 },
                              { 0x020F, 0x00 },
                              { 0x0210, 0x00 },
                              { 0x0211, 0x00 },
                              { 0x0212, 0x00 },
                              { 0x0213, 0x00 },
                              { 0x0214, 0x00 },
                              { 0x0215, 0x00 },
                              { 0x0216, 0x00 },
                              { 0x0217, 0x00 },
                              { 0x0218, 0x00 },
                              { 0x0219, 0x00 },
                              { 0x021A, 0x00 },
                              { 0x021B, 0x00 },
                              { 0x021C, 0x00 },
                              { 0x021D, 0x00 },
                              { 0x021E, 0x00 },
                              { 0x021F, 0x00 },
                              { 0x0220, 0x00 },
                              { 0x0221, 0x00 },
                              { 0x0222, 0x00 },
                              { 0x0223, 0x00 },
                              { 0x0224, 0x00 },
                              { 0x0225, 0x00 },
                              { 0x0226, 0x00 },
                              { 0x0227, 0x00 },
                              { 0x0228, 0x00 },
                              { 0x0229, 0x00 },
                              { 0x022A, 0x00 },
                              { 0x022B, 0x00 },
                              { 0x022C, 0x00 },
                              { 0x022D, 0x00 },
                              { 0x022E, 0x00 },
                              { 0x022F, 0x00 },
                              { 0x0235, 0x00 },
                              { 0x0236, 0x00 },
                              { 0x0237, 0x00 },
                              { 0x0238, 0xD8 },
                              { 0x0239, 0xD6 },
                              { 0x023A, 0x00 },
                              { 0x023B, 0x00 },
                              { 0x023C, 0x00 },
                              { 0x023D, 0x00 },
                              { 0x023E, 0xC0 },
                              { 0x0250, 0x00 },
                              { 0x0251, 0x00 },
                              { 0x0252, 0x00 },
                              { 0x0253, 0x00 },
                              { 0x0254, 0x00 },
                              { 0x0255, 0x00 },
                              { 0x025C, 0x00 },
                              { 0x025D, 0x00 },
                              { 0x025E, 0x00 },
                              { 0x025F, 0x00 },
                              { 0x0260, 0x00 },
                              { 0x0261, 0x00 },
                              { 0x026B, 0x35 },
                              { 0x026C, 0x33 },
                              { 0x026D, 0x34 },
                              { 0x026E, 0x30 },
                              { 0x026F, 0x45 },
                              { 0x0270, 0x56 },
                              { 0x0271, 0x42 },
                              { 0x0272, 0x31 },
                              { 0x0302, 0x00 },
                              { 0x0303, 0x00 },
                              { 0x0304, 0x00 },
                              { 0x0305, 0x00 },
                              { 0x0306, 0x16 },
                              { 0x0307, 0x00 },
                              { 0x0308, 0x00 },
                              { 0x0309, 0x00 },
                              { 0x030A, 0x00 },
                              { 0x030B, 0x80 },
                              { 0x030C, 0x00 },
                              { 0x030D, 0x00 },
                              { 0x030E, 0x00 },
                              { 0x030F, 0x00 },
                              { 0x0310, 0x00 },
                              { 0x0311, 0x00 },
                              { 0x0312, 0x00 },
                              { 0x0313, 0x00 },
                              { 0x0314, 0x00 },
                              { 0x0315, 0x00 },
                              { 0x0316, 0x00 },
                              { 0x0317, 0x00 },
                              { 0x0318, 0x00 },
                              { 0x0319, 0x00 },
                              { 0x031A, 0x00 },
                              { 0x031B, 0x00 },
                              { 0x031C, 0x00 },
                              { 0x031D, 0x00 },
                              { 0x031E, 0x00 },
                              { 0x031F, 0x00 },
                              { 0x0320, 0x00 },
                              { 0x0321, 0x00 },
                              { 0x0322, 0x00 },
                              { 0x0323, 0x00 },
                              { 0x0324, 0x00 },
                              { 0x0325, 0x00 },
                              { 0x0326, 0x00 },
                              { 0x0327, 0x00 },
                              { 0x0328, 0x00 },
                              { 0x0329, 0x00 },
                              { 0x032A, 0x00 },
                              { 0x032B, 0x00 },
                              { 0x032C, 0x00 },
                              { 0x032D, 0x00 },
                              { 0x0338, 0x00 },
                              { 0x0339, 0x1F },
                              { 0x033B, 0x00 },
                              { 0x033C, 0x00 },
                              { 0x033D, 0x00 },
                              { 0x033E, 0x00 },
                              { 0x033F, 0x00 },
                              { 0x0340, 0x00 },
                              { 0x0341, 0x00 },
                              { 0x0342, 0x00 },
                              { 0x0343, 0x00 },
                              { 0x0344, 0x00 },
                              { 0x0345, 0x00 },
                              { 0x0346, 0x00 },
                              { 0x0347, 0x00 },
                              { 0x0348, 0x00 },
                              { 0x0349, 0x00 },
                              { 0x034A, 0x00 },
                              { 0x034B, 0x00 },
                              { 0x034C, 0x00 },
                              { 0x034D, 0x00 },
                              { 0x034E, 0x00 },
                              { 0x034F, 0x00 },
                              { 0x0350, 0x00 },
                              { 0x0351, 0x00 },
                              { 0x0352, 0x00 },
                              { 0x0359, 0x00 },
                              { 0x035A, 0x00 },
                              { 0x035B, 0x00 },
                              { 0x035C, 0x00 },
                              { 0x035D, 0x00 },
                              { 0x035E, 0x00 },
                              { 0x035F, 0x00 },
                              { 0x0360, 0x00 },
                              { 0x0802, 0x00 },
                              { 0x0803, 0x00 },
                              { 0x0804, 0x00 },
                              { 0x0805, 0x00 },
                              { 0x0806, 0x00 },
                              { 0x0807, 0x00 },
                              { 0x0808, 0x00 },
                              { 0x0809, 0x00 },
                              { 0x080A, 0x00 },
                              { 0x080B, 0x00 },
                              { 0x080C, 0x00 },
                              { 0x080D, 0x00 },
                              { 0x080E, 0x00 },
                              { 0x080F, 0x00 },
                              { 0x0810, 0x00 },
                              { 0x0811, 0x00 },
                              { 0x0812, 0x00 },
                              { 0x0813, 0x00 },
                              { 0x0814, 0x00 },
                              { 0x0815, 0x00 },
                              { 0x0816, 0x00 },
                              { 0x0817, 0x00 },
                              { 0x0818, 0x00 },
                              { 0x0819, 0x00 },
                              { 0x081A, 0x00 },
                              { 0x081B, 0x00 },
                              { 0x081C, 0x00 },
                              { 0x081D, 0x00 },
                              { 0x081E, 0x00 },
                              { 0x081F, 0x00 },
                              { 0x0820, 0x00 },
                              { 0x0821, 0x00 },
                              { 0x0822, 0x00 },
                              { 0x0823, 0x00 },
                              { 0x0824, 0x00 },
                              { 0x0825, 0x00 },
                              { 0x0826, 0x00 },
                              { 0x0827, 0x00 },
                              { 0x0828, 0x00 },
                              { 0x0829, 0x00 },
                              { 0x082A, 0x00 },
                              { 0x082B, 0x00 },
                              { 0x082C, 0x00 },
                              { 0x082D, 0x00 },
                              { 0x082E, 0x00 },
                              { 0x082F, 0x00 },
                              { 0x0830, 0x00 },
                              { 0x0831, 0x00 },
                              { 0x0832, 0x00 },
                              { 0x0833, 0x00 },
                              { 0x0834, 0x00 },
                              { 0x0835, 0x00 },
                              { 0x0836, 0x00 },
                              { 0x0837, 0x00 },
                              { 0x0838, 0x00 },
                              { 0x0839, 0x00 },
                              { 0x083A, 0x00 },
                              { 0x083B, 0x00 },
                              { 0x083C, 0x00 },
                              { 0x083D, 0x00 },
                              { 0x083E, 0x00 },
                              { 0x083F, 0x00 },
                              { 0x0840, 0x00 },
                              { 0x0841, 0x00 },
                              { 0x0842, 0x00 },
                              { 0x0843, 0x00 },
                              { 0x0844, 0x00 },
                              { 0x0845, 0x00 },
                              { 0x0846, 0x00 },
                              { 0x0847, 0x00 },
                              { 0x0848, 0x00 },
                              { 0x0849, 0x00 },
                              { 0x084A, 0x00 },
                              { 0x084B, 0x00 },
                              { 0x084C, 0x00 },
                              { 0x084D, 0x00 },
                              { 0x084E, 0x00 },
                              { 0x084F, 0x00 },
                              { 0x0850, 0x00 },
                              { 0x0851, 0x00 },
                              { 0x0852, 0x00 },
                              { 0x0853, 0x00 },
                              { 0x0854, 0x00 },
                              { 0x0855, 0x00 },
                              { 0x0856, 0x00 },
                              { 0x0857, 0x00 },
                              { 0x0858, 0x00 },
                              { 0x0859, 0x00 },
                              { 0x085A, 0x00 },
                              { 0x085B, 0x00 },
                              { 0x085C, 0x00 },
                              { 0x085D, 0x00 },
                              { 0x085E, 0x00 },
                              { 0x085F, 0x00 },
                              { 0x0860, 0x00 },
                              { 0x0861, 0x00 },
                              { 0x090E, 0x02 },
                              { 0x091C, 0x04 },
                              { 0x0943, 0x00 },
                              { 0x0949, 0x00 },
                              { 0x094A, 0x00 },
                              { 0x094E, 0x49 },
                              { 0x094F, 0x02 },
                              { 0x095E, 0x00 },
                              { 0x0A02, 0x00 },
                              { 0x0A03, 0x01 },
                              { 0x0A04, 0x01 },
                              { 0x0A05, 0x01 },
                              { 0x0A14, 0x00 },
                              { 0x0A1A, 0x00 },
                              { 0x0A20, 0x00 },
                              { 0x0A26, 0x00 },
                              { 0x0B44, 0x0F },
                              { 0x0B4A, 0x0E },
                              { 0x0B57, 0x0E },
                              { 0x0B58, 0x01 },
                          // End configuration registers
              }
            },

            // "On Board Oscillator - 161.13281M":
            {
              2,  // Profile Index
              {
                          // Start configuration registers
                    { 0x0006, 0x00 },
                        { 0x0007, 0x00 },
                        { 0x0008, 0x00 },
                        { 0x000B, 0x74 },
                        { 0x0017, 0xD0 },
                        { 0x0018, 0xFF },
                        { 0x0021, 0x0F },
                        { 0x0022, 0x00 },
                        { 0x002B, 0x02 },
                        { 0x002C, 0x20 },
                        { 0x002D, 0x00 },
                        { 0x002E, 0x00 },
                        { 0x002F, 0x00 },
                        { 0x0030, 0x00 },
                        { 0x0031, 0x00 },
                        { 0x0032, 0x00 },
                        { 0x0033, 0x00 },
                        { 0x0034, 0x00 },
                        { 0x0035, 0x00 },
                        { 0x0036, 0x00 },
                        { 0x0037, 0x00 },
                        { 0x0038, 0x00 },
                        { 0x0039, 0x00 },
                        { 0x003A, 0x00 },
                        { 0x003B, 0x00 },
                        { 0x003C, 0x00 },
                        { 0x003D, 0x00 },
                        { 0x0041, 0x00 },
                        { 0x0042, 0x00 },
                        { 0x0043, 0x00 },
                        { 0x0044, 0x00 },
                        { 0x009E, 0x00 },
                        { 0x0102, 0x01 },
                        { 0x0112, 0x06 },
                        { 0x0113, 0x09 },
                        { 0x0114, 0x6B },
                        { 0x0115, 0x28 },
                        { 0x0117, 0x06 },
                        { 0x0118, 0x09 },
                        { 0x0119, 0x6B },
                        { 0x011A, 0x28 },
                        { 0x0126, 0x06 },
                        { 0x0127, 0x09 },
                        { 0x0128, 0x6B },
                        { 0x0129, 0x28 },
                        { 0x012B, 0x01 },
                        { 0x012C, 0x09 },
                        { 0x012D, 0x3B },
                        { 0x012E, 0x28 },
                        { 0x013F, 0x00 },
                        { 0x0140, 0x00 },
                        { 0x0141, 0x40 },
                        { 0x0206, 0x00 },
                        { 0x0208, 0x00 },
                        { 0x0209, 0x00 },
                        { 0x020A, 0x00 },
                        { 0x020B, 0x00 },
                        { 0x020C, 0x00 },
                        { 0x020D, 0x00 },
                        { 0x020E, 0x00 },
                        { 0x020F, 0x00 },
                        { 0x0210, 0x00 },
                        { 0x0211, 0x00 },
                        { 0x0212, 0x00 },
                        { 0x0213, 0x00 },
                        { 0x0214, 0x00 },
                        { 0x0215, 0x00 },
                        { 0x0216, 0x00 },
                        { 0x0217, 0x00 },
                        { 0x0218, 0x00 },
                        { 0x0219, 0x00 },
                        { 0x021A, 0x00 },
                        { 0x021B, 0x00 },
                        { 0x021C, 0x00 },
                        { 0x021D, 0x00 },
                        { 0x021E, 0x00 },
                        { 0x021F, 0x00 },
                        { 0x0220, 0x00 },
                        { 0x0221, 0x00 },
                        { 0x0222, 0x00 },
                        { 0x0223, 0x00 },
                        { 0x0224, 0x00 },
                        { 0x0225, 0x00 },
                        { 0x0226, 0x00 },
                        { 0x0227, 0x00 },
                        { 0x0228, 0x00 },
                        { 0x0229, 0x00 },
                        { 0x022A, 0x00 },
                        { 0x022B, 0x00 },
                        { 0x022C, 0x00 },
                        { 0x022D, 0x00 },
                        { 0x022E, 0x00 },
                        { 0x022F, 0x00 },
                        { 0x0235, 0x00 },
                        { 0x0236, 0x00 },
                        { 0x0237, 0xAB },
                        { 0x0238, 0xC8 },
                        { 0x0239, 0x0E },
                        { 0x023A, 0x01 },
                        { 0x023B, 0x00 },
                        { 0x023C, 0x00 },
                        { 0x023D, 0x60 },
                        { 0x023E, 0xEA },
                        { 0x0250, 0x00 },
                        { 0x0251, 0x00 },
                        { 0x0252, 0x00 },
                        { 0x0253, 0x00 },
                        { 0x0254, 0x00 },
                        { 0x0255, 0x00 },
                        { 0x025C, 0x00 },
                        { 0x025D, 0x00 },
                        { 0x025E, 0x00 },
                        { 0x025F, 0x00 },
                        { 0x0260, 0x00 },
                        { 0x0261, 0x00 },
                        { 0x026B, 0x35 },
                        { 0x026C, 0x33 },
                        { 0x026D, 0x34 },
                        { 0x026E, 0x30 },
                        { 0x026F, 0x45 },
                        { 0x0270, 0x56 },
                        { 0x0271, 0x42 },
                        { 0x0272, 0x31 },
                        { 0x0302, 0x00 },
                        { 0x0303, 0x00 },
                        { 0x0304, 0x00 },
                        { 0x0305, 0x00 },
                        { 0x0306, 0x16 },
                        { 0x0307, 0x00 },
                        { 0x0308, 0x00 },
                        { 0x0309, 0x00 },
                        { 0x030A, 0x00 },
                        { 0x030B, 0x80 },
                        { 0x030C, 0x00 },
                        { 0x030D, 0x00 },
                        { 0x030E, 0x00 },
                        { 0x030F, 0x00 },
                        { 0x0310, 0x00 },
                        { 0x0311, 0x00 },
                        { 0x0312, 0x00 },
                        { 0x0313, 0x00 },
                        { 0x0314, 0x00 },
                        { 0x0315, 0x00 },
                        { 0x0316, 0x00 },
                        { 0x0317, 0x00 },
                        { 0x0318, 0x00 },
                        { 0x0319, 0x00 },
                        { 0x031A, 0x00 },
                        { 0x031B, 0x00 },
                        { 0x031C, 0x00 },
                        { 0x031D, 0x00 },
                        { 0x031E, 0x00 },
                        { 0x031F, 0x00 },
                        { 0x0320, 0x00 },
                        { 0x0321, 0x00 },
                        { 0x0322, 0x00 },
                        { 0x0323, 0x00 },
                        { 0x0324, 0x00 },
                        { 0x0325, 0x00 },
                        { 0x0326, 0x00 },
                        { 0x0327, 0x00 },
                        { 0x0328, 0x00 },
                        { 0x0329, 0x00 },
                        { 0x032A, 0x00 },
                        { 0x032B, 0x00 },
                        { 0x032C, 0x00 },
                        { 0x032D, 0x00 },
                        { 0x0338, 0x00 },
                        { 0x0339, 0x1F },
                        { 0x033B, 0x00 },
                        { 0x033C, 0x00 },
                        { 0x033D, 0x00 },
                        { 0x033E, 0x00 },
                        { 0x033F, 0x00 },
                        { 0x0340, 0x00 },
                        { 0x0341, 0x00 },
                        { 0x0342, 0x00 },
                        { 0x0343, 0x00 },
                        { 0x0344, 0x00 },
                        { 0x0345, 0x00 },
                        { 0x0346, 0x00 },
                        { 0x0347, 0x00 },
                        { 0x0348, 0x00 },
                        { 0x0349, 0x00 },
                        { 0x034A, 0x00 },
                        { 0x034B, 0x00 },
                        { 0x034C, 0x00 },
                        { 0x034D, 0x00 },
                        { 0x034E, 0x00 },
                        { 0x034F, 0x00 },
                        { 0x0350, 0x00 },
                        { 0x0351, 0x00 },
                        { 0x0352, 0x00 },
                        { 0x0359, 0x00 },
                        { 0x035A, 0x00 },
                        { 0x035B, 0x00 },
                        { 0x035C, 0x00 },
                        { 0x035D, 0x00 },
                        { 0x035E, 0x00 },
                        { 0x035F, 0x00 },
                        { 0x0360, 0x00 },
                        { 0x0802, 0x00 },
                        { 0x0803, 0x00 },
                        { 0x0804, 0x00 },
                        { 0x0805, 0x00 },
                        { 0x0806, 0x00 },
                        { 0x0807, 0x00 },
                        { 0x0808, 0x00 },
                        { 0x0809, 0x00 },
                        { 0x080A, 0x00 },
                        { 0x080B, 0x00 },
                        { 0x080C, 0x00 },
                        { 0x080D, 0x00 },
                        { 0x080E, 0x00 },
                        { 0x080F, 0x00 },
                        { 0x0810, 0x00 },
                        { 0x0811, 0x00 },
                        { 0x0812, 0x00 },
                        { 0x0813, 0x00 },
                        { 0x0814, 0x00 },
                        { 0x0815, 0x00 },
                        { 0x0816, 0x00 },
                        { 0x0817, 0x00 },
                        { 0x0818, 0x00 },
                        { 0x0819, 0x00 },
                        { 0x081A, 0x00 },
                        { 0x081B, 0x00 },
                        { 0x081C, 0x00 },
                        { 0x081D, 0x00 },
                        { 0x081E, 0x00 },
                        { 0x081F, 0x00 },
                        { 0x0820, 0x00 },
                        { 0x0821, 0x00 },
                        { 0x0822, 0x00 },
                        { 0x0823, 0x00 },
                        { 0x0824, 0x00 },
                        { 0x0825, 0x00 },
                        { 0x0826, 0x00 },
                        { 0x0827, 0x00 },
                        { 0x0828, 0x00 },
                        { 0x0829, 0x00 },
                        { 0x082A, 0x00 },
                        { 0x082B, 0x00 },
                        { 0x082C, 0x00 },
                        { 0x082D, 0x00 },
                        { 0x082E, 0x00 },
                        { 0x082F, 0x00 },
                        { 0x0830, 0x00 },
                        { 0x0831, 0x00 },
                        { 0x0832, 0x00 },
                        { 0x0833, 0x00 },
                        { 0x0834, 0x00 },
                        { 0x0835, 0x00 },
                        { 0x0836, 0x00 },
                        { 0x0837, 0x00 },
                        { 0x0838, 0x00 },
                        { 0x0839, 0x00 },
                        { 0x083A, 0x00 },
                        { 0x083B, 0x00 },
                        { 0x083C, 0x00 },
                        { 0x083D, 0x00 },
                        { 0x083E, 0x00 },
                        { 0x083F, 0x00 },
                        { 0x0840, 0x00 },
                        { 0x0841, 0x00 },
                        { 0x0842, 0x00 },
                        { 0x0843, 0x00 },
                        { 0x0844, 0x00 },
                        { 0x0845, 0x00 },
                        { 0x0846, 0x00 },
                        { 0x0847, 0x00 },
                        { 0x0848, 0x00 },
                        { 0x0849, 0x00 },
                        { 0x084A, 0x00 },
                        { 0x084B, 0x00 },
                        { 0x084C, 0x00 },
                        { 0x084D, 0x00 },
                        { 0x084E, 0x00 },
                        { 0x084F, 0x00 },
                        { 0x0850, 0x00 },
                        { 0x0851, 0x00 },
                        { 0x0852, 0x00 },
                        { 0x0853, 0x00 },
                        { 0x0854, 0x00 },
                        { 0x0855, 0x00 },
                        { 0x0856, 0x00 },
                        { 0x0857, 0x00 },
                        { 0x0858, 0x00 },
                        { 0x0859, 0x00 },
                        { 0x085A, 0x00 },
                        { 0x085B, 0x00 },
                        { 0x085C, 0x00 },
                        { 0x085D, 0x00 },
                        { 0x085E, 0x00 },
                        { 0x085F, 0x00 },
                        { 0x0860, 0x00 },
                        { 0x0861, 0x00 },
                        { 0x090E, 0x02 },
                        { 0x091C, 0x04 },
                        { 0x0943, 0x00 },
                        { 0x0949, 0x00 },
                        { 0x094A, 0x00 },
                        { 0x094E, 0x49 },
                        { 0x094F, 0x02 },
                        { 0x095E, 0x00 },
                        { 0x0A02, 0x00 },
                        { 0x0A03, 0x01 },
                        { 0x0A04, 0x01 },
                        { 0x0A05, 0x01 },
                        { 0x0A14, 0x00 },
                        { 0x0A1A, 0x00 },
                        { 0x0A20, 0x00 },
                        { 0x0A26, 0x00 },
                        { 0x0B44, 0x0F },
                        { 0x0B4A, 0x0E },
                        { 0x0B57, 0x0E },
                        { 0x0B58, 0x01 },
                         // End configuration registers
              }
            },
        */
      // "EXT. 10 MHz":
      {
        1,  // Profile Index
        {
                    /* Start configuration registers */
                    { 0x0006, 0x00 },
                    { 0x0007, 0x00 },
                    { 0x0008, 0x00 },
                    { 0x000B, 0x74 },
                    { 0x0017, 0xD0 },
                    { 0x0018, 0xFF },
                    { 0x0021, 0x09 },
                    { 0x0022, 0x00 },
                    { 0x002B, 0x02 },
                    { 0x002C, 0x31 },
                    { 0x002D, 0x01 },
                    { 0x002E, 0x55 },
                    { 0x002F, 0x00 },
                    { 0x0030, 0x00 },
                    { 0x0031, 0x00 },
                    { 0x0032, 0x00 },
                    { 0x0033, 0x00 },
                    { 0x0034, 0x00 },
                    { 0x0035, 0x00 },
                    { 0x0036, 0x55 },
                    { 0x0037, 0x00 },
                    { 0x0038, 0x00 },
                    { 0x0039, 0x00 },
                    { 0x003A, 0x00 },
                    { 0x003B, 0x00 },
                    { 0x003C, 0x00 },
                    { 0x003D, 0x00 },
                    { 0x0041, 0x03 },
                    { 0x0042, 0x00 },
                    { 0x0043, 0x00 },
                    { 0x0044, 0x00 },
                    { 0x009E, 0x00 },
                    { 0x0102, 0x01 },
                    { 0x0112, 0x01 },
                    { 0x0113, 0x09 },
                    { 0x0114, 0x3B },
                    { 0x0115, 0x28 },
                    { 0x0117, 0x06 },
                    { 0x0118, 0x09 },
                    { 0x0119, 0x3E },
                    { 0x011A, 0x18 },
                    { 0x0126, 0x06 },
                    { 0x0127, 0x09 },
                    { 0x0128, 0x3E },
                    { 0x0129, 0x18 },
                    { 0x012B, 0x01 },
                    { 0x012C, 0x09 },
                    { 0x012D, 0x3B },
                    { 0x012E, 0x28 },
                    { 0x013F, 0x00 },
                    { 0x0140, 0x00 },
                    { 0x0141, 0x40 },
                    { 0x0206, 0x00 },
                    { 0x0208, 0x01 },
                    { 0x0209, 0x00 },
                    { 0x020A, 0x00 },
                    { 0x020B, 0x00 },
                    { 0x020C, 0x00 },
                    { 0x020D, 0x00 },
                    { 0x020E, 0x01 },
                    { 0x020F, 0x00 },
                    { 0x0210, 0x00 },
                    { 0x0211, 0x00 },
                    { 0x0212, 0x00 },
                    { 0x0213, 0x00 },
                    { 0x0214, 0x00 },
                    { 0x0215, 0x00 },
                    { 0x0216, 0x00 },
                    { 0x0217, 0x00 },
                    { 0x0218, 0x00 },
                    { 0x0219, 0x00 },
                    { 0x021A, 0x00 },
                    { 0x021B, 0x00 },
                    { 0x021C, 0x00 },
                    { 0x021D, 0x00 },
                    { 0x021E, 0x00 },
                    { 0x021F, 0x00 },
                    { 0x0220, 0x00 },
                    { 0x0221, 0x00 },
                    { 0x0222, 0x00 },
                    { 0x0223, 0x00 },
                    { 0x0224, 0x00 },
                    { 0x0225, 0x00 },
                    { 0x0226, 0x00 },
                    { 0x0227, 0x00 },
                    { 0x0228, 0x00 },
                    { 0x0229, 0x00 },
                    { 0x022A, 0x00 },
                    { 0x022B, 0x00 },
                    { 0x022C, 0x00 },
                    { 0x022D, 0x00 },
                    { 0x022E, 0x00 },
                    { 0x022F, 0x00 },
                    { 0x0235, 0x00 },
                    { 0x0236, 0x00 },
                    { 0x0237, 0x00 },
                    { 0x0238, 0x00 },
                    { 0x0239, 0x94 },
                    { 0x023A, 0x02 },
                    { 0x023B, 0x00 },
                    { 0x023C, 0x00 },
                    { 0x023D, 0x00 },
                    { 0x023E, 0x80 },
                    { 0x0250, 0x00 },
                    { 0x0251, 0x00 },
                    { 0x0252, 0x00 },
                    { 0x0253, 0x00 },
                    { 0x0254, 0x00 },
                    { 0x0255, 0x00 },
                    { 0x025C, 0x00 },
                    { 0x025D, 0x00 },
                    { 0x025E, 0x00 },
                    { 0x025F, 0x00 },
                    { 0x0260, 0x00 },
                    { 0x0261, 0x00 },
                    { 0x026B, 0x35 },
                    { 0x026C, 0x33 },
                    { 0x026D, 0x34 },
                    { 0x026E, 0x30 },
                    { 0x026F, 0x45 },
                    { 0x0270, 0x56 },
                    { 0x0271, 0x42 },
                    { 0x0272, 0x31 },
                    { 0x0302, 0x00 },
                    { 0x0303, 0x00 },
                    { 0x0304, 0x00 },
                    { 0x0305, 0x00 },
                    { 0x0306, 0x21 },
                    { 0x0307, 0x00 },
                    { 0x0308, 0x00 },
                    { 0x0309, 0x00 },
                    { 0x030A, 0x00 },
                    { 0x030B, 0x80 },
                    { 0x030C, 0x00 },
                    { 0x030D, 0x00 },
                    { 0x030E, 0x00 },
                    { 0x030F, 0x00 },
                    { 0x0310, 0x00 },
                    { 0x0311, 0x00 },
                    { 0x0312, 0x00 },
                    { 0x0313, 0x00 },
                    { 0x0314, 0x00 },
                    { 0x0315, 0x00 },
                    { 0x0316, 0x00 },
                    { 0x0317, 0x00 },
                    { 0x0318, 0x00 },
                    { 0x0319, 0x00 },
                    { 0x031A, 0x00 },
                    { 0x031B, 0x00 },
                    { 0x031C, 0x00 },
                    { 0x031D, 0x00 },
                    { 0x031E, 0x00 },
                    { 0x031F, 0x00 },
                    { 0x0320, 0x00 },
                    { 0x0321, 0x00 },
                    { 0x0322, 0x00 },
                    { 0x0323, 0x00 },
                    { 0x0324, 0x00 },
                    { 0x0325, 0x00 },
                    { 0x0326, 0x00 },
                    { 0x0327, 0x00 },
                    { 0x0328, 0x00 },
                    { 0x0329, 0x00 },
                    { 0x032A, 0x00 },
                    { 0x032B, 0x00 },
                    { 0x032C, 0x00 },
                    { 0x032D, 0x00 },
                    { 0x0338, 0x00 },
                    { 0x0339, 0x1F },
                    { 0x033B, 0x00 },
                    { 0x033C, 0x00 },
                    { 0x033D, 0x00 },
                    { 0x033E, 0x00 },
                    { 0x033F, 0x00 },
                    { 0x0340, 0x00 },
                    { 0x0341, 0x00 },
                    { 0x0342, 0x00 },
                    { 0x0343, 0x00 },
                    { 0x0344, 0x00 },
                    { 0x0345, 0x00 },
                    { 0x0346, 0x00 },
                    { 0x0347, 0x00 },
                    { 0x0348, 0x00 },
                    { 0x0349, 0x00 },
                    { 0x034A, 0x00 },
                    { 0x034B, 0x00 },
                    { 0x034C, 0x00 },
                    { 0x034D, 0x00 },
                    { 0x034E, 0x00 },
                    { 0x034F, 0x00 },
                    { 0x0350, 0x00 },
                    { 0x0351, 0x00 },
                    { 0x0352, 0x00 },
                    { 0x0359, 0x00 },
                    { 0x035A, 0x00 },
                    { 0x035B, 0x00 },
                    { 0x035C, 0x00 },
                    { 0x035D, 0x00 },
                    { 0x035E, 0x00 },
                    { 0x035F, 0x00 },
                    { 0x0360, 0x00 },
                    { 0x0802, 0x00 },
                    { 0x0803, 0x00 },
                    { 0x0804, 0x00 },
                    { 0x0805, 0x00 },
                    { 0x0806, 0x00 },
                    { 0x0807, 0x00 },
                    { 0x0808, 0x00 },
                    { 0x0809, 0x00 },
                    { 0x080A, 0x00 },
                    { 0x080B, 0x00 },
                    { 0x080C, 0x00 },
                    { 0x080D, 0x00 },
                    { 0x080E, 0x00 },
                    { 0x080F, 0x00 },
                    { 0x0810, 0x00 },
                    { 0x0811, 0x00 },
                    { 0x0812, 0x00 },
                    { 0x0813, 0x00 },
                    { 0x0814, 0x00 },
                    { 0x0815, 0x00 },
                    { 0x0816, 0x00 },
                    { 0x0817, 0x00 },
                    { 0x0818, 0x00 },
                    { 0x0819, 0x00 },
                    { 0x081A, 0x00 },
                    { 0x081B, 0x00 },
                    { 0x081C, 0x00 },
                    { 0x081D, 0x00 },
                    { 0x081E, 0x00 },
                    { 0x081F, 0x00 },
                    { 0x0820, 0x00 },
                    { 0x0821, 0x00 },
                    { 0x0822, 0x00 },
                    { 0x0823, 0x00 },
                    { 0x0824, 0x00 },
                    { 0x0825, 0x00 },
                    { 0x0826, 0x00 },
                    { 0x0827, 0x00 },
                    { 0x0828, 0x00 },
                    { 0x0829, 0x00 },
                    { 0x082A, 0x00 },
                    { 0x082B, 0x00 },
                    { 0x082C, 0x00 },
                    { 0x082D, 0x00 },
                    { 0x082E, 0x00 },
                    { 0x082F, 0x00 },
                    { 0x0830, 0x00 },
                    { 0x0831, 0x00 },
                    { 0x0832, 0x00 },
                    { 0x0833, 0x00 },
                    { 0x0834, 0x00 },
                    { 0x0835, 0x00 },
                    { 0x0836, 0x00 },
                    { 0x0837, 0x00 },
                    { 0x0838, 0x00 },
                    { 0x0839, 0x00 },
                    { 0x083A, 0x00 },
                    { 0x083B, 0x00 },
                    { 0x083C, 0x00 },
                    { 0x083D, 0x00 },
                    { 0x083E, 0x00 },
                    { 0x083F, 0x00 },
                    { 0x0840, 0x00 },
                    { 0x0841, 0x00 },
                    { 0x0842, 0x00 },
                    { 0x0843, 0x00 },
                    { 0x0844, 0x00 },
                    { 0x0845, 0x00 },
                    { 0x0846, 0x00 },
                    { 0x0847, 0x00 },
                    { 0x0848, 0x00 },
                    { 0x0849, 0x00 },
                    { 0x084A, 0x00 },
                    { 0x084B, 0x00 },
                    { 0x084C, 0x00 },
                    { 0x084D, 0x00 },
                    { 0x084E, 0x00 },
                    { 0x084F, 0x00 },
                    { 0x0850, 0x00 },
                    { 0x0851, 0x00 },
                    { 0x0852, 0x00 },
                    { 0x0853, 0x00 },
                    { 0x0854, 0x00 },
                    { 0x0855, 0x00 },
                    { 0x0856, 0x00 },
                    { 0x0857, 0x00 },
                    { 0x0858, 0x00 },
                    { 0x0859, 0x00 },
                    { 0x085A, 0x00 },
                    { 0x085B, 0x00 },
                    { 0x085C, 0x00 },
                    { 0x085D, 0x00 },
                    { 0x085E, 0x00 },
                    { 0x085F, 0x00 },
                    { 0x0860, 0x00 },
                    { 0x0861, 0x00 },
                    { 0x090E, 0x00 },
                    { 0x091C, 0x04 },
                    { 0x0943, 0x00 },
                    { 0x0949, 0x01 },
                    { 0x094A, 0x10 },
                    { 0x094E, 0x49 },
                    { 0x094F, 0x02 },
                    { 0x095E, 0x00 },
                    { 0x0A02, 0x00 },
                    { 0x0A03, 0x01 },
                    { 0x0A04, 0x01 },
                    { 0x0A05, 0x01 },
                    { 0x0A14, 0x00 },
                    { 0x0A1A, 0x00 },
                    { 0x0A20, 0x00 },
                    { 0x0A26, 0x00 },
                    { 0x0B44, 0x0F },
                    { 0x0B4A, 0x0E },
                    { 0x0B57, 0x10 },
                    { 0x0B58, 0x05 },
                    /* End configuration registers */
        }
      },

      // "EXT. 100 MHz":
      {
        2,  // Profile Index
        {
                    /* Start configuration registers */
                    { 0x0006, 0x00 },
                    { 0x0007, 0x00 },
                    { 0x0008, 0x00 },
                    { 0x000B, 0x74 },
                    { 0x0017, 0xD0 },
                    { 0x0018, 0xFF },
                    { 0x0021, 0x0B },
                    { 0x0022, 0x00 },
                    { 0x002B, 0x02 },
                    { 0x002C, 0x32 },
                    { 0x002D, 0x04 },
                    { 0x002E, 0x00 },
                    { 0x002F, 0x00 },
                    { 0x0030, 0x44 },
                    { 0x0031, 0x00 },
                    { 0x0032, 0x00 },
                    { 0x0033, 0x00 },
                    { 0x0034, 0x00 },
                    { 0x0035, 0x00 },
                    { 0x0036, 0x00 },
                    { 0x0037, 0x00 },
                    { 0x0038, 0x44 },
                    { 0x0039, 0x00 },
                    { 0x003A, 0x00 },
                    { 0x003B, 0x00 },
                    { 0x003C, 0x00 },
                    { 0x003D, 0x00 },
                    { 0x0041, 0x00 },
                    { 0x0042, 0x06 },
                    { 0x0043, 0x00 },
                    { 0x0044, 0x00 },
                    { 0x009E, 0x00 },
                    { 0x0102, 0x01 },
                    { 0x0112, 0x01 },
                    { 0x0113, 0x09 },
                    { 0x0114, 0x3B },
                    { 0x0115, 0x28 },
                    { 0x0117, 0x06 },
                    { 0x0118, 0x09 },
                    { 0x0119, 0x3E },
                    { 0x011A, 0x18 },
                    { 0x0126, 0x06 },
                    { 0x0127, 0x09 },
                    { 0x0128, 0x3E },
                    { 0x0129, 0x18 },
                    { 0x012B, 0x01 },
                    { 0x012C, 0x09 },
                    { 0x012D, 0x3B },
                    { 0x012E, 0x28 },
                    { 0x013F, 0x00 },
                    { 0x0140, 0x00 },
                    { 0x0141, 0x40 },
                    { 0x0206, 0x00 },
                    { 0x0208, 0x00 },
                    { 0x0209, 0x00 },
                    { 0x020A, 0x00 },
                    { 0x020B, 0x00 },
                    { 0x020C, 0x00 },
                    { 0x020D, 0x00 },
                    { 0x020E, 0x00 },
                    { 0x020F, 0x00 },
                    { 0x0210, 0x00 },
                    { 0x0211, 0x00 },
                    { 0x0212, 0x01 },
                    { 0x0213, 0x00 },
                    { 0x0214, 0x00 },
                    { 0x0215, 0x00 },
                    { 0x0216, 0x00 },
                    { 0x0217, 0x00 },
                    { 0x0218, 0x01 },
                    { 0x0219, 0x00 },
                    { 0x021A, 0x00 },
                    { 0x021B, 0x00 },
                    { 0x021C, 0x00 },
                    { 0x021D, 0x00 },
                    { 0x021E, 0x00 },
                    { 0x021F, 0x00 },
                    { 0x0220, 0x00 },
                    { 0x0221, 0x00 },
                    { 0x0222, 0x00 },
                    { 0x0223, 0x00 },
                    { 0x0224, 0x00 },
                    { 0x0225, 0x00 },
                    { 0x0226, 0x00 },
                    { 0x0227, 0x00 },
                    { 0x0228, 0x00 },
                    { 0x0229, 0x00 },
                    { 0x022A, 0x00 },
                    { 0x022B, 0x00 },
                    { 0x022C, 0x00 },
                    { 0x022D, 0x00 },
                    { 0x022E, 0x00 },
                    { 0x022F, 0x00 },
                    { 0x0235, 0x00 },
                    { 0x0236, 0x00 },
                    { 0x0237, 0x00 },
                    { 0x0238, 0x00 },
                    { 0x0239, 0x42 },
                    { 0x023A, 0x00 },
                    { 0x023B, 0x00 },
                    { 0x023C, 0x00 },
                    { 0x023D, 0x00 },
                    { 0x023E, 0x80 },
                    { 0x0250, 0x00 },
                    { 0x0251, 0x00 },
                    { 0x0252, 0x00 },
                    { 0x0253, 0x00 },
                    { 0x0254, 0x00 },
                    { 0x0255, 0x00 },
                    { 0x025C, 0x00 },
                    { 0x025D, 0x00 },
                    { 0x025E, 0x00 },
                    { 0x025F, 0x00 },
                    { 0x0260, 0x00 },
                    { 0x0261, 0x00 },
                    { 0x026B, 0x35 },
                    { 0x026C, 0x33 },
                    { 0x026D, 0x34 },
                    { 0x026E, 0x30 },
                    { 0x026F, 0x45 },
                    { 0x0270, 0x56 },
                    { 0x0271, 0x42 },
                    { 0x0272, 0x31 },
                    { 0x0302, 0x00 },
                    { 0x0303, 0x00 },
                    { 0x0304, 0x00 },
                    { 0x0305, 0x00 },
                    { 0x0306, 0x21 },
                    { 0x0307, 0x00 },
                    { 0x0308, 0x00 },
                    { 0x0309, 0x00 },
                    { 0x030A, 0x00 },
                    { 0x030B, 0x80 },
                    { 0x030C, 0x00 },
                    { 0x030D, 0x00 },
                    { 0x030E, 0x00 },
                    { 0x030F, 0x00 },
                    { 0x0310, 0x00 },
                    { 0x0311, 0x00 },
                    { 0x0312, 0x00 },
                    { 0x0313, 0x00 },
                    { 0x0314, 0x00 },
                    { 0x0315, 0x00 },
                    { 0x0316, 0x00 },
                    { 0x0317, 0x00 },
                    { 0x0318, 0x00 },
                    { 0x0319, 0x00 },
                    { 0x031A, 0x00 },
                    { 0x031B, 0x00 },
                    { 0x031C, 0x00 },
                    { 0x031D, 0x00 },
                    { 0x031E, 0x00 },
                    { 0x031F, 0x00 },
                    { 0x0320, 0x00 },
                    { 0x0321, 0x00 },
                    { 0x0322, 0x00 },
                    { 0x0323, 0x00 },
                    { 0x0324, 0x00 },
                    { 0x0325, 0x00 },
                    { 0x0326, 0x00 },
                    { 0x0327, 0x00 },
                    { 0x0328, 0x00 },
                    { 0x0329, 0x00 },
                    { 0x032A, 0x00 },
                    { 0x032B, 0x00 },
                    { 0x032C, 0x00 },
                    { 0x032D, 0x00 },
                    { 0x0338, 0x00 },
                    { 0x0339, 0x1F },
                    { 0x033B, 0x00 },
                    { 0x033C, 0x00 },
                    { 0x033D, 0x00 },
                    { 0x033E, 0x00 },
                    { 0x033F, 0x00 },
                    { 0x0340, 0x00 },
                    { 0x0341, 0x00 },
                    { 0x0342, 0x00 },
                    { 0x0343, 0x00 },
                    { 0x0344, 0x00 },
                    { 0x0345, 0x00 },
                    { 0x0346, 0x00 },
                    { 0x0347, 0x00 },
                    { 0x0348, 0x00 },
                    { 0x0349, 0x00 },
                    { 0x034A, 0x00 },
                    { 0x034B, 0x00 },
                    { 0x034C, 0x00 },
                    { 0x034D, 0x00 },
                    { 0x034E, 0x00 },
                    { 0x034F, 0x00 },
                    { 0x0350, 0x00 },
                    { 0x0351, 0x00 },
                    { 0x0352, 0x00 },
                    { 0x0359, 0x00 },
                    { 0x035A, 0x00 },
                    { 0x035B, 0x00 },
                    { 0x035C, 0x00 },
                    { 0x035D, 0x00 },
                    { 0x035E, 0x00 },
                    { 0x035F, 0x00 },
                    { 0x0360, 0x00 },
                    { 0x0802, 0x00 },
                    { 0x0803, 0x00 },
                    { 0x0804, 0x00 },
                    { 0x0805, 0x00 },
                    { 0x0806, 0x00 },
                    { 0x0807, 0x00 },
                    { 0x0808, 0x00 },
                    { 0x0809, 0x00 },
                    { 0x080A, 0x00 },
                    { 0x080B, 0x00 },
                    { 0x080C, 0x00 },
                    { 0x080D, 0x00 },
                    { 0x080E, 0x00 },
                    { 0x080F, 0x00 },
                    { 0x0810, 0x00 },
                    { 0x0811, 0x00 },
                    { 0x0812, 0x00 },
                    { 0x0813, 0x00 },
                    { 0x0814, 0x00 },
                    { 0x0815, 0x00 },
                    { 0x0816, 0x00 },
                    { 0x0817, 0x00 },
                    { 0x0818, 0x00 },
                    { 0x0819, 0x00 },
                    { 0x081A, 0x00 },
                    { 0x081B, 0x00 },
                    { 0x081C, 0x00 },
                    { 0x081D, 0x00 },
                    { 0x081E, 0x00 },
                    { 0x081F, 0x00 },
                    { 0x0820, 0x00 },
                    { 0x0821, 0x00 },
                    { 0x0822, 0x00 },
                    { 0x0823, 0x00 },
                    { 0x0824, 0x00 },
                    { 0x0825, 0x00 },
                    { 0x0826, 0x00 },
                    { 0x0827, 0x00 },
                    { 0x0828, 0x00 },
                    { 0x0829, 0x00 },
                    { 0x082A, 0x00 },
                    { 0x082B, 0x00 },
                    { 0x082C, 0x00 },
                    { 0x082D, 0x00 },
                    { 0x082E, 0x00 },
                    { 0x082F, 0x00 },
                    { 0x0830, 0x00 },
                    { 0x0831, 0x00 },
                    { 0x0832, 0x00 },
                    { 0x0833, 0x00 },
                    { 0x0834, 0x00 },
                    { 0x0835, 0x00 },
                    { 0x0836, 0x00 },
                    { 0x0837, 0x00 },
                    { 0x0838, 0x00 },
                    { 0x0839, 0x00 },
                    { 0x083A, 0x00 },
                    { 0x083B, 0x00 },
                    { 0x083C, 0x00 },
                    { 0x083D, 0x00 },
                    { 0x083E, 0x00 },
                    { 0x083F, 0x00 },
                    { 0x0840, 0x00 },
                    { 0x0841, 0x00 },
                    { 0x0842, 0x00 },
                    { 0x0843, 0x00 },
                    { 0x0844, 0x00 },
                    { 0x0845, 0x00 },
                    { 0x0846, 0x00 },
                    { 0x0847, 0x00 },
                    { 0x0848, 0x00 },
                    { 0x0849, 0x00 },
                    { 0x084A, 0x00 },
                    { 0x084B, 0x00 },
                    { 0x084C, 0x00 },
                    { 0x084D, 0x00 },
                    { 0x084E, 0x00 },
                    { 0x084F, 0x00 },
                    { 0x0850, 0x00 },
                    { 0x0851, 0x00 },
                    { 0x0852, 0x00 },
                    { 0x0853, 0x00 },
                    { 0x0854, 0x00 },
                    { 0x0855, 0x00 },
                    { 0x0856, 0x00 },
                    { 0x0857, 0x00 },
                    { 0x0858, 0x00 },
                    { 0x0859, 0x00 },
                    { 0x085A, 0x00 },
                    { 0x085B, 0x00 },
                    { 0x085C, 0x00 },
                    { 0x085D, 0x00 },
                    { 0x085E, 0x00 },
                    { 0x085F, 0x00 },
                    { 0x0860, 0x00 },
                    { 0x0861, 0x00 },
                    { 0x090E, 0x00 },
                    { 0x091C, 0x04 },
                    { 0x0943, 0x00 },
                    { 0x0949, 0x02 },
                    { 0x094A, 0x20 },
                    { 0x094E, 0x49 },
                    { 0x094F, 0x02 },
                    { 0x095E, 0x00 },
                    { 0x0A02, 0x00 },
                    { 0x0A03, 0x01 },
                    { 0x0A04, 0x01 },
                    { 0x0A05, 0x01 },
                    { 0x0A14, 0x00 },
                    { 0x0A1A, 0x00 },
                    { 0x0A20, 0x00 },
                    { 0x0A26, 0x00 },
                    { 0x0B44, 0x0F },
                    { 0x0B4A, 0x0E },
                    { 0x0B57, 0x81 },
                    { 0x0B58, 0x00 },
                    /* End configuration registers */
        }
      }

    }
  );



