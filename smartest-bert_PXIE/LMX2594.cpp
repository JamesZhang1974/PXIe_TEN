/*!
 \file   LMX2594.cpp
 \brief  LMX2594 Clock Hardware Interface Class Implementation - Derived from LMX2594
 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <QDebug>

#include "globals.h"
#include "BertFile.h"

#include "LMX2594.h"


// Debug macro for general debug mesages from LMX
// #define LMX_REGISTER_DEBUG
// #define LMX_PROFILE_DEBUG
#define DEBUG_LMX(MSG) qDebug() << "\t" << MSG;

#ifdef LMX_PROFILE_DEBUG
  #define DEBUG_LMX_PROFILES(MSG) qDebug() << "\t" << MSG;
#else
  #define DEBUG_LMX_PROFILES(MSG)
#endif

// Enable Lock Detect Pin: Define the following to set up MUXout pin as Lock Detect;
// If not defined, MUXout will be set up as MISO serial out. Pin MUST be set up as
// MISO if using readRegister!!
#define LD_LED_ENABLE 1

const QString LMX2594::PART_NO = QString("LMX2594");

/* // DEPRECATED
// Constants used to populate the Trigger Divide Ratio list:
#ifdef BERT_DEBUG_DIV_RATIOS
const QStringList LMX2594::TRIGGER_DIVIDE_LIST =
  { "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/128", "1/192" };
const size_t LMX2594::DEFAULT_DIVIDE_RATIO_INDEX = 6;
#else
const QStringList LMX2594::TRIGGER_DIVIDE_LIST =
  { "1", "1/2", "1/4" };
const size_t LMX2594::DEFAULT_DIVIDE_RATIO_INDEX = 1;
#endif
*/

// Constants used for the "OUTA_POW" and "OUTB_POW" register fields.
// These entries correspond to the list of power settings supplied
// by getOutputPowerList method.
const uint8_t LMX2594::POWER_M5DBM = 0;     // UNUSED!
const uint8_t LMX2594::POWER_0DBM  = 10;
const uint8_t LMX2594::POWER_5DBM  = 20;
const uint8_t LMX2594::POWER_10DBM = 31;    // UNUSED!
const uint8_t LMX2594::POWER_15DBM = 60;    // UNUSED!

const QStringList LMX2594::TRIGOUT_POWER_LIST = { " 0 dBm", " 5 dBm" };    //
const QList<uint8_t> LMX2594::POWER_CONSTS = { POWER_0DBM, POWER_5DBM };   // Used for trigger out (Main out fixed at 0 dBm)

const size_t LMX2594::DEFAULT_FOUT_POWER_INDEX = 0;    // Default power setting for main clock (0 bBm)
const size_t LMX2594::DEFAULT_TRIG_POWER_INDEX = 1;    // Default power setting for trigger out (5 DBM)


QList<LMXFrequencyProfile> LMX2594::frequencyProfiles = QList<LMXFrequencyProfile>();
QStringList LMX2594::frequencyList = QStringList();
uint16_t LMX2594::profileIndexDefault = 0;    // Index of default start-up frequency profile
// DEPRECATED int LMX2594::instanceCount = 0;
// DEPRECATED bool LMX2594::frequencyProfilesOK = false;

QList<LMXFrequencyProfile> LMX2594::frequencyProfilesFromFiles = QList<LMXFrequencyProfile>();
QStringList LMX2594::frequencyListFromFiles = QStringList();

//QList<GennumFirmware> LMX2594::firmwareFromFiles = QList<GennumFirmware>();
//QStringList LMX2594::firmwareListFromFiles = QStringList();


// Bit patterns for Register 0:
const uint16_t LMX2594::R0_NOTHING           = 0b0010010000010000;  // No bits set other than preset bits (don't change these)
const uint16_t LMX2594::R0_RAMP_EN           = 0b1000000000000000;  // RAMP_EN bit
const uint16_t LMX2594::R0_VCO_PHASE_SYNC_EN = 0b0100000000000000;  // VCO_PHASE_SYNC_EN bit
const uint16_t LMX2594::R0_OUT_MUTE          = 0b0000001000000000;  // OUT_MUTE bit = Mute output during FCAL
const uint16_t LMX2594::R0_FCAL_EN           = 0b0000000000001000;  // FCAL_EN bit = Run FCal
const uint16_t LMX2594::R0_MUXOUT_LD_SEL     = 0b0000000000000100;  // MUXOUT_LD_SEL bit
const uint16_t LMX2594::R0_RESET             = 0b0000000000000010;  // RESET bit = Reset
const uint16_t LMX2594::R0_POWERDOWN         = 0b0000000000000001;  // POWERDOWN bit

const uint16_t LMX2594::R0_FCAL_XXXX_ADJ_MASK = 0b0000000111100000;  // Mask to preserve ONLY FCAL_XXXX_ADJ bits

// Default value to load into R0 for normal running:
// Define "LD_LED_ENABLE" to enable the MUXout pin as Lock Detect (Enables Lock LED)
// If undefined, the MUXout pin will function as MISO for SPI data.
// The MUXout pin MUST be set to MISO if register readback functions are used (e.g. GetLMXVTuneLock slot)
const uint16_t LMX2594::R0_DEFAULT = LMX2594::R0_NOTHING         // Preset bits only (don't change these)
#ifdef LD_LED_ENABLE
                                   | LMX2594::R0_MUXOUT_LD_SEL   // MUXout pin set to Lock Detect (no serial out; readRegister WON'T work!)
#endif
                                   | LMX2594::R0_FCAL_EN;        // FCal enable (automatic FCal on Freq change)


// On the LMX2594, R1 and R7 are also used as part of basic configuration
// (CAL_CLK_DIV field and OUT_FORCE bit):
const uint16_t LMX2594::R1_DEFAULT     = 0b0000100000001000;  // Set up CAL_CLK_DIV for 100 MHz Input Oscillator
const uint16_t LMX2594::R7_DEFAULT     = 0b0100000010110010;  // Make sure OUT_FORCE is ON (required when NOT using OUT_MUTE in R0)

const uint16_t LMX2594::R31_DEFAULT    = 0b0000001111101100;  // CHDIV_DIV2 Disabled (for CHDIV <= 2)

const uint16_t LMX2594::R44_DEFAULT    = 0b0000000011100000; // OutA Power at minimum; Outputs A and B powered down; Other bits default

const uint16_t LMX2594::R45_DEFAULT    = 0b1100111011000000; // OUTA_MUX = VCO; ISET = Min (11); OUTB_PRW = Minimum
const uint16_t LMX2594::R45_OUTA_CHDIV = 0b1100011011000000; // OUTA_MUX = CHDIV; ISET = Min (11); OUTB_PRW = Minimum

const uint16_t LMX2594::R46_DEFAULT    = 0b0000011111111101; // OUTB_MUX = VCO
const uint16_t LMX2594::R46_OUTB_CHDIV = 0b0000011111111100; // OUTB_MUX = CHDIV (DIVIDED output)

const uint16_t LMX2594::R75_DEFAULT    = 0b0000100000000000; // CHDIV_DIV2 Disabled (for CHDIV <= 2)


/* DEPRECATED Not using output divider.
// Settings for R31 and R75 for various trigger out divide ratios:
const uint16_t LMX2594::R31_VALUES[] =
{
    0x03EC,   //   1/1   // Don't care; will be using VCO out
    0x03EC,   //   1/2
    0x43EC,   //   1/4
#ifdef BERT_DEBUG_DIV_RATIOS
    0x43EC,   //   1/8
    0x43EC,   //   1/16
    0x43EC,   //   1/32
    0x43EC,   //   1/128
    0x43EC    //   1/192
#endif
};

const uint16_t LMX2594::R75_VALUES[] =
{
    0x0800,   //   1/1   // Don't care; will be using VCO out
    0x0800,   //   1/2
    0x0840,   //   1/4
#ifdef BERT_DEBUG_DIV_RATIOS
    0x08C0,   //   1/8
    0x0940,   //   1/16
    0x09C0,   //   1/32
    0x0B00,   //   1/128
    0x0B40,   //   1/192
#endif
};
const size_t LMX2594::R31R75_VALUES_SIZE = sizeof(R31_VALUES) / sizeof(R31_VALUES[0]);
*/


LMX2594::LMX2594(I2CComms *comms,
                 const uint8_t i2cAddress,
                 const int deviceID,
                 M24M02 *eeprom)
// DEPRECATED     ,const QString registerFilePath)
 : comms(comms), i2cAddress(i2cAddress), deviceID(deviceID), eeprom(eeprom)
{
    selectedTrigDivideIndex = 0; // DEPRECATED  DEFAULT_DIVIDE_RATIO_INDEX;
    selectedFOutOutputPowerIndex = DEFAULT_FOUT_POWER_INDEX;
    selectedTrigOutputPowerIndex = DEFAULT_TRIG_POWER_INDEX;

    /* DEPRECATED MOVED
    // Read frequency profiles (Nb: These are static, so only read for first instantiation):
    if (instanceCount == 0)
    {
        int result = initFrequencyProfiles(registerFilePath, LMX2594::PART_NO);
        if (result == globals::OK)
        {
            frequencyProfilesOK = true;
            setSafeDefaults();
        }
        else
        {
            DEBUG_LMX("Error initialising LMX: frequency definition files not found or invalid (" << result << ")!")
        }
    }
    instanceCount++;
    */
}

LMX2594::~LMX2594()
{
    // Clean up frequency profiles:
    frequencyProfiles.clear();
    frequencyList.clear();

    frequencyProfilesFromFiles.clear();
    frequencyListFromFiles.clear();

}


//*****************************************************************************************
//   LMX Clock Synth Methods
// *****************************************************************************************

/*!
 \brief Check to see whether there is an SC18IS602B I2C->SPI adaptor
        on the specified I2C address. If so, we assume there's an
        LMX clock module connected to it.
 \param comms       I2C Comms (created elsewhere)
 \param i2cAddress  I2C address to check on (7 bit, i.e. no R/W bit)
 \return  true   SC18IS602B responded OK
 \return  false  No response... clock not found.
*/
bool LMX2594::ping(I2CComms *comms, const uint8_t i2cAddress)
{
    qDebug() << "LMX2594: Searching for clock adaptor on address " << INT_AS_HEX(i2cAddress,2) << "...";
    int result = initAdaptor(comms, i2cAddress);
    if (result == globals::OK) return true;
    else                       return false;
}


/*!
  \brief Determine the clock part number by looking at register definition files
         Register definitions are stored in ".tcs" file format (generated by
         Texas Instruments TICS Pro software). This method searches a specified
         directory for the first suitable file, and returns a string containing
         the part number, e.g. "LMX2592".

  \param searchPath   File path to search for files
  \param partNo       On success, this will be set to the part number

 \return globals::OK
 \return globals::DIRECTORY_NOT_FOUND
 \return globals::FILE_ERROR
*/
int LMX2594::getPartFromRegisterFiles(const QString searchPath, QString &partNo)
{
    qDebug() << "LMX xxxx: Find clock part number from frequency profiles in " << searchPath;
    QStringList freqFiles;
    int result;
    result = BertFile::readDirectory(searchPath, freqFiles);
    if (freqFiles.empty())
    {
        qDebug() << "No frequency profiles found. (Error: " << result << ")";
        return result;
    }
    qDebug() << freqFiles.count() << " files found. Parsing...";
    bool flagSetupSection = false;
    foreach( QString fileName, freqFiles )
    {
        qDebug() << "File: " << fileName;
        QStringList defLines;
        result = BertFile::readFile(searchPath + QString("\\") + QString(fileName), 100, defLines);  // Setup section must be in the first 100 lines.
        if (defLines.empty())
        {
            qDebug() << "  -Couldn't read file! (Error: " << result << ")";
        }
        else
        {
            foreach( QString defLineFull, defLines )
            {
                QString defLine = defLineFull.trimmed();
                if (defLine.left(1) == "[")
                {
                    // Section break;
                    if (defLine == "[SETUP]") flagSetupSection = true;  // New section is "[SETUP]"
                    else                      flagSetupSection = false;
                    continue;
                }
                else
                {
                    if (flagSetupSection
                     && defLine.length() > 5
                     && defLine.left(5) == "PART=")
                    {
                        // PART=... line found!
                        partNo = defLine.mid(5);
                        qDebug() << "  -Found part number: " << partNo;
                        return globals::OK;
                    }
                }
            }
        }
    }
    qDebug() << "Warning: No part number found!";
    return globals::GEN_ERROR;  // Part number not found!
}



/*!
 \brief Get Hardware Options for LMX2594
        Requests that this module emit signals describing its available options lists to client
*/
void LMX2594::getOptions()
{
    // DEPRECATED  MOVED to init emit ListPopulate("listLMXFreq",            globals::ALL_LANES, frequencyList,      profileIndexDefault);
    emit ListPopulate("listLMXTrigOutPower",    globals::ALL_LANES, TRIGOUT_POWER_LIST, DEFAULT_TRIG_POWER_INDEX);

    // DEPRECATED Moved to PCA9557 module:
    // emit ListPopulate("listLMXTrigOutDivRatio", globals::ALL_LANES, TRIGGER_DIVIDE_LIST, DEFAULT_DIVIDE_RATIO_INDEX);
}


/*!
 \brief Initialise LMX Interface
 This method sets up the I2C->SPI adaptor used to communicate
 with the LMX, then initialises the LMX.
 Assumes that general comms are already open (bertComms object).
 \return globals::OK   Success
 \return [error code]  Failed to initialise the LMX
*/
int LMX2594::init()
{
    qDebug() << "LMX2594: Init for LMX ID " << deviceID << "; I2C Address " << INT_AS_HEX(i2cAddress,2);

    int result = globals::OK;

    // Read frequency profiles from EEPROM:
    result = eeprom->readFrequencyProfiles(0, frequencyProfiles);
    // Make sure we have frequency profiles:
    if (result != globals::OK || frequencyProfiles.count() == 0)
    {
        qDebug() << "Error initialising LMX: No frequency profiles!";
        return globals::MISSING_LMX;
    }
    // Create list of frequencies for UI:
    // Nb: We assume the profiles were already sorted in ascending order
    // of frequency when stored to EEPROM!
    int index = 0;
    foreach (LMXFrequencyProfile profile, frequencyProfiles)
    {
        float thisFrequency = profile.getFrequency();
        frequencyList.append(
                    QString().sprintf("%2.5f GHz  (%2.5f Gbps)",
                                      static_cast<double>(thisFrequency) / 1000.0,
                                      static_cast<double>(thisFrequency) / 500.0)
                    );
        // Profiles should already be sorted by frequency.
        // Set profileIndexDefault to the index of the highest frequency
        // less than or equal to 14.5GHz.
        if (thisFrequency <= 14500.0f) profileIndexDefault = static_cast<uint16_t>(index);
        index++;
    }
    emit ListPopulate("listLMXFreq", globals::ALL_LANES, frequencyList, profileIndexDefault);

    emit ShowMessage("Configuring Frequency Synthesizer...");
    // Initialise the comms (I2C to SPI adaptor):
    result = initAdaptor(comms, i2cAddress);
    if (result != globals::OK)
    {
        spiAdaptorIsOpen = false;
        return result;  // ERROR setting up comms!
    }
    spiAdaptorIsOpen = true;

    // Initialise the LMX to safe startup state:
    result = initPart();
    if (result != globals::OK) return result;

    GetLMXInfo();   // Update the client to reflect the new settings
    return globals::OK;
}



// =================================================================================
//  SLOTS
// =================================================================================

/*!
 \brief Get LMX Info Slot: Request for info about current clock settings.
*/
void LMX2594::GetLMXInfo()
{
    float frequency = 0.0;
    getFrequency(selectedProfileIndex, &frequency);
    emit LMXInfo(deviceID,
                 selectedProfileIndex,
                 selectedTrigOutputPowerIndex,
                 selectedFOutOutputPowerIndex,
                 selectedTrigDivideIndex,
                 flagOutputsOn,
                 frequency);
}


/*!
 \brief Get LMX VTune Lock Status Slot: Request for the current state of VCO VTune Lock
 NOTE: This reads the 'rb_LD_VTUNE' value from Register 110. To work, the MUXout pin MUST
 be set up as a serial out (MISO); see LD_LED_ENABLE def (controls MUXout pin assignment).
 See also PCA9557 slot 'ReadLMXLockDetect' to read the voltage of the Lock Detect pin.
*/
void LMX2594::GetLMXVTuneLock()
{
    uint16_t R110 = 0;
    int result = readRegister(110, &R110);
    if (result == globals::OK)
    {
        int ldVTune = (R110 >> 9) & 0x03;
        DEBUG_LMX("LMX2594: Read R110 OK; rb_LD_VTUNE = " << ldVTune)
        if (ldVTune == 2) emit LMXVTuneLock(deviceID, true);
        else              emit LMXVTuneLock(deviceID, false);

    }
    else
    {
        DEBUG_LMX("LMX2594: Error reading R110: " << result)
        emit LMXVTuneLock(deviceID, false);
    }
}


/*!
 \brief Slot: Select frequency profile
 \param indexProfile   Index of profile to select
 \param triggerResync  OPTIONAL: Should this change trigger a resync of other components?
                       If true (default), an "LMX Settings Changed" signal is emitted.
                       Set to false if caller is going to do their own resync of other
                       components, or doesn't care.
*/
void LMX2594::SelectProfile(int indexProfile, bool triggerResync)
{
    DEBUG_LMX("LMX2594: Select frequency profile " << indexProfile << " on Clock " << deviceID)
    emit ShowMessage("Changing Synthesizer Frequency...");
    int result = selectProfile(indexProfile);
    if (result == globals::OK)
    {
        emit Result(globals::OK, globals::ALL_LANES);
        emit ShowMessage("OK.");
    }
    else
    {
        emit ShowMessage("Error selecting frequency!");
        emit Result(result, globals::ALL_LANES);
    }
    GetLMXInfo();                                         // Update the client to reflect the new settings
    if (triggerResync) emit LMXSettingsChanged(deviceID); // Inform the client that settings have changed - other parts of the system may need to resync.
}



/* DEPRECATED:
 *  No longer any slot to select Trigger Divide; Both RF outputs will be set to
 *  /1 (no divide) and an external divider will be used on the trigger out.
 *
 * \brief Slot: Select Trigger Divide Ratio
 * \param indexTriggerDivide  Index of new divide ratio
 * \param doFCal              Should a frequency calibration be carried out after changing?
*
void LMX2594::SelectTriggerDivide(int indexTriggerDivide, bool doFCal)
{
    DEBUG_LMX("LMX2594: Select trigger divide " << indexTriggerDivide << " on Clock " << deviceID)
    int result = selectTriggerDivide((const size_t)indexTriggerDivide, doFCal);
    if (result == globals::OK)
    {
        emit Result(globals::OK, globals::ALL_LANES);
    }
    else
    {
        emit ShowMessage("Error selecting trigger divide ratio!");
        emit Result(result, globals::ALL_LANES);
    }
    GetLMXInfo();  // Update the client to reflect the new settings
    // Nb: Don't need to emit 'LMX Settings Changed' for changes to trigger out.
}
*/


/*!
 \brief Slot: Configure Outputs
 \param indexFOutOutputPower  Output power selection;         Use -1 to select default index (DEFAULT_FOUT_POWER_INDEX)
 \param indexTrigOutputPower  Trigger output power selection; Use -1 to select default index (DEFAULT_TRIG_POWER_INDEX)
 \param outputsOn             Outputs on / off
 \param triggerResync  OPTIONAL: Should this change trigger a resync of other components?
                       If true (default), an "LMX Settings Changed" signal is emitted.
                       Set to false if caller is going to do their own resync of other
                       components, or doesn't care.
 */
void LMX2594::ConfigureOutputs(int indexFOutOutputPower, int indexTrigOutputPower, bool outputsOn, bool triggerResync)
{
    DEBUG_LMX("LMX: Configure outputs on clock " << deviceID << ": FOut power index = " << indexFOutOutputPower << "; TrigOut power index = " << indexTrigOutputPower << "; Outputs On: " << outputsOn)
    bool majorSettingsChange = false;
    int useIndexFOutOutputPower = indexFOutOutputPower;
    int useIndexTrigOutputPower = indexTrigOutputPower;
    if (useIndexFOutOutputPower < 0) useIndexFOutOutputPower = DEFAULT_FOUT_POWER_INDEX;
    if (useIndexTrigOutputPower < 0) useIndexTrigOutputPower = DEFAULT_TRIG_POWER_INDEX;

    if (useIndexFOutOutputPower != selectedFOutOutputPowerIndex
     || outputsOn != flagOutputsOn) majorSettingsChange = true;
        // Need to signal an important settings change if main out power has changed
        // or outputs turned on or off. No need if we only changed trigger output power.
    selectedFOutOutputPowerIndex = static_cast<uint16_t>(useIndexFOutOutputPower);
    selectedTrigOutputPowerIndex = static_cast<uint16_t>(useIndexTrigOutputPower);
    flagOutputsOn = outputsOn;
    int result = configureOutputs();
    if (result == globals::OK)
    {
        emit Result(globals::OK, globals::ALL_LANES);
    }
    else
    {
        emit ShowMessage("Error configuring clock outputs!");
        emit Result(result, globals::ALL_LANES);
    }
    GetLMXInfo();    // Update the client to reflect the new settings
    if (majorSettingsChange && triggerResync) emit LMXSettingsChanged(deviceID);
        // Inform the client that settings have changed - other parts of the system may need to resync.
}


void LMX2594::ReadTcsFrequencyProfiles(QString searchPath)
{
    if (deviceID != 0) return;   // This slot is only implemented for MASTER LMX.
    DEBUG_LMX("LMX: Read TCS frequency profile files for clock " << deviceID << " from " << searchPath)
    int result = globals::OK;
    result = LMX2594::getProfilesFromRegisterFiles(searchPath, PART_NO);
    if (result == globals::OK)
    {
        emit ListPopulate("listTCSFreq", globals::ALL_LANES, frequencyListFromFiles, 0);
    }
    else
    {
        DEBUG_LMX("LMX2594: Error reading frequency profile files from path " << searchPath << ": " << result)
    }
}



void LMX2594::LMXEEPROMWriteFrequencyProfiles()
{
    if (deviceID != 0) return;   // This slot is only implemented for MASTER LMX.
    DEBUG_LMX("LMX: Write frequency profile files to EEPROM for clock " << deviceID)
    int result = eeprom->writeFrequencyProfiles(0, frequencyProfilesFromFiles);
    if (result != globals::OK)
    {
        DEBUG_LMX("LMX2594: Error writing frequency profiles to EEPROM: " << result)
    }
    emit Result(result, globals::ALL_LANES);
}



/*!
 \brief Verify Frequency Profiles
 Check whether frequencies read from TCS files match those read from EEPROM.
 This should be TRUE once frequencies have been written to EEPROM with
 LMXEEPROMWriteFrequencyProfiles signal, then the profiles have been reloaded
 from EEPROM by disconnect followed by reconnect (with same TCS files in clockdefs dir).
*/
void LMX2594::LMXVerifyFrequencyProfiles()
{
    if (deviceID != 0) return;   // This slot is only implemented for MASTER LMX.
    qDebug() << "===================================";
    qDebug() << " Verify LMX Frequency Profiles:";
    qDebug() << "===================================";
    qDebug() << "Check Count: " << frequencyProfilesFromFiles.count() << " from FILES; "
                                << frequencyProfiles.count() << " from EEPROM";
    if (frequencyProfilesFromFiles.count() == frequencyProfiles.count())
    {
        qDebug() << "--OK.";
    }
    else
    {
        qDebug() << "--Count of frequencies not the same!";
        emit ShowMessage(QString("Verify FAILED: Number of profiles is different (%1 in FILES; %2 in EEPROM)")
                         .arg(frequencyProfilesFromFiles.count())
                         .arg(frequencyProfiles.count()));
        emit Result(globals::GEN_ERROR, globals::ALL_LANES);
        return;
    }

    for (int index = 0; index < frequencyProfilesFromFiles.count(); index++)
    {
        qDebug() << "Check Profile " << index
                 << ": Freq from FILE = " << frequencyProfilesFromFiles[index].getFrequency()
                 << "; Freq from EEPROM = " << frequencyProfiles[index].getFrequency();

        if (static_cast<int>(frequencyProfilesFromFiles[index].getFrequency())
         == static_cast<int>(frequencyProfiles[index].getFrequency()))
        {
            qDebug() << "--Frequencies match.";
        }
        else
        {
            qDebug() << "--Frequencies not the same!";
            emit ShowMessage(QString("Verify FAILED: Profile frequency is different at profile %1 (%2 in FILES; %3 in EEPROM)")
                             .arg(index)
                             .arg(static_cast<double>(frequencyProfilesFromFiles[index].getFrequency()))
                             .arg(static_cast<double>(frequencyProfiles[index].getFrequency())));
            emit Result(globals::GEN_ERROR, globals::ALL_LANES);
            return;
        }

        if (frequencyProfilesFromFiles[index].getRegisterCount()
         == frequencyProfiles[index].getRegisterCount())
        {
            qDebug() << "--Register counts match.";
        }
        else
        {
            qDebug() << "--Different number of registers!";
            emit ShowMessage(QString("Verify FAILED: Number of registers is different at profile %1 (%2 in FILES; %3 in EEPROM)")
                             .arg(index)
                             .arg(frequencyProfilesFromFiles[index].getRegisterCount())
                             .arg(frequencyProfiles[index].getRegisterCount()));
            emit Result(globals::GEN_ERROR, globals::ALL_LANES);
            return;
        }
        qDebug() << "Compare register values for Profile " << index << "...";
        for (int regAddr = 0; regAddr < frequencyProfilesFromFiles[index].getRegisterCount(); regAddr++)
        {
            bool regFound;
            uint16_t regValueFile = frequencyProfilesFromFiles[index].getRegisterValue(static_cast<uint8_t>(regAddr), &regFound);
            uint16_t regValueEEPROM = frequencyProfiles[index].getRegisterValue(static_cast<uint8_t>(regAddr), &regFound);
            if (regValueFile != regValueEEPROM)
            {
                qDebug() << "--Different register values at address " << regAddr
                         << ": FILE = " << regValueFile << "; EEPROM = " << regValueEEPROM;
                emit ShowMessage(QString("Verify FAILED: Register value is different at profile %1, reg %2 (%3 in FILES; %4 in EEPROM)")
                                 .arg(index)
                                 .arg(regAddr)
                                 .arg(regValueFile)
                                 .arg(regValueEEPROM));
                emit Result(globals::GEN_ERROR, globals::ALL_LANES);
                return;
            }
        }
        qDebug() << "--Register values match.";
    }

    qDebug() << "===================================";
    qDebug() << "LMX Frequency Profiles Verified OK!";
    qDebug() << "===================================";
    emit Result(globals::OK, globals::ALL_LANES);
    emit ShowMessage("LMX Frequency Profiles Verified OK!");
}


/*!
 \brief Slot: Reset LMX Device to Defaults
*/
void LMX2594::ResetDevice()
{
    DEBUG_LMX("LMX2594: Reset Device - Clock " << deviceID)
    int result = resetDevice();
    if (result == globals::OK)
    {
        emit Result(globals::OK, globals::ALL_LANES);
    }
    else
    {
        emit ShowMessage("Error resetting clock!");
        emit Result(result, globals::ALL_LANES);
    }
    GetLMXInfo();  // Update the client to reflect the new settings
}

/*!
 \brief Slot: Set LMX Enable status (enable / disable)
  From LMX2594 Manual: "The LMX2594 can be powered up and down using the CE pin...
  When the device comes out of the powered down state, register R0 must be
  programmed with FCAL_EN high again to re-calibrate the device.

 \param enabled
*/
void LMX2594::SetLMXEnable(bool enabled)
{
    DEBUG_LMX("LMX2594: Set Enabled State to " << enabled << " on clock " << deviceID)
    int result = setLMXEnable(enabled);
    if (result == globals::OK)
    {
        emit Result(globals::OK, globals::ALL_LANES);
        if (enabled)
        {
            runFCal();     // This should be enough to get the clock locked again (see LMX manual)
            GetLMXInfo();  // Update the client to reflect the new settings
            emit LMXSettingsChanged(deviceID);   // Signal other devices that a clock change has occurred
        }
    }
    else
    {
        emit ShowMessage("Error disabling/enabling clock!");
        emit Result(result, globals::ALL_LANES);
    }
}




/*****************************************************************************************/
/* PRIVATE Methods                                                                       */
/*****************************************************************************************/



/*!
 \brief Build a list of frequency profiles based on ".tcs" files found in a specified directory
 \return globals::OK
 \return globals::DIRECTORY_NOT_FOUND
 \return globals::FILE_ERROR
*/
int LMX2594::getProfilesFromRegisterFiles(const QString registerFilePath, QString partNo)
{
    qDebug() << "LMX2594: Load frequency profiles from " << registerFilePath;
    frequencyProfilesFromFiles.clear();   // Remove any old profiles
    frequencyListFromFiles.clear();       //
    QStringList freqFiles;
    int result;
    result = BertFile::readDirectory(registerFilePath, freqFiles);
    if (freqFiles.empty())
    {
        qDebug() << "LMX2594: ERROR: No frequency profiles found. (Error: " << result << ")";
        return result;
    }
    DEBUG_LMX_PROFILES(freqFiles.count() << " files found. Parsing...")
    foreach( QString fileName, freqFiles )
    {
        DEBUG_LMX_PROFILES("File: " << fileName)
        QStringList defLines;
        result = BertFile::readFile(registerFilePath + QString("\\") + QString(fileName), 400, defLines);
        if (defLines.empty())
        {
            qDebug() << "LMX2594: ERROR: Couldn't read file! (File: " << fileName << "; Error: " << result << ")";
        }
        else
        {
            // DEPRECATED LMXFrequencyProfile thisFrequencyProfile(partNo, REGISTER_COUNT, defLines);
            LMXFrequencyProfile thisFrequencyProfile(REGISTER_COUNT);

            result = parseTcsFrequencyProfile(defLines, partNo, thisFrequencyProfile);

            if (result == globals::OK && thisFrequencyProfile.isValid())
            {
                // File was parsed OK! (valid frequency profile):
                if (frequencyProfilesFromFiles.empty())
                {
                    frequencyProfilesFromFiles.append(thisFrequencyProfile);
                }
                else
                {
                    // Already have frequencies. We want to insert this one
                    // in the correct place in the list (sorted by frequency):
                    QList<LMXFrequencyProfile>::iterator i;
                    for (i = frequencyProfilesFromFiles.begin(); i != frequencyProfilesFromFiles.end(); ++i)
                    {
                        if (i->getFrequency() >= thisFrequencyProfile.getFrequency())
                        {
                            frequencyProfilesFromFiles.insert(i, thisFrequencyProfile);
                            break;
                        }
                    }
                    if (i == frequencyProfilesFromFiles.end())
                    {
                        frequencyProfilesFromFiles.append(thisFrequencyProfile);
                    }
                }
                DEBUG_LMX_PROFILES("Parsed OK; Freq = " << thisFrequencyProfile.getFrequency())
            }
        }
    }



    // Create list of frequency values to display in UI:
    uint16_t thisIndex = 0;
    QList<LMXFrequencyProfile>::iterator i;
    for (i = frequencyProfilesFromFiles.begin(); i != frequencyProfilesFromFiles.end(); ++i)
    {
        float thisFrequency = i->getFrequency();
        frequencyListFromFiles.append(
                    QString().sprintf("%2.5f GHz  (%2.5f Gbps)",
                                      static_cast<double>(thisFrequency) / 1000.0,
                                      static_cast<double>(thisFrequency) / 500.0)
                    );
        thisIndex++;
    }
    DEBUG_LMX_PROFILES("-----------------------------")
    DEBUG_LMX_PROFILES(frequencyProfilesFromFiles.count() << " frequency profiles found.")
    if (frequencyProfilesFromFiles.count() > 0)
    {
        DEBUG_LMX_PROFILES("Default profile: " << profileIndexDefault << " (" << frequencyProfilesFromFiles.at(profileIndexDefault).getFrequency() << " MHz)")
    }
    DEBUG_LMX_PROFILES("-----------------------------")

    return globals::OK;
}





/*!
 \brief Parse the content of a .tcs file (TI Software) and generate a Frequency Profile
 \param partNo
 \param registerCount
 \param fileContent
 \param profileToFill
 \return
*/
int LMX2594::parseTcsFrequencyProfile(QStringList &fileContent, const QString partNo, LMXFrequencyProfile &profileToFill)
{
    if (fileContent.empty()) return globals::INVALID_DATA; // Nothing to parse.

    enum fileSection_t
    {
        SECTION_UNKNOWN,
        SECTION_SETUP,
        SECTION_PINS,
        SECTION_MODES,
        SECTION_FLEX
    };

    bool flagSetupOK = false;
    bool flagFrequencyOK = false;
    enum fileSection_t currentSection = SECTION_UNKNOWN;
    // DEPRECATED LMXRegister foundRegister;
    uint8_t foundRegAddress = 0;
    uint16_t foundRegValue = 0;
    QString partNoFull = "PART=";
    partNoFull.append(partNo);

    foreach( QString defLineFull, fileContent )
    {
        QString defLine = defLineFull.trimmed();

        // Check for new section: //////////////////
        if (defLine == "[SETUP]")
        {
            currentSection = SECTION_SETUP;
            continue;
        }
        if (defLine == "[PINS]")
        {
            currentSection = SECTION_PINS;
            continue;
        }
        if (defLine == "[MODES]")
        {
            // Check to make sure we've seen "PART=LMXxxxx":
            if (!flagSetupOK)
            {
                qDebug() << "LMX2594: Parse TCS Files: Parse ERROR: Got to [MODES] but didn't see '" << partNoFull << "'" << endl
                         << "Giving up.";
                return globals::INVALID_DATA;
            }
            currentSection = SECTION_MODES;
            continue;
        }
        if (defLine == "[FLEX]")
        {
            currentSection = SECTION_FLEX;
            continue;
        }
        // Check for certain strings, depending on section: /////
        switch (currentSection)
        {
        case SECTION_SETUP:
            if (defLine == partNoFull) flagSetupOK = true;
            break;
        case SECTION_PINS:
            break;  // Don't care about anything in this section.
        case SECTION_MODES:
            if ( defLine.startsWith(QString("VALUE"), Qt::CaseSensitive) )
            {
                QStringList parts = defLine.split("=");
                if (parts.count() > 1)
                {
                    uint32_t readValue = static_cast<uint32_t>(parts.at(1).toLong());
                    foundRegAddress = static_cast<uint8_t>(readValue >> 16);
                    foundRegValue   = static_cast<uint16_t>(readValue & 0xFFFF);
                }
                else
                {
                    qDebug() << "LMX2594: Parse TCS Files: Parse ERROR: Error parsing register value line: " << defLine;
                }

                if (foundRegAddress < REGISTER_COUNT) profileToFill.setRegisterValue(foundRegAddress, foundRegValue);
                else                                  qDebug() << "LMX2594: Parse TCS Files: IGNORED Invalid register address: " << static_cast<int>(foundRegAddress);


            }
            break;
        case SECTION_FLEX:
            // Check for "FoutA_FREQ=xxxxxx" to get frequency for this profile:
            if ( defLine.startsWith(QString("FoutA_FREQ="), Qt::CaseSensitive) )
            {
                QStringList parts = defLine.split("=");
                if (parts.count() > 1)
                {
                    float frequency = parts.at(1).toFloat();
                    profileToFill.setFrequency(frequency);
                    flagFrequencyOK = true;
                    DEBUG_LMX_PROFILES("Freq for this profile: Str: " << parts.at(1) << "; Float: " << frequency << " MHz")
                }
            }
            break;
        case SECTION_UNKNOWN:
            // Unknown section. Ignore contents.
            break;
        }  // switch (currentSection)
    }  // foreach( QString defLineFull, freqFiles )...

    // For this to be a valid register profile, certain sections must have
    // been found, including at least one register value:
    if (   flagSetupOK
        && flagFrequencyOK
        && (profileToFill.getUsedRegisterCount() > 0) )
    {
        profileToFill.setValid();
    }
    else
    {
        qDebug() << "LMX2594: Parse TCS Files: Parse ERROR: Invalid frequency profile file: missing part number, register settings or frequency";
    }

    DEBUG_LMX_PROFILES("Register profile parsing finished. Valid? " << profileToFill.isValid() << "; "
                        << profileToFill.getUsedRegisterCount() << " register values found.")
    return globals::OK;
}






/*!
 \brief Get clock synth frequency for a given profile
 This can be used after init Frequency Profiles, to return the
 frequency of a specific profile. Nb: Generally used in
 conjunction with getFrequencyList; i.e. a combo box in the
 UI is populated with the frequency list, and when an item is
 selected, getFrequency is used to find the frequency of the
 selected item.
 \param index               Index of profile to get frequency from - 0 is first
 \param frequency           Pointer to a float; set to the frequency on success (MHz)
 \return globals::OK        Item found for requested index.
 \return globals::OVERFLOW  Index was larger than the list of frequency profiles
*/
int LMX2594::getFrequency(int index, float *frequency) const
{
    Q_ASSERT(index < (size_t)frequencyProfiles.count());
    if (index >= frequencyProfiles.count()) return globals::OVERFLOW;
    if (frequency) *frequency = frequencyProfiles.at(index).getFrequency();
    return globals::OK;
}


/*!
 \brief Select Frequency Profile by Index
 \param index               Index of profile to get frequency from - 0 is first
 \return globals::OK        Item found for requested index.
 \return globals::OVERFLOW  Index was larger than the list of frequency profiles
 \return [Error Code]       Error from derived class implementation (selectProfilePart)
*/
int LMX2594::selectProfile(int index)
{
    Q_ASSERT(index < (size_t)frequencyProfiles.count());
    if (index >= frequencyProfiles.count()) return globals::OVERFLOW;
    DEBUG_LMX("LMX2594: Select frequency profile " << index << ": " << frequencyProfiles.at(index).getFrequency() << " MHz")

    uint8_t registerAddress;
    uint16_t registerValue;
    bool registerFound;
    int result;

    resetDevice();

    // Load registers in REVERSE ORDER; Don't load R0 (power control)
    for (registerAddress = 112; registerAddress > 0; registerAddress--)
    {
        registerValue = frequencyProfiles.at(index).getRegisterValue(registerAddress, &registerFound);
        if (registerFound)
        {
            // DEBUG_LMX("LMX2594: Write " << QString().sprintf("0x%04X", registerValue) << " to register " << registerAddress;
            result = writeRegister(registerAddress, registerValue);
            if (result != globals::OK)
            {
                DEBUG_LMX("LMX2594: ERROR writing register! (" << result << ")")
                return globals::WRITE_ERROR;
            }
        }
    }
    selectedProfileIndex = static_cast<uint16_t>(index);

    // Restore the previous power and divider settings
    // and turn outputs on:
    flagOutputsOn = true;
    configureOutputs();

    // Calibrate: Required after changing PLL settings
    runFCal();

    DEBUG_LMX("LMX2594: Registers set for profile!")
    return globals::OK;
}


/* DEPRECATED; To do if resurrecting: SWAP power settings (Trig and main RF outs have been swapped)
 * Output divide is now fixed (see setSafeDefaults)
 *
 \brief Select Trigger Divide Ratio
 \param index              Index of new divide ratio (see getTriggerDivideList)
 \return globals::OK       Ratio set OK
 \return globals::OVERFLOW Invalid index
 \return [Error Code]      Error code from selectTriggerDividePart (derived class)
*
int LMX2594::selectTriggerDivide(const size_t index, const bool doFCal)
{
    Q_ASSERT(index < R31R75_VALUES_SIZE);
    if (index >= R31R75_VALUES_SIZE) return globals::OVERFLOW;
    DEBUG_LMX("LMX2594: Select trigger divide index " << index)
    uint16_t R45;
    int result;
    if (index == 0)
    {
        DEBUG_LMX("Selecting VCO for OUTA (No divider)")
        R45 = (R45_OUTA_VCO | POWER_CONSTS[selectedTrigOutputPowerIndex]);  // OUTA_MUX = 01 (VCO); OUT_ISET = 11; Set OUTB_POW;
        DEBUG_LMX("Set R45 to " << QString().sprintf("0x%04X", R45))
        result = writeRegister(45, R45);
        if (result != globals::OK) return result;
        DEBUG_LMX("OK.")
        // Don't care about CHDIV settings (R31 / R75).
    }
    else
    {
        DEBUG_LMX("Selecting CHDIV Out for OUTA")
        R45 = (R45_DEFAULT | POWER_CONSTS[selectedTrigOutputPowerIndex]);  // OUTA_MUX = 00; OUT_ISET = 11; Set OUTB_POW;
        DEBUG_LMX("Set R45 to " << QString().sprintf("0x%04X", R45))
        result = writeRegister(45, R45);
        if (result != globals::OK) return result;
        DEBUG_LMX("OK.")
        DEBUG_LMX("R31 = " << QString().sprintf("0x%04X", R31_VALUES[index]) << "; R75 = " << QString().sprintf("0x%04X", R75_VALUES[index]))
        result                            = writeRegister(31, R31_VALUES[index]);
        if (result == globals::OK) result = writeRegister(75, R75_VALUES[index]);
    }
    if (result == globals::OK && doFCal) runFCal();
    if (result != globals::OK)
    {
        DEBUG_LMX("LMX2594: Error setting divide ratio: " << result)
    }
    else
    {
        selectedTrigDivideIndex = index;
    }
    return result;
}
*/


/*!
 \brief Run frequency calibration
 \return globals::OK       Calibration OK
 \return [Error Code]      Error code from writeRegister
*/
int LMX2594::runFCal()
{
    DEBUG_LMX("LMX: Run FCAL...")
    // Need to preserve FCAL_HPFD_ADJ and FCAL_LPFD_ADJ bits from
    // profile (these are fields in R0):
    bool bFound = false;
    uint16_t R0 = 0;
    if (selectedProfileIndex < frequencyProfiles.count())
    {
        R0 = frequencyProfiles.at(selectedProfileIndex).getRegisterValue(0, &bFound);
    }
    if (!bFound) R0 = R0_DEFAULT;

    // Clear the FCAL_EN bit:
    uint16_t tempR0 = R0 & ~R0_FCAL_EN;

    int result;
    result = writeRegister(0, tempR0);  // FCCAL_EN low.
    if (result != globals::OK) return result;
    globals::sleep(100);

    result = writeRegister(0, R0);  // Restore R0 (Should set FCAL_EN)
    globals::sleep(100);
    return result;
}



/*!
 \brief Reset the LMX device
 \return globals::OK       Calibration OK
 \return [Error Code]      Error code from resetPart (derived class)
*/
int LMX2594::resetDevice()
{
    DEBUG_LMX("LMX: RESET...")
    // Need to preserve FCAL_HPFD_ADJ and FCAL_LPFD_ADJ bits from
    // profile (these are fields in R0):
    bool bFound = false;
    uint16_t R0 = 0;
    if (selectedProfileIndex < frequencyProfiles.count())
    {
        R0 = frequencyProfiles.at(selectedProfileIndex).getRegisterValue(0, &bFound);
    }
    if (!bFound) R0 = R0_DEFAULT;  // Default to use if no profile selected.

    return resetPart(R0);

}



/*!
 \brief Initialise I2C -> SPI Adaptor
 We expect there to be an SC18IS602B I2C->SPI adaptor available
 on the comms port represented by bertComms object.
 This method configures the adaptor to the correct mode:
   * MSB of data first
   * SPICLK LOW when idle; data clocked in on leading edge
   * 58 kHz SPI Clock Rate
  => Config word = 0x03
  See SC18IS602B docs for other options.

 \param comms       I2C Comms (created elsewhere)
 \param i2cAddress  I2C address to check on (7 bit, i.e. no R/W bit)

 \return globals::OK              Comms online; Ack from SC18IS602B
 \return globals::NOT_CONNECTED   Comms object is not connected to the instrument
 \return globals::GEN_ERROR       Read / Write error
 \return globals::TIMEOUT         Timeout waiting for data read/write or macro completion
 \return [Error code]             Error code from comms->read or comms->write
*/
int LMX2594::initAdaptor(I2CComms *comms, const uint8_t i2cAddress)
{
    int result;
    const uint8_t configData[] = { 0xF0, 0x03 };
    qDebug() << "Set up SC18IS602B I2C->SPI adaptor on I2C address " << static_cast<int>(i2cAddress);
    result = comms->writeRaw(i2cAddress, configData, 2);
    if (result == globals::OK) qDebug() << "SC18IS602B Ready.";
    else                       qDebug() << "Error: SC18IS602B didn't respond!";
    return result;
}



/*!
 \brief Initialise LMX2594 Chip

 This method resets the LMX2594, then loads initial register
 settings to place the device in a known state (prior to
 setting the frequency and enabling the output).
 Startup procedure is taken from LMX2594 documentation,
 Section 7.5.1

 This method assumes that initAdaptor() has already been
 called successfully (i.e. the I2C->SPI interface is ready).

 \return globals::OK   Comms online; Ack from SC18IS602B
 \return globals::NOT_CONNECTED   No connection to BERT
 \return [Error code]             ???
*/
int LMX2594::initPart()
{
    int result;
    DEBUG_LMX("initPart: Set up LMX2594 Clock Synth IC")

    // Step 1: Device RESET:
    result = resetPart(R0_DEFAULT);
    if (result != globals::OK) goto finished;

    // Step 2: Load default register values:

    // If frequency profiles were set up OK, load default profile:
    result = selectProfile(profileIndexDefault);
    if (result == globals::OK)
    {
        DEBUG_LMX("LMX: Ouptut drivers ON")
        flagOutputsOn = true;
        result = configureOutputs();
    }
    else
    {
        // Error occurrec selecting a default profile... possibly no frequency profiles set up!
        // Set safe defaults (Outputs off / minimum power):
        result                            = writeRegister(0,  R0_DEFAULT );
        if (result == globals::OK) result = writeRegister(44, R44_DEFAULT);
        if (result == globals::OK) result = writeRegister(45, R45_DEFAULT);
    }

    /* DEPRECATED
    if (result != globals::OK)
    {
        // Error... try loading failsafe default register settings:
        size_t initDataIndex;
        DEBUG_LMX(" -Set LMX2594 Registers to init values...")
        for (initDataIndex = 0; initDataIndex < INIT_REGISTERS_SIZE; initDataIndex++)
        {
            if (INIT_REGISTERS[initDataIndex].address == 0) continue;  // SKIP register R0.
            DEBUG_LMX("  -Set R" << INIT_REGISTERS[initDataIndex].address)
            result = writeRegister( INIT_REGISTERS[initDataIndex].address,
                                    INIT_REGISTERS[initDataIndex].value );
            if (result != globals::OK) break;
        }
    }
    */

  finished:
    if (result == globals::OK) DEBUG_LMX("LMX2594 Ready.")
    else                       DEBUG_LMX("ERROR setting up LMX2594!")
    return result;
}






/*!
 \brief Reset device: Part-specific implementation
 \param defaultR0  Value to use for R0. This should have any required bits set
                   EXCEPT the RESET bit which must be 0. The method will modify
                   the RESET bit and use other bits from defaultR0.
 \return [code] Result code from writeRegister
*/
int LMX2594::resetPart(uint16_t defaultR0)
{
    DEBUG_LMX("LMX2594 RESET...")
    int result = writeRegister(0, defaultR0 | R0_RESET);  // Reset bit = High. Nb: DOESN'T Self-clear on the LMX2594!
    if (result != globals::OK) return result;
    globals::sleep(LMX2594_RESET_SLEEP);

    result = writeRegister(0, defaultR0);                 // Reset bit = Low.
    globals::sleep(LMX2594_RESET_POST_SLEEP);
    DEBUG_LMX(" -OK.")
    return result;
}




int LMX2594::configureOutputs()
{
    uint16_t R44 = 0, R45 = 0;

    // We need to use MASH_RESET_EN and MASH_ORDER fields from profile for Reg 44:
    bool bFound44 = false;
    bool bFound45 = false;

    if (selectedProfileIndex < frequencyProfiles.count())
    {
        R44 = frequencyProfiles.at(selectedProfileIndex).getRegisterValue(44, &bFound44);
        R45 = frequencyProfiles.at(selectedProfileIndex).getRegisterValue(45, &bFound45);
    }
    if (!bFound44) R44 = R44_DEFAULT;  // Default to use if no profile selected.
    if (!bFound45) R45 = R45_DEFAULT;  // Default to use if no profile selected.


    DEBUG_LMX("RF Pow Index: " << selectedFOutOutputPowerIndex << " = " << POWER_CONSTS[selectedFOutOutputPowerIndex])

    R44 = (R44 & 0xC03F) |  // Mask: OUTA_POW, OUTA_PD and OUTB_PD bits set to 0
          (POWER_CONSTS[selectedFOutOutputPowerIndex] << 8) | // OUTA_POW
          (((flagOutputsOn) ? 0x00 : 0x03) << 6);             // OUTA_PD and OUTB_PD

    R45 = (R45 & 0xFFC0) |  // Mask: OUTB_PWR set to 0; other bits preserved
           POWER_CONSTS[selectedTrigOutputPowerIndex];  // Set OUTB_POW

    DEBUG_LMX("Set R44 to " << R44 << "; R45 to " << R45)
    int result                        = writeRegister(44, R44);
    if (result == globals::OK) result = writeRegister(45, R45);
    DEBUG_LMX("Result: " << result)
    return result;

}




/*!
 \brief Set safe default values for frequency profiles
 This method overrides some register values in each
 profile, to create safe default settings regardless
 of what settings were loaded from the frequency profile
 file.
 The following settings are changed:
  - Set up R0, R1 and R7 (OUT_MUTE, MUXOUT_LD_SEL, etc)
  - Power down output drivers (application should turn
    them ON once all settings have been configured)
  - Set output levels to 0 dBm
  - Set CHDIV ratio to default
  - Set up output MUX to select CHDIV for output A
    and VCO for output B
 \return globals::OK
 \return [error code]
*/
void LMX2594::setSafeDefaults()
{
    QList<LMXFrequencyProfile>::iterator i;
    for (i = frequencyProfiles.begin(); i != frequencyProfiles.end(); ++i)
    {
        DEBUG_LMX("Set SAFE defaults for " << i->getFrequency())

        // ---- Register R0 set up: ---------------------------------
        // * First, extract FCAL_HPFD_ADJ and FCAL_LPFD_ADJ from
        //   value recovered from file: We need to preserve these.
        // * Override some fields:
        //   * Lock Detect enabled;
        //   * MUXOUT pin set to LD;
        //   * FCAL_EN, RESET and POWERDOWN to 0.
        bool bFound;
        uint16_t R0 = i->getRegisterValue(0, &bFound);
            // Get R0 that was specified in register defs...
        if (!bFound) R0 = R0_DEFAULT;
            // Safe fallback if not found.
        uint16_t maskedR0 = R0 & R0_FCAL_XXXX_ADJ_MASK;
            // ...Leave only FCAL_HPFD_ADJ and FCAL_LPFD_ADJ
        R0 = maskedR0 | R0_DEFAULT;
            // ...Set the other bits we want from default (including FCAL_EN on by default!
        i->setRegisterValue(0, R0);

        // ---- Register R1 and R7 set up: -------------------------
        // Sets CAL_CLK_DIV for 100 MHz input oscillator;
        // Set OUT_FORCE to ON.
        i->setRegisterValue(1, R1_DEFAULT);
        i->setRegisterValue(7, R7_DEFAULT);

        // ---- Set up outputs: ------------------------------------
        // Set default trigger divide ratio (actually, we will be using VCO out):
        // R31: CHDIV_DIV2  } Used to set output divide ratio: Effective ONLY if OUTA_MUX
        // R75: CHDIV       } and/or OUTB_MUX set to 0  to select channel divider output
        // 28-Oct-2018: Disabled: Channel Divide will be controlled by whatever is in register def files
        // i->setRegisterValue(31, R31_DEFAULT);
        // i->setRegisterValue(75, R75_DEFAULT);


        // R44:
        //  OUTA_PWR
        //  OUTB_PD
        //  OUTA_PD
        //  MASH_RESET_N
        //  MASH_ORDER
        // * Preserve MASH_EN and MASH_ORDER bits
        // * Set other bits to safe defaults:
        //   OUT*_POW = 0dBm; OUT*_PD = 1
        // These settings use R44, R45 and R46 on LMX2594:
        uint16_t R44 = i->getRegisterValue(44, &bFound);
        if (!bFound) R44 = R44_DEFAULT;
        uint16_t maskedR44 = R44 & 0x0027;
          // ...Leave only MASH_RESET_EN and MASH_ORDER bits
        R44 = maskedR44 | 0x00C0;
          // ...Set OUTA_PD and OUTB_PD (Power down both outputs)
        R44 = setRegisterBits(R44, 6, 8, POWER_0DBM); // Set OUTA_POW = 0dBm:
        i->setRegisterValue(44, R44);

        // R45:           R46:
        //  OUTA_MUX       OUTB_MUX
        //  OUT_ISET
        //  OUTB_PWR
        // 28-Oct-2018: Disabled: Channel Divide will be controlled by whatever is in register def files
        // i->setRegisterValue(45, R45_DEFAULT);
        // i->setRegisterValue(46, R46_DEFAULT);
    }
}



/*************************************************************************************/
/* LMX Enable / Disable:                                                             */
/*************************************************************************************/

/*!
 \brief Enable or Disable LMX2594
 Assumes that the LMX is connected via the SC18IS602B I2C-SPI bridge,
 and that Pin !SS3/GPIO3 on the SC18IS602B is connected to "EN" (Enable) on
 the GT1724.

 !SS3 is HIGH by power-on default, thus the GT1724 is usually enabled.

 It is disabled below by pulling !SS3 low (i.e. setting correcponding bit in
 the SC18IS602B Function ID byte).

 \param enabled  New status for the GT1724

 \return globals::OK               Success... Register data written
 \return globals::NOT_INITIALISED  Clock interface hasn't been initialised yet
 \return globals::OVERFLOW         Invalid register address
 \return [error code]              Comms or adaptor error writing to LMX
*/
int LMX2594::setLMXEnable(bool enabled)
{
    if (!spiAdaptorIsOpen) return globals::NOT_INITIALISED;
    int result;

    // Configure SC18IS602B pin GPIO3 / !SS3 as GPIO:
    // By default, GPIO pins are configured with weak pull up and strong pull down, which should be OK here.
    const uint8_t gpioEnData[] = { 0xF6,                     // GPIO Enable
                                   0x08 };                   // Enable GPIO3
     result = comms->writeRaw(i2cAddress, gpioEnData, 2);
#ifdef LMX_DEBUG
     DEBUG_LMX("[SC18IS602B GPIO EN for LMX]")
#endif
    if (result != globals::OK)
     {
         qDebug() << "[SC18IS602B for LMX] Error setting up GPIO: " << result;
         return result;
     }

    // GPIO3: To enable LMX, we want to set GPIO 3 high; To disable, set Low.
    uint8_t gpioPins;
    if (enabled) gpioPins = 0x08;   // 1000 = Set GPIO 3
    else         gpioPins = 0x00;   // 0000 = Clear GPIO 3

    // Set GPIO Pins on SC18IS602B: Instruction 0xF4
    const uint8_t gpioData[] = { 0xF4,           // GPIO Write
                                 gpioPins };     // Set the GPIO pins to 1100 (set GPIO 2 and 3).

     result = comms->writeRaw(i2cAddress, gpioData, 4);
#ifdef LMX_DEBUG
     DEBUG_LMX("[SC18IS602B GPIO SET for LMX] Pins: " << QString().sprintf("0x%02X", gpioData)
#endif
     return result;
}



/*************************************************************************************/
/*  LMX Register Write Fns                                                           */
/*************************************************************************************/



/*!
 \brief Write value to LMX register
 \param regAddress  Register number to write to
 \param regValue    Value to write

 \return globals::OK               Success... Register data written
 \return globals::NOT_INITIALISED  Clock interface hasn't been initialised yet
 \return globals::OVERFLOW         Invalid register address
 \return [error code]              Comms or adaptor error writing to LMX
*/
int LMX2594::writeRegister(const uint8_t regAddress, const uint16_t regValue)
{
    if (!spiAdaptorIsOpen) return globals::NOT_INITIALISED;
    Q_ASSERT(regAddress < REGISTER_COUNT);
    if (regAddress >= REGISTER_COUNT) return globals::OVERFLOW;

    // Register Write: Write to the SC18IS602B I2C-SPI bridge, using a register address
    // with uppermost bit (R/W) set to 0. Data bytes are sent after the register address.
    const uint8_t rawData[] = { 0x04,                     // Function ID: Selects the !SS pin to use
                                regAddress,               // Register Address byte
                                static_cast<uint8_t>(regValue >> 8),  // Upper byte of value
                                static_cast<uint8_t>(regValue) };     // Lower byte of value

     int result = comms->writeRaw(i2cAddress, rawData, 4);
#ifdef LMX_REGISTER_DEBUG
     DEBUG_LMX("[LMX Register WRITE] Addr: " << regAddress << " Data: " << QString().sprintf("0x%04X", regValue) << " Result: " << result)
#endif
     return result;
}



/*!
 \brief Set specific bits in a register value, leaving other bits unchanged

 \param registerInputValue Initial value of register
 \param nBits              Number of bits to set within the register
 \param shift              Number of bits to shift the value left
 \param value              Value to set for selected bits

 \return value  Updated register value
*/
uint16_t LMX2594::setRegisterBits(const uint16_t registerInputValue,
                                  const uint8_t  nBits,
                                  const uint8_t  shift,
                                  const uint16_t value)
{
    uint16_t maskIn = static_cast<uint16_t>((0xFFFF >> (16-nBits)) << shift);  // Mask with 1 in the bits we want to set
    uint16_t maskOut = ~maskIn;                         // Mask with 0 in the bits we want to set

    uint16_t registerValue;
    registerValue = registerInputValue & maskOut;  // Zero existing bits in selected field
    registerValue = registerValue | static_cast<uint16_t>(value << shift);  // Set bits
    return registerValue;
}




/*!
 \brief Write specific bits of one register

 Uses setRegisterBits to set only specific bits of a register
 then writes to that register of the LMX.

 This method tries to find the CURRENT value of the register
 using the current profile selection. A bitwise AND/OR is then
 used to set ONLY the bits specified, and the value is written
 to the register.

 If the current profile is invalid or profiles have not been
 loaded, registerDefaultValue is used as the initial value for the
 register.

 See setRegisterBits for more info.

 \param address  Register to set (0 to n)
 \param nBits  Number of bits to set within the register
 \param shift  Number of bits to shift the value left
 \param value  Value to set
 \param registerDefaultValue
               Default initial value for register if no
               profile is selected

 \return globals::OK
 \return [Error code] - See writeRegister errors
*/
int LMX2594::writeRegisterBits(const uint8_t  address,
                               const uint8_t  nBits,
                               const uint8_t  shift,
                               const uint16_t value,
                               const uint16_t registerDefaultValue)
{
    uint16_t registerOldValue = 0;
    uint16_t registerValue;
    bool bFound = false;
    // Get current setting for register (only want to change some bits):
    if (selectedProfileIndex < frequencyProfiles.count())
    {
        registerOldValue = frequencyProfiles.at(selectedProfileIndex).getRegisterValue(address, &bFound);
    }
    if (!bFound) registerOldValue = registerDefaultValue;  // Default to use if no profile selected.

    registerValue = setRegisterBits(registerOldValue,
                                    nBits,
                                    shift,
                                    value);

    return writeRegister(address, registerValue);
}


/*!
 \brief Read a Register from the LMX2594
 \param regAddress   Address of register to read. This is the register address as
                     printed in the manual, i.e. < 128, R/W bit NOT included.
 \param regValue     Pointer used to return the register value
 \return globals::OK               Success
 \return globals::NOT_INITIALISED  Clock interface hasn't been initialised yet
 \return globals::OVERFLOW         Invalid register address
 \return [error code]              Comms or adaptor error writing to LMX
*/
int LMX2594::readRegister(const uint8_t regAddress, uint16_t *regValue)
{
    if (!spiAdaptorIsOpen) return globals::NOT_INITIALISED;
    Q_ASSERT(regAddress < REGISTER_COUNT);
    if (regAddress >= REGISTER_COUNT) return globals::OVERFLOW;

#ifdef LD_LED_ENABLE
    // MUXout pin is set up as Lock Detect output; Can't read back register values!
    Q_ASSERT(false);
    Q_UNUSED(regValue);
    return globals::NOT_IMPLEMENTED;
#else

    // Readback Step 1: Write to the SC18IS602B I2C-SPI bridge, using a register address
    // with uppermost bit (R/W) set to 1. Address is followed by two placeholder bytes.
    // While the placeholders are clocked OUT, the LMX2594 will be sending the register
    // data bytes on the MISO pin. SC18IS602B will hold them in its buffer.
    uint8_t regAddFull = regAddress | 0x80;    // Register address with R/W bit set to 1 (upper bit of address)
    const uint8_t rawData[] = { 0x04,          // Function ID: Selects the !SS pin to use
                                regAddFull,    // Register Address byte
                                0x00,          // Placeholder: Empty byte (not sure why this is needed...)
                                0x00,          // Placeholder: Upper byte of data will be received while this is clocked out.
                                0x00 };        // Placeholder: Lower byte of data will be received while this is clocked out.

    int result = comms->writeRaw(i2cAddress, rawData, 5);
    if (result != globals::OK)
    {
        DEBUG_LMX("ERROR during LMX register readback step 1: " << result)
        return result;  // Error!?
    }

    // Readback Step 2: Issue an I2C raw read (no reg address) for the SC18IS602B
    // I2C-SPI bridge, which should return the two bytes of data.
    uint8_t registerData[3] = { 0x00, 0x00, 0x00};
    result = comms->readRaw(i2cAddress, registerData, 3);
    if (result != globals::OK)
    {
        DEBUG_LMX("ERROR during LMX register readback step 2: " << result)
        return result;  // Error!?
    }
    *regValue = static_cast<uint16_t>((registerData[1] << 8) | registerData[2]);

  #ifdef LMX_REGISTER_DEBUG
     DEBUG_LMX("[LMX Register READ] Addr: " << regAddress << " Data: " << INT_AS_HEX(*regValue,4) << " Result: " << result)
  #endif
    return result;
#endif

}










