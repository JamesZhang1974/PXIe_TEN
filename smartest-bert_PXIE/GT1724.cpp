/*!
 \file   GT1724.cpp
 \brief  Functional commands to control a Gennum GT1724 PRBS Generator / Checker - Implementation

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <QThread>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <cmath>

#include "EyeMonitor.h"

#include "GT1724.h"
#include "BertModel.h"

// Debug Macro for Register Read / Write:
//#define BERT_REGISTER_DEBUG
#ifdef BERT_REGISTER_DEBUG
  #define DEBUG_REG(MSG) qDebug() << "\t\t\t" << MSG;
#endif
#ifndef DEBUG_REG
  #define DEBUG_REG(MSG)
#endif

// Debug macro for general debug mesages from GT1724
#define DEBUG_GT1724(MSG) qDebug() << "\t\t" << MSG;



// Macro: Display a message in the debug log, emit error result, and return, if result code is NOT globals::OK
#define RETURN_ON_ERROR(MSG)                           \
    if (result != globals::OK)                         \
    {                                                  \
        qDebug() << MSG << " (Err: " << result << ")"; \
        return result;                                 \
    }


// Macro: Filter a slot call by lane: If it's for "ALL_LANES", use our first lane
// (laneOffset); if it's not for one of our lanes, ignore.
#define LANE_FILTER(LANE) \
    if (LANE == globals::ALL_LANES) LANE = laneOffset;      \
    if (LANE < laneOffset || LANE > (laneOffset+3)) return;


// Mod a lane ID with 4 to convert it to the range 0 - 3 (remove lane offset...).
#define LANE_MOD(LANE) (LANE % 4)


GT1724::GT1724(I2CComms *comms, const uint8_t i2cAddress, const uint8_t laneOffset)
 : comms(comms), i2cAddress(i2cAddress), laneOffset(laneOffset), coreNumber(laneOffset/4 + 1)
{
    ed01.edRunTime = new QTime();
    ed23.edRunTime = new QTime();

    eyeMonitor01 = new EyeMonitor(this, laneOffset, 1);  // Create eye monitor instances for
    eyeMonitor23 = new EyeMonitor(this, laneOffset, 3);  // each ED input
}


GT1724::~GT1724()
{
    delete ed01.edRunTime;
    delete ed23.edRunTime;

    delete eyeMonitor01;
    delete eyeMonitor23;
}


/*!
 \brief Get Hardware Options for GT1724
        Requests that this module emit signals describing its available options lists to client
*/
void GT1724::getOptions()
{
    // Inform the client about available config options / select lists for the GT1724:
    // PG Lane Lists: PG output lanes are 0 and 2 for each chip,
    // so we send each list twice (onece for each lane).
    emit ListPopulate("listPGAmplitude", laneOffset + 0, PG_OUTPUT_SWING_LIST, 0);
    emit ListPopulate("listPGAmplitude", laneOffset + 2, PG_OUTPUT_SWING_LIST, 0);

    emit ListPopulate("listPGPattern", laneOffset + 0, PG_PATTERN_LIST, 0);
    emit ListPopulate("listPGPattern", laneOffset + 2, PG_PATTERN_LIST, 0);

    emit ListPopulate("listPGDeemphLevel", laneOffset + 0, PG_EQ_DEEMPH_LIST, 0);
    emit ListPopulate("listPGDeemphLevel", laneOffset + 2, PG_EQ_DEEMPH_LIST, 0);

    emit ListPopulate("listPGDeemphCursor", laneOffset + 0, PG_EQ_CURSOR_LIST, 0);
    emit ListPopulate("listPGDeemphCursor", laneOffset + 2, PG_EQ_CURSOR_LIST, 0);

    emit ListPopulate("listPGCrossPoint", laneOffset + 0, PG_CROSS_POINT_LIST, 0);
    emit ListPopulate("listPGCrossPoint", laneOffset + 2, PG_CROSS_POINT_LIST, 0);

    emit ListPopulate("listPGCDRBypass", laneOffset + 0, CDR_BYPASS_OPTIONS_LIST, CDR_BYPASS_OPTIONS_DEFAULT);
    emit ListPopulate("listPGCDRBypass", laneOffset + 2, CDR_BYPASS_OPTIONS_LIST, CDR_BYPASS_OPTIONS_DEFAULT);

    emit ListPopulate("listCDRDivideRatio", coreNumber-1, CDR_FREQDIV_OPTIONS_LIST, CDR_FREQDIV_OPTIONS_DEFAULT);

    if (BertModel::UseFourChanPGMode())
    {
        // Four PG channels per Core: No ED options!
        emit ListPopulate("listPGAmplitude", laneOffset + 1, PG_OUTPUT_SWING_LIST, 0);
        emit ListPopulate("listPGAmplitude", laneOffset + 3, PG_OUTPUT_SWING_LIST, 0);

        emit ListPopulate("listPGPattern", laneOffset + 1, PG_PATTERN_LIST, 0);
        emit ListPopulate("listPGPattern", laneOffset + 3, PG_PATTERN_LIST, 0);

        emit ListPopulate("listPGDeemphLevel", laneOffset + 1, PG_EQ_DEEMPH_LIST, 0);
        emit ListPopulate("listPGDeemphLevel", laneOffset + 3, PG_EQ_DEEMPH_LIST, 0);

        emit ListPopulate("listPGDeemphCursor", laneOffset + 1, PG_EQ_CURSOR_LIST, 0);
        emit ListPopulate("listPGDeemphCursor", laneOffset + 3, PG_EQ_CURSOR_LIST, 0);

        emit ListPopulate("listPGCrossPoint", laneOffset + 1, PG_CROSS_POINT_LIST, 0);
        emit ListPopulate("listPGCrossPoint", laneOffset + 3, PG_CROSS_POINT_LIST, 0);

        emit ListPopulate("listPGCDRBypass", laneOffset + 1, CDR_BYPASS_OPTIONS_LIST, CDR_BYPASS_OPTIONS_DEFAULT);
        emit ListPopulate("listPGCDRBypass", laneOffset + 3, CDR_BYPASS_OPTIONS_LIST, CDR_BYPASS_OPTIONS_DEFAULT);
    }
    else
    {
        emit ListPopulate("listEDPattern", laneOffset + 1, ED_PATTERN_LIST, ED_PATTERN_DEFAULT);
        emit ListPopulate("listEDPattern", laneOffset + 3, ED_PATTERN_LIST, ED_PATTERN_DEFAULT);

        emit ListPopulate("listEDEQBoost", laneOffset + 1, ED_EQ_BOOST_LIST, 0);
        emit ListPopulate("listEDEQBoost", laneOffset + 3, ED_EQ_BOOST_LIST, 0);

        emit ListPopulate("listEyeScanVStep", globals::ALL_LANES, EYESCAN_VHSTEP_LIST, EYESCAN_VHSTEP_DEFAULT);
        emit ListPopulate("listEyeScanHStep", globals::ALL_LANES, EYESCAN_VHSTEP_LIST, EYESCAN_VHSTEP_DEFAULT);
        emit ListPopulate("listEyeScanCountRes", globals::ALL_LANES, EYESCAN_COUNTRES_LIST, EYESCAN_COUNTRES_DEFAULT);
        emit ListPopulate("listBathtubCountRes", globals::ALL_LANES, EYESCAN_COUNTRES_LIST, EYESCAN_COUNTRES_DEFAULT);
        emit ListPopulate("listBathtubVOffset", globals::ALL_LANES, EYESCAN_VOFF_LIST, EYESCAN_VOFF_DEFAULT);
    }
}


/*!
 \brief GT1724 Initialisation
 \return globals::OK    Success!
 \return [error code]   Error connecting or downloading macros. Comms problem or invalid I2C address?
 SIGNALS:
  emits ShowMessage(...) to show progress messages to user
*/
int GT1724::init()
{
    qDebug() << "GT1724: Init for GT1724 at lane " << laneOffset << "; I2C Address " << INT_AS_HEX(i2cAddress,2);
    emit ShowMessage(QString("Configuring Instrument (Core %1)...").arg(coreNumber));
    int result;
    Q_ASSERT(comms->portIsOpen());

    // Check whether the extension macros have been loaded yet:
    qDebug() << "GT1724: Checking Macro Version...";
    result = macroCheck(laneOffset);
    qDebug() << "GT1724: Macro Check Result: " << result;
    if (result == globals::MACRO_ERROR)
    {
        qDebug() << "GT1724: Error reading macro version from board. Comms error?";
        emit ShowMessage("Comms Error!");
        return globals::GEN_ERROR;
    }

    // Make sure CDR Bypass is set to default at each boot (default should be OFF!)
    forceCDRBypass0 = CDR_BYPASS_OPTIONS_DEFAULT;
    forceCDRBypass1 = CDR_BYPASS_OPTIONS_DEFAULT;
    forceCDRBypass2 = CDR_BYPASS_OPTIONS_DEFAULT;
    forceCDRBypass3 = CDR_BYPASS_OPTIONS_DEFAULT;

    if (result == globals::MACROS_LOADED)
    {
        qDebug() << "GT1724: Extension macros already loaded. Connected OK!";
        goto WarmBoot;  // Don't need to load default config (warm boot!). Fall out.
    }

    result = downloadHexFile();
    if (result != globals::OK)
    {
        if (result == globals::FILE_ERROR) emit ShowMessage("Couldn't read utilities macro file!");
        else                               emit ShowMessage("Couldn't download utilities macro file!");
        return globals::GEN_ERROR;
    }

    // Run the macro check again to verify the macro download:
    result = macroCheck(laneOffset);
    qDebug() << "GT1724: Macro Recheck Result: " << result;
    if (result != globals::MACROS_LOADED)
    {
        qDebug() << "GT1724: Error downloading extension macros!";
        emit ShowMessage("Error downloading macros!");
        return globals::GEN_ERROR;
    }

    // Cold Boot: Set default settings:
    emit ShowMessage(QString("Configuring Instrument (Core %1)...").arg(coreNumber));
    qDebug() << "GT1724: Cold Boot. Setting defaults for GT1724 at lane " << laneOffset;
    configSetDefaults(25e9);  // Nb: Bitrate shouldn't matter here because CDR Bypass defaults to OFF.

  WarmBoot:

    // Get current instrument config, and send the info to clients:
    int pattern;
    getCurrentSettings(&pattern);

    // Force reconfigure of PG to make sure it's synced OK.
    qDebug() << "GT1724: Force resync of GT1724 at lane " << laneOffset;
    configPG(pattern, 25e9);   // Don't actually know bit rate here! Shouldn't matter because CDR Bypass should default to OFF.

    return globals::OK;
}



/*!
 \brief Get Current Settings
        Reads current settings from the chip and emits signals to update the client
 \param pattern  Used to return the currently selected pattern index
 \return globals::OK  Settings read OK
 \return [error code] Error occurred. Not all settings read
*/
int GT1724::getCurrentSettings(int *pattern)
{
    int result = globals::OK;
    *pattern = 0;
    DEBUG_GT1724("GT1724: Getting current config for GT1724 at lane " << laneOffset)

    if (result == globals::OK) result = getOutputSwings();          // Query output swings for PG lanes
    if (result == globals::OK) result = getPRBSPattern(pattern);    // Query PRBS pattern for all lanes

    if (result == globals::OK) result = getDeEmphasis(laneOffset + 0);  //
    if (result == globals::OK) result = getDeEmphasis(laneOffset + 2);  // Query De-Emphasis and Cross Point settings for PG lanes
    if (result == globals::OK) result = getCrossPoint(laneOffset + 0);  //
    if (result == globals::OK) result = getCrossPoint(laneOffset + 2);  //

    if (result == globals::OK) result = getForceCDRBypass(laneOffset + 0);  // Query 'Force CDR Bypass' settings
    if (result == globals::OK) result = getForceCDRBypass(laneOffset + 2);  //

    if (BertModel::UseFourChanPGMode())
    {
        if (result == globals::OK) result = getDeEmphasis(laneOffset + 1);  //
        if (result == globals::OK) result = getDeEmphasis(laneOffset + 3);  // Query De-Emphasis and Cross Point settings for PG lanes
        if (result == globals::OK) result = getCrossPoint(laneOffset + 1);  //
        if (result == globals::OK) result = getCrossPoint(laneOffset + 3);  //
        if (result == globals::OK) result = getForceCDRBypass(laneOffset + 1);  // Query 'Force CDR Bypass' settings
        if (result == globals::OK) result = getForceCDRBypass(laneOffset + 3);  //
    }
    else
    {
        if (result == globals::OK) result = getEQBoost(laneOffset + 1);  // Query EQ Boost setting (ED page)
        if (result == globals::OK) result = getEQBoost(laneOffset + 3);  //
    }

    if (result != globals::OK) return result;

    // The following methods don't check for errors so any errors will be ignored.
    getLaneInverted(laneOffset + 0);  // Query "inverted" option for PG lanes
    getLaneInverted(laneOffset + 2);  //

    getLaneOn(laneOffset + 0);  // 2 PG output lanes for each GT1724 (0 and 2)...
    getLaneOn(laneOffset + 2);  // Query On / Off (=Mute) status

    if (BertModel::UseFourChanPGMode())
    {
        getLaneInverted(laneOffset + 1);
        getLaneInverted(laneOffset + 3);

        getLaneOn(laneOffset + 1);
        getLaneOn(laneOffset + 3);
    }

    return globals::OK;
}




/*!
 \brief Static method to look for a GT1724 chip on the specified I2C address
        This is implemented by attempting to read the macro version from the IC.
 \param comms       Pointer to I2C Comms class to use (created elsewhere)
 \param i2cAddress  Address to check on (7 bit, i.e. not including R/W bit)
 \return true   Board responded OK
 \return false  Error trying to read macro version... no GT1724 on this address.
*/
bool GT1724::ping(I2CComms *comms, const uint8_t i2cAddress)
{
    qDebug() << "GT1724: Searching on address " << INT_AS_HEX(i2cAddress,2) << "...";
    Q_ASSERT(comms->portIsOpen());
    int result;

    // Use I2C adaptor 'I2C_TEST' function to check for any device on this address:
    result = comms->pingAddress(i2cAddress);
    if (result != globals::OK)
    {
        qDebug() << "GT1724 not found (no ACK on I2C address; result: " << result << ")";
        return false;
    }

    // Check that we have a GT1724 by reading the macro version:
    uint8_t resultData[4] = { 0 };
    result = runMacroStatic(comms, i2cAddress, 0x18, NULL, 0, resultData, 4, 800);  // Query macro version
    if (result == globals::OK)
    {
        qDebug() << "GT1724 compatible IC found.";
        return true;
    }
    else
    {
        qDebug() << "Not found.";
        return false;
    }
}




/*!
 \brief Comms Check - Read and write registers for testing comms

 SLOT!
 Emits: globals::OK   Success
        [ <  0]       Error setting register or running macro
        [ >= 0]       Number of errors during write / read tests. Should be 0.

*/
void GT1724::CommsCheck(int metaLane)
{
    LANE_FILTER(metaLane);
    qDebug() << "GT1724: Comms Check: Using lane " << metaLane;

    // Repeated Register Write / Read to register 1 (LOS threshold): 0 - 63
    uint8_t lane, i, baseValue, setValue;
    int result, countGood, countError;
    countGood = 0;
    countError = 0;

    // Get current register contents
    uint8_t initValue[4] = {0};
    for (lane = 0; lane < 4; lane++)
    {
        result = getRegister(lane, 1, &initValue[lane]);
        if (result == globals::OK)
        {
            qDebug() << QString(" Initial Read: L%1 R1 Got: %2: OK").arg(lane).arg(initValue[lane]);
        }
        else
        {
            qDebug() << " Error reading initial value: " << result;
            countError = result;
            goto finished;
        }
    }

    baseValue = (initValue[0] & 0xC0);  // Mask out top 2 bits (preserve)

    // CONTIGUOUS READ / WRITE Test: Read/write each lane in sequence:
    for (lane = 0; lane < 4; lane++)
    {
        for (i = 0; i < 64; i++)
        {
            setValue = baseValue + i;
            commsCheckOneRegister(lane, setValue, countGood, countError);
        }
    }

  finished:

    qDebug() << "Test Finished. " << countGood << " OK; " << countError << " ERRORS";

    // Restore register values:
    for (lane = 0; lane < 4; lane++)
    {
        setRegister(lane, 1, initValue[lane]);
    }

    emit ShowMessage(QString("Comms Check Finished. %1 OK; %2 ERRORS.").arg(countGood).arg(countError));
    emit Result(countError, metaLane);
}
/*!
 \brief Comms Check: Write to and read back ONE register (uses register 1).
        Used by commsCheck for debugging / testing comms
 \param lane
 \param value
 \param countGood
 \param countError
 \return Error count, or result code on failure
*/
int GT1724::commsCheckOneRegister(int lane, uint8_t value, int &countGood, int &countError)
{
    uint8_t gotValue;
    int result;
    result = setRegister(lane, 1, value);
    if (result == globals::OK)
    {
        qDebug() << QString(" L%1 R1 Set: %2: OK").arg(lane).arg(value);
    }
    else
    {
        qDebug() << QString(" L%1 R1: Error setting register: %2").arg(lane).arg(result);
        countError++;
        return result;
    }
    result = getRegister(lane, 1, &gotValue);
    if (result == globals::OK)
    {
        if (gotValue == value)
        {
            qDebug() << QString(" L%1 R1 Get: %2: OK").arg(lane).arg(gotValue);
            countGood++;
        }
        else
        {
            qDebug() << QString(" L%1 R1 Get: %2: Unexpected value!").arg(lane).arg(gotValue);
            countError++;
        }
    }
    else
    {
        qDebug() << QString(" L%1 R1: Error reading back register: %2").arg(lane).arg(result);
        countError++;
    }
    return result;
}


/*!
 \brief Check whether the extension macros have been loaded
 Uses the 'Query Macro Version' macro, and checks that the
 output data is a known macro version.

 \return result  Returns the results of the macro check

 Possible results: globals::MACROS_LOADED      Macros have been loaded
                   globals::MACROS_NOT_LOADED  Macros not loaded yet.
                   globals::MACRO_ERROR        Error running macro - not connected?

 Emits:  UpdateString "MacroVersion" containing macro version string
*/
int GT1724::macroCheck(int metaLane)
{
    uint8_t resultData[4] = { 0 };
    int macroStatus = globals::MACROS_NOT_LOADED;  // Default status... Couldn't confirm that macros are loaded.

    int result = runMacro(0x18, NULL, 0, resultData, 4);  // Query macro version
    if (result != globals::OK)
    {
        DEBUG_GT1724("GT1724: Error running macro check  (" << result << ") - not connected?")
        return globals::MACRO_ERROR;
    }
    DEBUG_GT1724("GT1724: Macro version read OK")
        // Check macro version data:
        size_t macroIndex;
        for (macroIndex = 0; macroIndex < globals::N_MACRO_FILES; macroIndex++)
        {
            if (  (resultData[0] == globals::MACRO_FILES[macroIndex].macroVersion[0]) &&
                  (resultData[1] == globals::MACRO_FILES[macroIndex].macroVersion[1]) &&
                  (resultData[2] == globals::MACRO_FILES[macroIndex].macroVersion[2]) &&
                  (resultData[3] == globals::MACRO_FILES[macroIndex].macroVersion[3])  )
            {
                macroVersion = &(globals::MACRO_FILES[macroIndex]);
                macroStatus = globals::MACROS_LOADED;
                DEBUG_GT1724("GT1724: Found extension macros version " << macroVersion->macroVersionString)
                emit UpdateString("MacroVersion", metaLane, macroVersion->macroVersionString);
                break;
            }
        }
        if (macroStatus != globals::MACROS_LOADED)
        {
            // Couldn't match the macro version we read from the chip to a known macro version.
            DEBUG_GT1724("GT1724: Unknown extension macros version: "
                          << QString("[%1][%2][%3][%4]")
                             .arg(resultData[0], 2, 16, QChar('0'))
                             .arg(resultData[1], 2, 16, QChar('0'))
                             .arg(resultData[2], 2, 16, QChar('0'))
                             .arg(resultData[3], 2, 16, QChar('0')))
            DEBUG_GT1724("This probably means extension macros haven't been downloaded yet.")
            emit UpdateString("MacroVersion", metaLane, QString("Unknown"));
        }
    return macroStatus;
}
/*****************************************************************************************/




/*****************************************************************************
   BERT Commands
******************************************************************************/



/*!
 \brief Get IC Temperature from Gennum GTxxxx chip
 SLOT
  Emits Result    [globals::OK] or [error code], lane
  Emits UpdateString "CoreTemperature", lane, degrees (Success only)

 \param temperatureDegrees  Pointer to an int, used to return temperature in degrees C
 \return [Error Code]       Error from hardware/comms functions
*/
void GT1724::GetTemperature(int metaLane)
{
    LANE_FILTER(metaLane);

#ifdef BERT_SIGNALS_DEBUG
    DEBUG_GT1724("GT1724 [" << this << "]: Received Sig GetTemperature with lane " << metaLane << " on thread " << QThread::currentThreadId())
#endif

    uint8_t output[2] = { 0, 0 };
    int result;
    // Read the on-chip temperature sensor:
    int temperatureDegrees = 0;  // Default if read fails.
    result = runMacro(0x28, NULL, 0, output, 2);
    if (result != globals::OK)
    {
        emit Result(result, metaLane);
        return;
    }
    // Convert output data to degrees C:
    uint16_t tempRaw = ((uint16_t)output[0] << 8) | (uint16_t)output[1];
    if (tempRaw == 0)
    {
        emit Result(globals::READ_ERROR, metaLane);
        return;  // Improbable result!
    }
    temperatureDegrees = (int)( ((float)tempRaw * 0.415) - 277.565 );
#define BERT_EXTRA_DEBUG
#ifdef BERT_EXTRA_DEBUG
    DEBUG_GT1724("Read Chip Temperature OK for lane: " << metaLane)
    /* DEBUG
    qDebug() << QString("  Raw Data: [0x%1][0x%2]; Raw Int: %3; Deg C: %4")
                .arg(output[0],2,16,QChar('0'))
                .arg(output[1],2,16,QChar('0'))
                .arg(tempRaw)
                .arg(temperatureDegrees); */
#endif
    emit UpdateString(QString("CoreTemperature"), metaLane, QString("%1 ºC").arg(temperatureDegrees));
    emit Result(globals::OK, metaLane);
}




/*!
 \brief Load default settings - Run ONCE after connect

 This method sets various PG parameters, e.g. output swing and
 De-Emphasis. It also sets up the pattern generator with the
 default pattern and starts it (by calling config PG).
   SLOT!

 \param metaLane   Used to select the GT1724 core to configure; 0 = 1st, 4 = 2nd, etc.
 \param bitRate    Instrument bit rate (bits/sec); Used to select certain bitrate
                   dependant options when configuring the GT1724.
*/
void GT1724::ConfigSetDefaults(int metaLane, double bitRate)
{
    DEBUG_GT1724("GT1724: ConfigSetDefaults signal for lane " << metaLane)
    LANE_FILTER(metaLane);
    int result = configSetDefaults(bitRate);
    // Echo back the new settings to the client
    int pattern = 0;
    if (result== globals::OK) result = getCurrentSettings(&pattern);
    // Send the results:
    emit Result(result, metaLane);
}
// Implementation of ConfigSetDefaults
int GT1724::configSetDefaults(double bitRate)
{
    DEBUG_GT1724("GT1724: ConfigSetDefaults on Lane " << laneOffset)

    int result;

    // Make sure CDR Bypass is set to default (default should be OFF!)
    forceCDRBypass0 = CDR_BYPASS_OPTIONS_DEFAULT;
    forceCDRBypass1 = CDR_BYPASS_OPTIONS_DEFAULT;
    forceCDRBypass2 = CDR_BYPASS_OPTIONS_DEFAULT;
    forceCDRBypass3 = CDR_BYPASS_OPTIONS_DEFAULT;

    result = configPG(2, bitRate);                                 // --- Set up pattern generator (default pattern PRBS31)
    RETURN_ON_ERROR("GT1724: Config Set Defaults: PG may not be correctly set up!");

    uint8_t swingData[4] = { 0xA0, 0xA0, 0xA0, 0xA0 };             // --- Configure output driver main swing to 800 mV (all lanes)... Lane value = [lane mV] / 5; 0xA0 = 160d = 800 mV
    result = runMacro(0x61, swingData, 4, NULL, 0 );     //
    RETURN_ON_ERROR("GT1724: Config Set Defaults: Failed to set output swing");

    result                            = setEQBoost(0, 0);          // --- Set EQ boost to 0
    if (result == globals::OK) result = setEQBoost(2, 0);
    RETURN_ON_ERROR("GT1724: Config PG: Error setting EQ boost for lane 0 or 2");

    result                            = setDeEmphasis(0, 5, 0);    // --- Set default De-Emphasis to 1.9 dBm of Post-cursor
    if (result == globals::OK) result = setDeEmphasis(2, 5, 0);    //
    RETURN_ON_ERROR("GT1724: Config Set Defaults: Failed to set up de-emphasis");

    // Make sure PG output lanes are ON:
    result                            = setLaneOn(0, true, false);  // --- Enable / unmute outputs for PG lanes
    emit(SetPGLedStatus(0, true));                                  //Turn on PG1 LED
    if (result == globals::OK) result = setLaneOn(2, true, false);  //
    emit(SetPGLedStatus(2, true));                                  //Turn on PG3 LED
    RETURN_ON_ERROR("GT1724: Config Set Defaults: Failed to turn on outputs");

    // 4 PG channel mode:
    if (BertModel::UseFourChanPGMode())
    {
        result                            = setEQBoost(1, 0);          // --- Set EQ boost to 0
        if (result == globals::OK) result = setEQBoost(3, 0);
        RETURN_ON_ERROR("GT1724: Config PG: Error setting EQ boost for lane 1 or 3");

        result                            = setDeEmphasis(1, 5, 0);    // --- Set default De-Emphasis to 1.9 dBm of Post-cursor
        if (result == globals::OK) result = setDeEmphasis(3, 5, 0);    //
        RETURN_ON_ERROR("GT1724: Config Set Defaults: Failed to set up de-emphasis");

        // Make sure PG output lanes are ON:
        result                            = setLaneOn(1, true, false);  // --- Enable / unmute outputs for PG lanes
        emit(SetPGLedStatus(1, true));                                  //Turn on PG2 LED;
        if (result == globals::OK) result = setLaneOn(3, true, false);  //
        emit(SetPGLedStatus(3, true));                                  //Turn on PG4 LED;
        RETURN_ON_ERROR("GT1724: Config Set Defaults: Failed to turn on outputs");
    }

    DEBUG_GT1724("GT1724: PG default setup completed OK!")
    return globals::OK;
}





/*!
  \brief Configure the pattern generator

  Call after a change to:
    * Pattern
    * Clock synth frequency
    * Clock synth RF output level

  This method resets the pattern generator and set up the lanes. It selects
  the requested pattern and turns on the output drivers.
  Note this method DOESN'T set various other parameters which can be
  altered from the PG page while the PG is running, e.g. Output Swing,
  pattern inversion, or De-Emphasis controls. These are set to default values
  by config Set Defaults and are NOT reset on pattern change.

 \param pattern          Index of selected pattern in the combo list.
 \param bitRate          Instrument bit rate (bits/sec)

 SLOT!
 Emits: globals::OK            Success
        [error code]           Error setting register or running macro

*/
void GT1724::ConfigPG(int metaLane, int pattern, double bitRate)
{
    LANE_FILTER(metaLane);
    emit ShowMessage("Configuring Pattern Generator...");
    int result = configPG(pattern, bitRate);
    // Echo back the new pattern to the client
    // Nb: Only one pattern for all PG lanes, so send a list select for both lanes:
    emit ListSelect("listPGPattern", laneOffset + 0, pattern);
    emit ListSelect("listPGPattern", laneOffset + 2, pattern);
    if (BertModel::UseFourChanPGMode())
    {
        emit ListSelect("listPGPattern", laneOffset + 1, pattern);
        emit ListSelect("listPGPattern", laneOffset + 3, pattern);
    }
    if (result == globals::OK) emit ShowMessage("OK.");
    // Send the results:
    emit Result(result, metaLane);
}
// Config PG command implementation: returns globals::OK or error code
int GT1724::configPG(int pattern, double bitRate)
{
    DEBUG_GT1724("GT1724: Configure PG on Lane " << laneOffset)
    int result;

    // REMOVED! Shouldn't need?
    //result = runVerySimpleMacro(0x65);       // --- RESET the PG (Macro 0x65)
    //RETURN_ON_ERROR("GT1724: Config PG: Reset failed");

    result = runSimpleMacro(0x60, 0x01);      // --- Mission Low Power
    RETURN_ON_ERROR("GT1724: Config PG: Mission Low Power failed");

    // Route LB input to PRBS gen (External clock in):
    result = runSimpleMacro(0x60, 0x08);      // --- LB to PRBS gen
    RETURN_ON_ERROR("GT1724: Config PG: LB to PRBS gen failed");

    if (BertModel::UseFourChanPGMode())
    {
        // ====== 4 x PG Mode: ======================
        // Run device multi-cast mode to route PRBS.
        // PRBS to ALL channels:
        result = runSimpleMacro(0x64, 0x1F);      // --- Config device multi-cast mode: All lanes get PRBS Gen to CDR
        RETURN_ON_ERROR("GT1724: Config PG: Lane setup failed");

        result                            = setPDDeEmphasis(0, false);  // --- Power up the De-Emphasis path
        if (result == globals::OK) result = setPDDeEmphasis(1, false);  //
        if (result == globals::OK) result = setPDDeEmphasis(2, false);  //
        if (result == globals::OK) result = setPDDeEmphasis(3, false);  //
        RETURN_ON_ERROR("GT1724: Config PG: Error turning on De-Emphasis path");

        result = setLosEnable(1);                                // --- Turn on LOS detect power
        RETURN_ON_ERROR("GT1724: Config PG: Failed to turn on LOS detect");

        DEBUG_GT1724("GT1724: Change PG pattern to " << pattern)
                result = setPRBSOptions(pattern, 0, 0, 1);               // --- Select PG pattern (Macro 0x58)
        // vcoFreq=0, clockSource=0, enable=1
        RETURN_ON_ERROR("GT1724: Config PG: Error setting PG pattern");

        // Turn on CDR bypass for PG channels
        // (NOTE: Only useful when running as a PG outside of the range that the CDR can lock)
        result                            = setForceCDRBypass(0, forceCDRBypass0, bitRate);
        if (result == globals::OK) result = setForceCDRBypass(1, forceCDRBypass1, bitRate);
        if (result == globals::OK) result = setForceCDRBypass(2, forceCDRBypass2, bitRate);
        if (result == globals::OK) result = setForceCDRBypass(3, forceCDRBypass3, bitRate);
        RETURN_ON_ERROR("GT1724: Config PG: Error setting CDR Bypass");

    }
    else
    {
        // ====== 2 x PG, 2 x ED: ===================

        // Run device multi-cast mode to route PRBS.
        // PRBS to channels 0 and 2: Nb: Assume lanes 1 and 3 will get data from inputs!
        //   OLD, removed: Calling with 0x05 (LB to CDR): NOT needed.
        //   result = runSimpleMacro(0x64, 0x05);   // --- Config device multi-cast mode: Lanes 0 and 2 get LB to CDR
        //   RETURN_ON_ERROR("GT1724: Config PG: Lane setup failed");

        result = runSimpleMacro(0x64, 0x15);      // --- Config device multi-cast mode: Lanes 0 and 2 get PRBS Gen to CDR
        RETURN_ON_ERROR("GT1724: Config PG: Lane setup failed");

        result                            = setPDDeEmphasis(0, false);  // --- Power up the De-Emphasis path
        if (result == globals::OK) result = setPDDeEmphasis(1, false);  //
        if (result == globals::OK) result = setPDDeEmphasis(2, false);  //
        if (result == globals::OK) result = setPDDeEmphasis(3, false);  //
        RETURN_ON_ERROR("GT1724: Config PG: Error turning on De-Emphasis path");

        // REMOVED: Stops the PRBS checker from working.
        //result                            = setPDDeEmphasis(1, 1);  // --- Power DOWN de-emphasis and output driver on lane 1 and 3 (UNUSED output)
        //if (result == globals::OK) result = setPDDeEmphasis(3, 1);
        //RETURN_ON_ERROR("GT1724: Config PG: Error powering down de-emphasis path for lane 1 or 3");

        // Power down main path driver for lanes 1 and 3 (unused outputs; reduces noise)
        result                            = setPDLaneMainPath(1, 1);  // --- Power DOWN Main path driver on lane 1 and 3 (UNUSED output)
        if (result == globals::OK) result = setPDLaneMainPath(3, 1);
        RETURN_ON_ERROR("GT1724: Config PG: Error powering down de-emphasis path for lane 1 or 3");

        result = setLosEnable(1);                                // --- Turn on LOS detect power
        RETURN_ON_ERROR("GT1724: Config PG: Failed to turn on LOS detect");

        DEBUG_GT1724("GT1724: Change PG pattern to " << pattern)
                result = setPRBSOptions(pattern, 0, 0, 1);               // --- Select PG pattern (Macro 0x58)
        // vcoFreq=0, clockSource=0, enable=1
        RETURN_ON_ERROR("GT1724: Config PG: Error setting PG pattern");


        // Turn on CDR bypass for PG channels
        // (NOTE: Only useful when running as a PG outside of the range that the CDR can lock)
        result                            = setForceCDRBypass(0, forceCDRBypass0, bitRate);
        if (result == globals::OK) result = setForceCDRBypass(2, forceCDRBypass2, bitRate);
        RETURN_ON_ERROR("GT1724: Config PG: Error setting CDR Bypass");
    }

    DEBUG_GT1724("GT1724: PG Configured OK!")
    return globals::OK;

}




/*!
 \brief Config CDR Mode
   Configures the GT1724 for "CDR mode" (recover clock data).
   SLOT!

 \param metaLane     Selects the GT1724 core (0, 4, 8, etc)
 \param inputLane    Select input lane to get signal from (1 or 3)
 \param freqDivider  Select divide ratio (0 - 5, see GT1724 documentation)
*/
void GT1724::ConfigCDR(int metaLane, int inputLane, int freqDivider)
{
    LANE_FILTER(metaLane);
    if (inputLane < 0 || inputLane > 3) return;       // Invalid setting.
    if (freqDivider < 0 || freqDivider > 5) return;   // Invalid setting.
    emit ShowMessage("Configuring Clock Recovery Mode");
    int result = configCDR(inputLane, freqDivider);
    if (result == globals::OK) emit ShowMessage("OK.");
    // Send the results:
    emit Result(result, metaLane);
}
// Config CDR Mode command implementation: returns globals::OK or error code
int GT1724::configCDR(int inputLane, int freqDivider)
{
    DEBUG_GT1724("GT1724: Configure CDR on GT1724 at lane " << laneOffset << "; using input lane " << inputLane)
    int result;

    result = runVerySimpleMacro(0x65);       // --- RESET device configuration (Macro 0x65)
    RETURN_ON_ERROR("GT1724: Config CDR: Reset failed");

    result = runSimpleMacro(0x60, 0x01);      // --- Mission Low Power
    RETURN_ON_ERROR("GT1724: Config CDR: Mission Low Power failed");

    // Select RECOVERED CLOCK TO MCK mode:
    uint8_t param = static_cast<uint8_t>(static_cast<uint8_t>(freqDivider) << 5) | static_cast<uint8_t>(inputLane);
    result = runSimpleMacro(0x63, param);      // --- Config Device Lane Mode: RECOVERED CLOCK TO MCK
    RETURN_ON_ERROR("GT1724: Config PG: LB to PRBS gen failed");

    DEBUG_GT1724("GT1724: CDR Mode Configured OK!")
    return globals::OK;
}



/************* Channel ON / OFF: ***************************************/
/*!
 \brief Set Lane Output On / Off (Off actually means Mute).
 \param lane    Lane to set
 \param laneOn  On / Off status; TRUE = On (Unmute)
 \param powerDownOnMute Also power down the output driver when muted
                       (NOTE: This will also disable PRBS checker for the lane!)
 SLOT!
 Emits: Result globals::OK            Success
        Result [error code]           Error setting register or running macro
*/
void GT1724::SetLaneOn(int lane, bool laneOn, bool powerDownOnMute)
{
    LANE_FILTER(lane);
    int result = setLaneOn(lane, laneOn, powerDownOnMute);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error turning channel On / Off.");
}
// Set Lane On / Off - Command implementation
int GT1724::setLaneOn(int lane, bool laneOn, bool powerDownOnMute)
{
    DEBUG_GT1724("GT1724: Set Lane " << lane << " mute options - Lane On: " << laneOn << "; powerDownOnMute: " << powerDownOnMute)
    // Get current register contents (Nb: From docs, shouldn't change bits 3-7):
    emit SetPGLedStatus(lane, laneOn);
    uint8_t data = 0;
    getRegister(LANE_MOD(lane), GTREG_DRV_REG_0, &data);
    data = (data & 0xF8);  // Mask out lower 3 bits, = 0b11111000
    if (powerDownOnMute) data = (data | 0x04);  // Set bit 2 if "Power Down On Mute" is specified.
    if (!laneOn)         data = (data | 0x01);  // Set bit 0 if "Mute" (Off!) is specified.
    return setRegister(LANE_MOD(lane), GTREG_DRV_REG_0, data);
}
/*!
 \brief Read Lane on / off state (Off = Muted)
 \param lane    Lane to read
*/
bool GT1724::getLaneOn(int lane)
{
    DEBUG_GT1724("GT1724: Get Lane " << lane << " On / Off...")
    bool state = false;
    uint8_t data = 0;
    int result = getRegister(LANE_MOD(lane), GTREG_DRV_REG_0, &data);
    if (result == globals::OK)
    {
        if (data & 0x01) state = false; // Muted! (Off)
        else             state = true;  // Not Muted (On)
        emit UpdateBoolean("boolPGLaneOn", lane, state);
    }
    else
    {
        DEBUG_GT1724("GT1724: Error reading LaneOn status (Lane: " << lane << "; Result: " << result << ")")
    }
    return state;
}



/* UNUSED!  TODO: Update and add slots if needed...
************* CDR Auto-Bypass on LOL: ************************************

*
 \brief Set CDR Auto Bypass On LOL bit
 \param lane           Lane to set
 \param autoBypassOn   Autobypass status - TRUE = On
 SLOT!
 Emits: Result globals::OK            Success
        Result [error code]           Error setting register or running macro
*
int GT1724::SetAutoBypassOnLOL(int lane, bool autoBypassOn)
{
    LANE_FILTER(lane);
    emit Result(setAutoBypassOnLOL(lane, autoBypassOn), lane);
}
int GT1724::setAutoBypassOnLOL(int lane, bool autoBypassOn)
  {
  qDebug() << "Setting Lane " << lane << " CDR Auto Bypass on LOL to " << autoBypassOn;
  // Get current register contents - Auto bypass is Bit 1
  // (Nb: From docs, should preserve values of other bits):
  uint8_t data = 0;
  getRegister(LANE_MOD(lane), GTREG_CDR_REG_0, &data);
  data = (data & 0xFD);  // Mask out bit 1, = 0b11111101
  int result;
  if (autoBypassOn) result = setRegister(LANE_MOD(lane), GTREG_CDR_REG_0, (data | 0x02) );  // Auto bypass ON
  else              result = setRegister(LANE_MOD(lane), GTREG_CDR_REG_0, (data)        );  // Auto bypass OFF
  return result;
  }
* UNUSED!
 \brief Read CDR Auto Bypass On LOL bit
 \param lane    Lane to read
 SLOT!
 Emits: UpdateBoolean "boolAutoBypassOnLOL"
*
bool GT1724::getAutoBypassOnLOL (int lane)
  {
  uint8_t data = 0;
  getRegister(LANE_MOD(lane), GTREG_CDR_REG_0, &data);
  return ( (data & 0x02) >> 1 );   // Return only the value of Bit 1 (Auto Bypass bit)
  }
*/

/************* CDR Force Bypass: ***************************************/
/*!
 \brief Check whether Force CDR Bypass should be ON (depends on setting and bit rate)
 \param forceCDRBypass  0 = OFF; 1 = ON; 2 = Auto
 \param bitRate         Bit rate in bits per second
 \return true or false
*/
bool GT1724::checkForceCDRBypass(int forceCDRBypass, double bitRate)
{
    switch (forceCDRBypass)
    {
    case 0:    // OFF
        return false;
    case 1:     // ON
        return true;
    default:    // AUTO: Depends on whether bit rate is within the CDR bands
        if ( (bitRate < 12.5e9)
         ||  (bitRate > 14.5e9 && bitRate < 24.5e9)
         ||  (bitRate > 29.0e9) )
        {
            return true;  // OUT of CDR band; CDR Bypass ON
        }
        else
        {
            return false;  // IN BAND: CDR Bypass OFF
        }
    }
}


/*!
 \brief Set CDR Force Bypass bit
 \param lane           Lane to set
 \param forceCDRBypass Force Bypass status - TRUE = On
 SLOT!
 Emits: Result globals::OK            Success
        Result [error code]           Error setting register or running macro
*/
void GT1724::SetForceCDRBypass(int lane, int forceCDRBypass, double bitRate)
{
    Q_ASSERT(forceCDRBypass >= 0 && forceCDRBypass < 3);
    if (forceCDRBypass < 0 || forceCDRBypass > 2) return; // Invalid setting.
    LANE_FILTER(lane);
    int result = setForceCDRBypass(lane, forceCDRBypass, bitRate);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error turning CDR Bypass On / Off.");
}
// Set CDR Bypass
int GT1724::setForceCDRBypass(int lane, int forceCDRBypass, double bitRate)
{
    int localLane = LANE_MOD(lane);
    if (localLane == 0) forceCDRBypass0 = forceCDRBypass;
    if (localLane == 1) forceCDRBypass1 = forceCDRBypass;
    if (localLane == 2) forceCDRBypass2 = forceCDRBypass;
    if (localLane == 3) forceCDRBypass3 = forceCDRBypass;

    bool forceBypassOn = checkForceCDRBypass(forceCDRBypass, bitRate);

    qDebug() << "CDR Bypass: Lane " << lane << "; Setting: " << forceCDRBypass << "; Set CDR Force Bypass to " << forceBypassOn;
    // Get current register contents - Force bypass is Bit 0
    // (Nb: From docs, should preserve values of other bits):
    uint8_t data = 0;
    getRegister(LANE_MOD(lane), GTREG_CDR_REG_0, &data);
    //qDebug() << "Previous Value: ";
    //qDebug() << "  GTREG_CDR_REG_0: " << data;
    data = (data & 0xF8);  // Mask out bit 0, = 0b11111000
    int result;
    if (forceBypassOn) result = setRegister(LANE_MOD(lane), GTREG_CDR_REG_0, (data | 0x05) );  // bypass ON; CDR power DOWN
    else               result = setRegister(LANE_MOD(lane), GTREG_CDR_REG_0, (data)        );  // bypass OFF
    return result;
}

/*!
 \brief Emit a signal with current "Force CDR Bypass" setting
 \param lane    Lane to emit signal for. Only even lanes have "Force CDR Bypass"
                setting; other lanes will be ignored.
   EMITS ListSelect "listPGCDRBypass"
*/
int GT1724::getForceCDRBypass (int lane)
{
    int localLane = LANE_MOD(lane);
    if (localLane == 0) emit ListSelect("listPGCDRBypass", lane, forceCDRBypass0);
    if (localLane == 1) emit ListSelect("listPGCDRBypass", lane, forceCDRBypass1);
    if (localLane == 2) emit ListSelect("listPGCDRBypass", lane, forceCDRBypass2);
    if (localLane == 3) emit ListSelect("listPGCDRBypass", lane, forceCDRBypass3);
    return globals::OK;

    /* DEPRECATED: Don't read actual setting from device;
     * Just return the virtual setting forceCDRBypassX.
    bool cdrBypassOn = false;
    uint8_t data = 0;
    int result = getRegister(LANE_MOD(lane), GTREG_CDR_REG_0, &data);
    if (result == globals::OK)
    {
        if (data & 0x01) cdrBypassOn = true;   // Check the value of Bit 0 (Force Bypass bit)
        emit ListSelect("listPGCDRBypass", lane, cdrBypassOn);
    }
    else
    {
        DEBUG_GT1724("GT1724: Error reading de-emphasis settings (Lane: " << lane << "; Result: " << result << ")")
    }
    return result;
    */
}




/*************** Output driver swing: *************************************/
/*!
 \brief Set output driver swing
 \param lane        Lane to set
 \param swingIndex  Index of new voltage swing in PG_OUTPUT_SWING_LIST
  SLOT
*/
void GT1724::SetOutputSwing(int lane, int swingIndex)
{
    LANE_FILTER(lane);
    Q_ASSERT(swingIndex >= 0 && swingIndex < PG_OUTPUT_SWING_LOOKUP.size());
    int swing = PG_OUTPUT_SWING_LOOKUP.at(swingIndex);
    int result = setOutputSwing(lane, swing);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error setting channel amplitude.");
}
// Command Implementation:
int GT1724::setOutputSwing(int lane, int swing)
{
    // We can only set ALL swing values (not a single lane), so first we
    // need to QUERY existing values, then update only the requested lane.
    int result;
    int swings[4];
    result = queryOutputDriverMainSwing(swings); // Query existing swing values
    if (result != globals::OK)
    {
        DEBUG_GT1724("GT1724: Error reading current output swings: " << result)
        return result;
    }
    // Now update the requested lane and set the new values:
    swings[LANE_MOD(lane)] = swing;
    result = configOutputDriverMainSwing(swings);
    if (result != globals::OK)
    {
        DEBUG_GT1724("GT1724: Error setting new output swings: " << result)
        return result;
    }
    return globals::OK;
}

/*!
 \brief Get output swings for lanes 0 and 2 (PG Outputs),
        and emit ListSelect signals to update clients
 \return globals::OK  succes
 \return [error code]

 Emits: ListSelect for lanes 0 and 2 of this GT1724,
        with index of current voltage swing selection
*/
int GT1724::getOutputSwings()
{
    int swingData[4];
    int result = queryOutputDriverMainSwing(swingData);
    if (result != globals::OK)
    {
        DEBUG_GT1724("GT1724: Error getting output swings (" << result << ")")
        return result;
    }
    int i, swingIndex, laneStep;
    laneStep = 2;
    if (BertModel::UseFourChanPGMode()) laneStep = 1;

    for (i=0; i<4; i+=laneStep)
    {
        // Convert swing register setting (returned by query) to list index:
        swingIndex = PG_OUTPUT_SWING_LOOKUP.indexOf(swingData[i]);
        DEBUG_GT1724("GT1724: Get Output Swing for Lane " << i << ": Swing = " << swingData[i] << "; Index = " << swingIndex)
        if (swingIndex < 0) swingIndex = 0;
        emit ListSelect("listPGAmplitude", laneOffset + i, swingIndex);
    }
    return globals::OK;
}

/*!
 \brief Run Config Output Driver Main Swing Macro
 \param  swings  Pointer to an array of 4 integers, used to set voltage swing.
                 Note the range of these values must be 40 to 220, representing
                 approx mV / 5 (i.e. actual output swing will be in the range
                 200 - 1100 mV).
 \return globals::OK           Success
 \return [Error Code]          Error from hardware/comms functions
*/
int GT1724::configOutputDriverMainSwing(int swings[4])
{
    uint8_t swingData[4];
    // Clamp supplied values to range 40-220:
    int i;
    for (i=0; i<4; i++)
    {
        swingData[i] = (uint8_t)swings[i];
        if (swingData[i] < 40)  swingData[i] = 40;
        if (swingData[i] > 220) swingData[i] = 220;
        DEBUG_GT1724("GT1724: Setting Output Swing Lane " << i << " to " << swingData[i] << "( " << swings[i] << " mV)")
    }
    int result;
    result = runMacro(0x61, swingData, 4, NULL, 0 );  // Config Output Driver Main Swing Macro
    return result;
}
/*!
 \brief Run Query Output Driver Main Swing Macro
 \param  swings  Pointer to an array of 4 integers, used to return voltage swing.
                 Note the range of these values will be 40 to 220, representing
                 approx mV / 5 (i.e. actual output swing will be in the range
                 200 - 1100 mV).
                 If an error occurs, the values in swings will be set to 0.
 \return globals::OK      Success
 \return [Error Code]     Error from hardware/comms functions
*/
int GT1724::queryOutputDriverMainSwing(int swings[4])
{
    // Default results: Set swings to 0 (in case of error...).
    int i;
    for (i=0; i<4; i++) swings[i] = 0;
    // Use the Query Output Driver Main Swing Macro:
    uint8_t swingData[4] = { 0 };
    int result;
    result = runMacro(0x69, NULL, 0, swingData, 4 );
    if (result != globals::OK) return result;     //  Macro error...
    for (i=0; i<4; i++)
    {
        DEBUG_GT1724("GT1724: Read Output Swing Lane " << i << ": " << swingData[i])
        swings[i] = (int)swingData[i];
    }
    return globals::OK;
}




/********** Query / Configure PRBS Options: **************************/
/*!
 \brief Set PRBS Options
 Calls the Configure and Enable PRBS Generator Macro
 \param pattern  Pattern to use (0-7, see 1724 specs)
 \param vcoFreq  VCO frequency selection (0-7, see 1724 specs)
 \param source   Source (0 = External, 1 = Internal VCO)
 \param enable   Enable bit (0 = Disable, 1 = Enable)
 \return globals::OK            Success
 \return globals::OVERFLOW      Input parameter out of allowed range
 \return [Error Code]           Error from hardware/comms functions
*/
int GT1724::setPRBSOptions(int pattern, int vcoFreq, int source, int enable)
{
    Q_ASSERT(pattern >= 0 && pattern <= 7);
    Q_ASSERT(vcoFreq >= 0 && vcoFreq <= 7);
    Q_ASSERT(source >= 0 && source <= 1);
    Q_ASSERT(enable >= 0 && enable <= 1);

    if ( (pattern < 0) || (pattern > 7) ||
         (vcoFreq < 0) || (vcoFreq > 7) ||
         (source < 0)  || (source > 1)  ||
         (enable < 0)  || (enable > 1) ) return globals::OVERFLOW;
    uint8_t prbsOption;
    prbsOption = ((uint8_t)pattern << 5) | ((uint8_t)vcoFreq << 2) | ((uint8_t)source << 1) | (uint8_t)enable;
    return runMacro(0x58, &prbsOption, 1, NULL, 0);
}

/*!
 \brief Get Current PRBS Option Setting
 Calls the Query PRBS Generator Macro
 \param pattern  int pointer to return pattern (0-7, see 1724 specs)
 \param vcoFreq  int pointer to return VCO frequency selection (0-7, see 1724 specs)
 \param source   int pointer to return source (0 = External, 1 = Internal VCO)
 \param enable   int pointer to return enable bit (0 = Disable, 1 = Enable)
 \return globals::OK           Success
 \return [Error Code]          Error from hardware/comms functions
*/
int GT1724::getPRBSOptions(int *pattern, int *vcoFreq, int *source, int *enable)
{
    uint8_t prbsOption = 0;
    int result = runMacro(0x59, NULL, 0, &prbsOption, 1);
    if (result != globals::OK) return result;
    *pattern = (prbsOption & 0xE0) >> 5;
    *vcoFreq = (prbsOption & 0x1C) >> 2;
    *source  = (prbsOption & 0x02) >> 1;
    *enable  = (prbsOption & 0x01);
    return globals::OK;
}
/*!
 \brief Get Current PRBS pattern setting and send to CLIENT.
 \return globals::OK           Success
 \return [Error Code]          Error from hardware/comms functions
 Emits ListSelect for lanes 0 and 2 of this GT1724,
       with index of current pattern selection
*/
int GT1724::getPRBSPattern(int *pattern)
{
    *pattern = 0;
    int vcoFreq, source, enable;
    int result = getPRBSOptions(pattern, &vcoFreq, &source, &enable);
    if (result != globals::OK)
    {
        DEBUG_GT1724("GT1724: Error getting PRBS gen pattern (" << result << ")")
        return result;
    }
    emit ListSelect("listPGPattern", laneOffset + 0, *pattern);
    emit ListSelect("listPGPattern", laneOffset + 2, *pattern);
    if (BertModel::UseFourChanPGMode())
    {
        emit ListSelect("listPGPattern", laneOffset + 1, *pattern);
        emit ListSelect("listPGPattern", laneOffset + 3, *pattern);
    }
    return globals::OK;
}






/********* 'Inverted' On / Off: ***********************

 "Inversion" of the signal is controlled by the
 l#_eq_polarity_invert bit (l#_eq_reg_0 register).

 Note that the Query / Configure Device Multicast Mode
 macros also read / set this bit, along with doing
 other things in the case of the Configure macro.

*******************************************************/
/*!
 \brief Set Lane Inverted On / Off
 \param lane    Lane to set

 \param inverted  Inverted status:  true = Inverted

 \return globals::OK      Success
 \return [Error Code]     Error from hardware/comms functions
*/
int GT1724::setLaneInverted(int lane, bool inverted)
  {
  DEBUG_GT1724("GT1724: Setting Lane " << lane << " inverted to " << inverted)

  /**** Set invert bit in REG_EQ_REG_0: **************/
  // Get current register contents (Nb: From docs, shouldn't change reserved bits):
  uint8_t dataER = 0;
  getRegister(LANE_MOD(lane), GTREG_EQ_REG_0, &dataER);
  dataER = (dataER & 0xFE);  // Mask out bit 0 (=0b11111110)
  int result;
  if (inverted)
    {
    result = setRegister(LANE_MOD(lane), GTREG_EQ_REG_0, (dataER | 0x01) );  // Inverted (Nb: set bit 0 only)
    }
  else
    {
    result = setRegister(LANE_MOD(lane), GTREG_EQ_REG_0, dataER );  // Not Inverted
    }
  /********************************************************************/

  return result;
  }
// Set lane inverted status: SLOT
// Emits Result
void GT1724::SetLaneInverted(int lane, bool inverted)
{
    LANE_FILTER(lane);
    int result = setLaneInverted(lane, inverted);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error setting pattern inversion.");
}

/*!
 \brief Read 'Inverted' state
 \param lane    Lane to read
 \return true   Channel is inverted
 \return false  Channel is not inverted
*/
bool GT1724::getLaneInverted(int lane, bool emitSignal)
{
    bool state = false;
    uint8_t dataER = 0;
    int result = getRegister(LANE_MOD(lane), GTREG_EQ_REG_0, &dataER);
    if (result == globals::OK)
    {
        dataER = (dataER & 0x01);  // Mask out bit 0
        state = (dataER != 0);
        if (emitSignal) emit UpdateBoolean("boolPGInverted", lane, state);
    }
    else
    {
        DEBUG_GT1724("GT1724: Error reading LaneInverted status (Lane: " << lane << "; Result: " << result << ")")
    }
    return state;
}




/********* Power Control for De-Emphasis Path: ***********************/
/*!
 \brief Set Power Down bit for DeEmphasis Path
 \param lane       Lane to set
 \param powerDown  Power Down: true  = power DOWN (OFF) de-emphasis path and de-emphasis output driver
                               false = de-emphasis path and de-emphasis output driver are TURNED ON!
Must be 0 or 1.
                       1 = power down de-emphasis path and de-emphasis output driver
 \return globals::OK        Success
 \return [Error Code]       Error from hardware/comms functions
*/
int  GT1724::setPDDeEmphasis(int lane, bool powerDown)
{
    DEBUG_GT1724("GT1724: Setting lane " << lane << " de-emphasis power down bit to: " << powerDown)
    // Get current register contents (Nb: From docs, shouldn't change reserved bits):
    uint8_t data = 0;
    uint8_t pdBit;
    if (powerDown) pdBit = 0x01;
    else           pdBit = 0x00;
    getRegister(LANE_MOD(lane), GTREG_PD_REG_6, &data);
    data =  (data & 0xFD) |    // Use mask to clear bit 1 (need to preserve rest of register)
            (pdBit << 1);      // Add 'De-emph Power Down' bit (shift to place in bit 1)
    return setRegister(LANE_MOD(lane), GTREG_PD_REG_6, data );
}

/*!
 \brief Get Power Down bit for DeEmphasis Path
 \param lane       Lane to read settings for (0-3)
 \param powerDown  Pointer to uint8; used to return Power Down bit
 \return globals::OK
*/
bool GT1724::getPDDeEmphasis(int lane)
{
    uint8_t data = 0;
    getRegister(LANE_MOD(lane), GTREG_PD_REG_6, &data);
    bool powerDown = false;
    if ((data & 0x02) >> 1 == 0x01) powerDown = true;  // Mask out all bits except 1, and shift right one to give deemph pd bit
    return powerDown;
}



/* // DEPRECATED  UNUSED
********* Power Control for Lane Output Driver: ***********************
*
 \brief Set Power Down bit for entire lane output driver
 \param lane       Lane to set (0 - 7);
 \param powerDown  Power Down bit; Must be 0 or 1.
                       1 = power down lane output driver
 \return globals::OK        Success
 \return globals::OVERFLOW  Input parameter out of allowed range
 \return [Error Code]       Error from hardware/comms functions
*
int  GT1724::setPDLaneOutput (const uint8_t lane, const uint8_t powerDown)
  {
  qDebug() << "GT1724: Setting lane " << lane << " output driver power down bit to: " << powerDown;
  if (powerDown > 1) return globals::OVERFLOW;
  // Get current register contents (Nb: From docs, shouldn't change reserved bits):
  uint8_t data = 0;
  getRegister(LANE_MOD(lane), GTREG_PD_REG_6, &data);
  data =  (data & 0xFE) |    // Use mask to clear bit 0 (need to preserve rest of register)
          (powerDown);       // Add 'Lane Power Down' bit (bit 0)
  return setRegister(LANE_MOD(lane), GTREG_PD_REG_6, data );
  }
*
 \brief Get Power Down bit for entire lane output driver
 \param lane       Lane to read settings for (0-7)
 \param powerDown  Pointer to uint8; used to return Power Down bit
 \return globals::OK
*
int  GT1724::getPDLaneOutput (const uint8_t lane, uint8_t *powerDown)
  {
  // qDebug() << "GT1724: Checking Lane " << lane << " Ouput Power Down Settings...";
  uint8_t data = 0;
  getRegister(LANE_MOD(lane), GTREG_PD_REG_6, &data);
  *powerDown   = (data & 0x01);  // Mask out all bits except 0
  return globals::OK;
  }
*/




/********* De-Emphasis Level: ***********************/
/*!
 \brief Set De-Emphasis settings (De-emphasis level and pre/post cursor selection)
 \param lane     Lane to set
 \param level    De-emphasis level (0-15)
 \param prePost  Pre / Post cursor option (0 = Post, 1 = Pre).
 \return globals::OK        Success
 \return globals::OVERFLOW  Input parameter out of allowed range
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::setDeEmphasis(int lane, int level, int prePost)
{
    DEBUG_GT1724("GT1724: Setting Lane " << lane << " de-emphasis; level: " << level << "prePost: " << prePost)
    Q_ASSERT(level >= 0 && level < PG_EQ_DEEMPH_LIST.size());
    Q_ASSERT(prePost >= 0 && prePost <= 1);
    if ( (level < 0) || (level >= PG_EQ_DEEMPH_LIST.size()) || (prePost < 0) || (prePost > 1) ) return globals::OVERFLOW;
    // Get current register contents (Nb: From docs, shouldn't change reserved bits):
    uint8_t data = 0;
    getRegister(LANE_MOD(lane), GTREG_DRV_REG_2, &data);
    data =  (data & 0xE0) |   // Use mask to clear all but bits 5-7 (need to preserve these)
            (level << 1)  |   // Add 'level' bits (shift to place in bits 1-4)
            (prePost);        // Add Pre / Post (bit 0)
    return setRegister(LANE_MOD(lane), GTREG_DRV_REG_2, data );
}
// SLOT to set De-Emphasis
void GT1724::SetDeEmphasis(int lane, int level, int prePost)
{
    LANE_FILTER(lane);
    int result = setDeEmphasis(lane, level, prePost);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error setting de-emphasis options.");
}
/*!
 \brief Get De-Emphasis settings (De-emphasis level and pre/post cursor selection)
 \param lane     Lane to read settings for
 \param level    Pointer to uint8, used to return De-emphasis level (0-15)
 \param prePost  Pointer to uint8, used to return Pre / Post cursor option (0 = Post, 1 = Pre).
 \return globals::OK
*/
int GT1724::getDeEmphasis(int lane)
{
    uint8_t level = 0;
    uint8_t prePost = 0;
    uint8_t data = 0;
    int result = getRegister(LANE_MOD(lane), GTREG_DRV_REG_2, &data);
    level   = (data & 0x1E) >> 1;  // Mask out all bits except 1-4, and shift right one to give deemph_level
    prePost = (data & 0x01);       // Pre / Post given by bit 0.
    if (result == globals::OK)
    {
        emit ListSelect("listPGDeemphLevel", lane, (int)level);
        emit ListSelect("listPGDeemphCursor", lane, (int)prePost);
    }
    else
    {
        DEBUG_GT1724("GT1724: Error reading de-emphasis settings (Lane: " << lane << "; Result: " << result << ")")
    }
    return result;
}




/********* Cross point adjust *********************************/
/*!
 \brief Set output driver crossing point adjust
 \param lane            Lane to set
 \param crossPointIndex Index of Cross Point setting to use, from PG_CROSS_POINT_LOOKUP
                        See GT1724 data sheet for all values
 \return globals::OK        Success
 \return globals::OVERFLOW  Input parameter out of allowed range
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::setCrossPoint(int lane, int crossPointIndex)
{
    Q_ASSERT(crossPointIndex >= 0 && crossPointIndex < PG_CROSS_POINT_LOOKUP.size());
    if (crossPointIndex < 0|| crossPointIndex > PG_CROSS_POINT_LOOKUP.size()) return globals::OVERFLOW;
    int crossPoint = PG_CROSS_POINT_LOOKUP.at(crossPointIndex);
    DEBUG_GT1724("GT1724: Setting cross point... level: " << crossPoint)
    // Get current register contents (Nb: From docs, shouldn't change bits 5-7):
    uint8_t data = 0;
    getRegister(LANE_MOD(lane), GTREG_DRV_REG_5, &data);
    data = (data & 0xE0) | (uint8_t)crossPoint;  // Mask out lower 5 bits, and OR with new cross point value:
    return setRegister(LANE_MOD(lane), GTREG_DRV_REG_5, data);
}
// SLOT to set cross point:
void GT1724::SetCrossPoint(int lane, int crossPointIndex)
{
    LANE_FILTER(lane);
    int result = setCrossPoint(lane, crossPointIndex);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error setting de-emphasis options.");
}
/*!
 \brief Get output driver crossing point adjust
        On csuccess, emits List Select signal with
        index of current cross point setting for lane
 \param lane       Lane to read
 \return globals::OK        Success
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::getCrossPoint(int lane)
{
    // Get current register contents (Nb: From docs, shouldn't change bits 5-7):
    uint8_t crossPoint;
    int result = getRegister(LANE_MOD(lane), GTREG_DRV_REG_5, &crossPoint);
    crossPoint = (crossPoint & 0x1F);   // Mask out upper 3 bits (reserved)
    if (result == globals::OK)
    {
        int crossPointIndex = PG_CROSS_POINT_LOOKUP.indexOf((int)crossPoint);
        if(crossPointIndex < 0) crossPointIndex = 0;
        emit ListSelect("listPGCrossPoint", lane, crossPointIndex);
    }
    else
    {
        DEBUG_GT1724("GT1724: Error reading de-emphasis settings (Lane: " << lane << "; Result: " << result << ")")
    }
    return result;
}



/********* EQ Boost *********************************/
/*!
 \brief Set input stage eq boost
 \param lane          Lane to set
 \param eqBoostIndex  Index of EQ Boost level in ED_EQ_BOOST_LOOKUP table (=EQ Boost register value)
                      See GT1724 data sheet
 \return globals::OK        Success
 \return globals::OVERFLOW  Input parameter out of allowed range
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::setEQBoost(int lane, int eqBoostIndex)
{
    Q_ASSERT(eqBoostIndex >= 0 && eqBoostIndex < ED_EQ_BOOST_LOOKUP.size());
    if (eqBoostIndex < 0 || eqBoostIndex > ED_EQ_BOOST_LOOKUP.size()) return globals::OVERFLOW;
    uint8_t eqBoost = (uint8_t)ED_EQ_BOOST_LOOKUP.at(eqBoostIndex);
    DEBUG_GT1724("GT1724: Setting EQ Boost... level: " << eqBoost)
    // Get current register contents (Bb: From docs, shouldn't change bit 7):
    uint8_t data = 0;
    getRegister(LANE_MOD(lane), GTREG_EQ_REG_2, &data);
    data = (data & 0x80) | eqBoost;  // Mask out lower 7 bits, and OR with new EQ value
    return setRegister(LANE_MOD(lane), GTREG_EQ_REG_2, data);
}
// SLOT to set EQ Boost
void GT1724::SetEQBoost(int lane, int eqBoostIndex)
{
    LANE_FILTER(lane);
    int result = setEQBoost(lane, eqBoostIndex);
    emit Result(result, lane);
    if (result != globals::OK) emit ShowMessage("Error setting EQ boost.");
}

/*!
 \brief Get input stage eq boost
        On csuccess, emits List Select signal with
        index of current cross point setting for lane
 \param lane       Lane to read
 \return globals::OK        Success
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::getEQBoost(int lane)
{
    // Get current register contents:
    uint8_t eqBoost;
    int result = getRegister(LANE_MOD(lane), GTREG_EQ_REG_2, &eqBoost);
    eqBoost = (eqBoost & 0x7F);   // Mask out upper bit (reserved)
    if (result == globals::OK)
    {
        int eqBoostIndex = ED_EQ_BOOST_LOOKUP.indexOf((int)eqBoost);
        if(eqBoostIndex < 0) eqBoostIndex = 0;
        emit ListSelect("listEDEQBoost", lane, eqBoostIndex);
    }
    else
    {
        DEBUG_GT1724("GT1724: Error reading EQ Boost settings (Lane: " << lane << "; Result: " << result << ")")
    }
    return result;
}



/********* Power Control for Lane "Main Path": ***********************/
/*!
 \brief Set Power Down bit for lane's "main path".
        Note: this is based on undocumented information passed to us by
        Steven Buchinger <SBuchinger@semtech.com>
        "What I’ve found to work is to power down only the output driver of
         the main path. This is similar to 0x*42[0:0] but only powers down the
         main path driver. To do this, you can set 0x*42[3:3] = 1."

 \param lane       Lane to set
 \param powerDown  Power Down bit; Must be 0 or 1.
                       1 = power down lane main path
 \return globals::OK        Success
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::setPDLaneMainPath(int lane, bool powerDown)
{
    DEBUG_GT1724("GT1724: Setting lane " << lane << " main path power down bit to: " << powerDown)
    // Get current register contents (Nb: From docs, shouldn't change reserved bits):
    uint8_t data = 0;
    uint8_t pdBit;
    if (powerDown) pdBit = 0x01;
    else           pdBit = 0x00;
    getRegister(LANE_MOD(lane), GTREG_PD_REG_6, &data);
    data =  (data & 0xF7) |    // Use mask to clear bit 3 (need to preserve rest of register)
            (pdBit << 3);      // Add 'Main Path Power Down' bit (bit 3)
    return setRegister(LANE_MOD(lane), GTREG_PD_REG_6, data);
}





/********** Query / Configure Error Detector Options: ******************/
/*!
 \brief Set ED Options
 Calls the Configure and Enable PRBS Checker Macro
 The function sets the options for both interlane checkers at the same time.
 Nb: 'XX' refers to: 01 - ED options for lane 0 / 1 checker;
                     23 - ED Options for lane 2 / 3 checker.

 \param patternXX     Reference pattern to use (0 - 2): 0 = PRBS 9; 1 = PRBS 15; 2 = PRBS 31
 \param invertXX      Invert the pattern (0 - Normal; 1 = Invert)
 \param enableXX      Enable the PRBS checker (0 = Disable; 1 = Enable)

 \return globals::OK         Success
 \return globals::OVERFLOW   Input parameter out of allowed range
 \return [Error Code]        Error from hardware/comms functions
*/
int GT1724::setEDOptions(int pattern01, int invert01, int enable01,
                         int pattern23, int invert23, int enable23)
{
    const uint8_t laneSelect = 1;  // Select input lane for checker: 0 = Lane 0 (ED 0) / 2 (ED 1) ; 1 = Lane 1 (ED 0) / 3 (ED 1)

    uint8_t edOptionData[2];
    int result;

    edOptionData[0] = ((uint8_t)pattern23 << 6) | ((uint8_t)laneSelect << 5) | ((uint8_t)enable23 << 4) |
                      ((uint8_t)pattern01 << 2) | ((uint8_t)laneSelect << 1) | ((uint8_t)enable01);
    edOptionData[1] = ((uint8_t)invert23 << 1) | (uint8_t)invert01;

    DEBUG_GT1724("GT1724: Enabling PRBS checker on GT1724 " << laneOffset << QString("; Data: B0 [0x%1] B1 [0x%2]").arg(edOptionData[0], 2, 16, QChar('0')).arg(edOptionData[1], 2, 16, QChar('0')))

    // Note: Enabling the PRBS checker with long patterns (e.g. PRBS31) may take up to 2 seconds
    // to complete, so we use runLongMacro here to specify a longer timeout.
    result = runLongMacro(0x50, edOptionData, 2, NULL, 0, 5000);
    if (result != globals::OK) return result;

    return globals::OK;
}
// SLOT: Set ED Options
void GT1724::SetEDOptions(int metaLane,
                          int pattern01, bool invert01, bool enable01,
                          int pattern23, bool invert23, bool enable23)
{
    LANE_FILTER(metaLane);
    Q_ASSERT(pattern01 >= 0 && pattern01 < 3);
    Q_ASSERT(pattern23 >= 0 && pattern23 < 3);
    uint8_t inv01 = (invert01) ? 0x01 : 0x00;
    uint8_t inv23 = (invert23) ? 0x01 : 0x00;
    uint8_t ena01 = (enable01) ? 0x01 : 0x00;
    uint8_t ena23 = (enable23) ? 0x01 : 0x00;
    int result = setEDOptions((int)pattern01, inv01, ena01,
                              (int)pattern23, inv23, ena23);

    // If starting the ED, Reset the bit and error counts, and run timer:
    if (ena01)
    {
        ed01.bitsTotal = 0.0;
        ed01.errorsTotal = 0.0;
        ed01.lastMeasureTimeMs = 0;
        ed01.edRunTime->start();
        ed01.edRunning = true;
    }
    else
    {
        ed01.edRunning = false;
    }
    if (ena23)
    {
        ed23.bitsTotal = 0.0;
        ed23.errorsTotal = 0.0;
        ed23.lastMeasureTimeMs = 0;
        ed23.edRunTime->start();
        ed23.edRunning = true;
    }
    else
    {
        ed23.edRunning = false;
    }
    emit Result(result, laneOffset);
    if (result != globals::OK) emit ShowMessage("Error setting ED options.");
}

/*!
 \brief Get Current ED Option Setting
 Calls the Query PRBS Checker Macro

 \return globals::OK           Success
 \return [Error Code]          Error from hardware/comms functions

 Emits (on success): UpdateBoolean and ListSelect signals for ED options:
           listEDPattern
           boolEDPatternInvert
           boolEDEnable
*/
int GT1724::getEDOptions()
{
    uint8_t edOptionsData[2] = {0,0};
    int result;

    result = runMacro(0x51, NULL, 0, edOptionsData, 2 );
    if (result != globals::OK) return result;

    uint8_t invert01     = (edOptionsData[1])      & 0x01;
    uint8_t pattern01    = (edOptionsData[0] >> 2) & 0x03;
    uint8_t enable01     = (edOptionsData[0])      & 0x01;

    uint8_t invert23     = (edOptionsData[1] >> 1) & 0x01;
    uint8_t pattern23    = (edOptionsData[0] >> 6) & 0x03;
    uint8_t enable23     = (edOptionsData[0] >> 4) & 0x01;

    emit ListSelect    ("listEDPattern",       laneOffset + 1, pattern01          );
    emit UpdateBoolean ("boolEDPatternInvert", laneOffset + 1, (invert01 != 0x00) );
    emit UpdateBoolean ("boolEDEnable",        laneOffset + 1, (enable01 != 0x00) );

    emit ListSelect    ("listEDPattern",       laneOffset + 3, pattern23          );
    emit UpdateBoolean ("boolEDPatternInvert", laneOffset + 3, (invert23 != 0x00) );
    emit UpdateBoolean ("boolEDEnable",        laneOffset + 3, (enable23 != 0x00) );

    return globals::OK;
}



/******** Read Error Counts: ***********************/

/*!
 \brief Get Error Detector Counts
 Runs the Query PRBS Checker Reading macro twice for the specified
 ED lane, retrieving both the bit count and error count.
 Nb: Doesn't stop the error detector, so the two counts won't
 be from exactly the same time point.

 \param edLane  Selects the error detector to read (0 = L01; 1 = L23)
 \param bits    Used to return the bit count.
                Use NULL to skip bit count measurement
 \param errors  Used to return the error count
                Use NULL to skip error count measurement

 \return globals::OK           Success
 \return [Error Code]          Error from hardware/comms functions
*/
int GT1724::getEDCount(int edLane, double *bits, double *errors)
{
#ifdef BERT_ED_DEBUG
    DEBUG_GT1724("GT1724: Get ED Count: ED Lane: " << edLane)
#endif

    uint8_t options;
    uint8_t output[2];
    int result;
    if (bits)
    {
        // Read the bit counter:
        *bits = 0;
        options = (0x01 << 1) | (uint8_t)edLane;
        output[0] = 0; output[1] = 0;
        result = runLongMacro(0x53, &options, 1, output, 2, 1000);
        if (result != globals::OK) return result;
        // Convert output data to a double:
        *bits = edBytesToDouble(output);
#ifdef BERT_ED_EXTRA_DEBUG
        uint16_t value = (uint16_t)(output[0] << 8) | output[1];
        uint16_t exp   = value >> 10;
        uint16_t mant  = value & 0x03FF;
        double fValue = (double)mant  *  pow( 2.0, (double)exp );
        DEBUG_GT1724(" -Read Bit count OK:")
        DEBUG_GT1724(QString("  Raw Data: [0x%1][0x%2];  Exp:[0x%3] Mant:[0x%4] -> [0x%5]; Recalcd: %6; Calcd: %7")
                    .arg(output[0],2,16,QChar('0'))
                    .arg(output[1],2,16,QChar('0'))
                    .arg(exp,4,16,QChar('0'))
                    .arg(mant,4,16,QChar('0'))
                    .arg(value,4,16,QChar('0'))
                    .arg(fValue)
                    .arg(*bits))
#endif
    }
    if (errors)
    {
        *errors = 0;
        // Read the number of errors:
        options = (uint8_t)edLane;
        output[0] = 0; output[1] = 0;
        result = runLongMacro(0x53, &options, 1, output, 2, 1000);
        if (result != globals::OK) return result;
        // Convert output data to a double:
        *errors = edBytesToDouble(output);
#ifdef BERT_ED_EXTRA_DEBUG
        uint16_t value = (uint16_t)(output[0] << 8) | output[1];
        uint16_t exp   = value >> 10;
        uint16_t mant  = value & 0x03FF;
        double fValue = (double)mant  *  pow( 2.0, (double)exp );
        DEBUG_GT1724(" -Read Err count OK:")
        DEBUG_GT1724(QString("  Raw Data: [0x%1][0x%2];  Exp:[0x%3] Mant:[0x%4] -> [0x%5]; Recalcd: %6; Calcd: %7")
                    .arg(output[0],2,16,QChar('0'))
                    .arg(output[1],2,16,QChar('0'))
                    .arg(exp,4,16,QChar('0'))
                    .arg(mant,4,16,QChar('0'))
                    .arg(value,4,16,QChar('0'))
                    .arg(fValue)
                    .arg(*errors))
#endif
    }
    return globals::OK;
}
// SLOT for reading ED error and bit counters
// On success, emits EDCount signal with counts for the specified lane
// Nb: DOESN'T emit "Result".
// lane should be an ED input lane, i.e. 1 / 3 / 5 / 7 / etc
// Nb: Reads one lane at a time from ED, so doesn't support "ALL_LANES"
void GT1724::GetEDCount(int lane, double bitRate)
{
    LANE_FILTER(lane);
    int edLane = (LANE_MOD(lane)-1) / 2;
    Q_ASSERT(edLane == 0 || edLane == 1);

    DEBUG_GT1724("GT1724 (" << this << "): GetEDCount for lane " << lane << "; edLane " << edLane)

    if (edLane < 0 || edLane > 1) return;

    edParameters_t *ed;
    if (edLane == 0) ed = &ed01;  // Lane 0/1 counts requested.
    else             ed = &ed23;  // Lane 2/3 counts requested.
    if (!ed->edRunning)
    {
        return; // Lane not enabled. No point getting ED counts.
     }
    // Don't measure errors if not locked:
    if (ed->los || ed->lol)
    {
        emit EDCount(lane, false, 0.0, 0.0, 0.0, 0.0);
        return;
    }
    // Calculate emapsed time since last measurement on this channel:
    // Note: "edRunTime->elapsed()" returns the number of milliseconds
    // since edRunTime timer started; BUT it wraps back to 0 every 24 hours
    // (see QT docs). We need to check whether it's gone "backwards":
    int elapsedNow = ed->edRunTime->elapsed();
    int timeDiffMs;
    if (elapsedNow < ed->lastMeasureTimeMs)
    {
        // Elapsed timer has wrapped around: Nb 86,400,000 ms in 24 hours.
        timeDiffMs = (86400000 - ed->lastMeasureTimeMs) + elapsedNow;
    }
    else
    {
        timeDiffMs = elapsedNow - ed->lastMeasureTimeMs;
    }
    ed->lastMeasureTimeMs = elapsedNow;

    double currentBits = 0.0;
    double currentErrors = 0.0;

    /* Bit Count Estimation System:
     * The GT1724 part doesn't ACTUALLY count bits; it just estimates the bit count
     * based on elapsed time, assuming a constant bit rate (around 25.78Gb/s).
     * We can do better: We estimate the bit rate ourselves, using time and also
     * the ACTUAL bit rate (known from clock input).
     * If BERT_REAL_BIT_COUNT is defined, use the GT1724 bit count instead.
     * Nb: Couters return the TOTAL since the ED was started!
     */
#ifdef BERT_REAL_BIT_COUNT
    // Get error count and bit count from GT1724 macro:
    result = getEDCount(edLane, &currentBits, &currentErrors);
    // Multiply the error and bit counts by 2, since ED only checks every OTHER bit.
    currentBits *= 2.0;
    currentErrors *= 2.0;
#else
    int result = getEDCount(edLane, NULL, &currentErrors);
    // Multiply the error count by 2, since ED only checks every OTHER bit.
    currentErrors *= 2.0;
    // Estimate bit count from run time and bit rate instead.
    currentBits = ed->bitsTotal + ( ( (double)timeDiffMs * bitRate ) / 1000 );
#endif

    if (result != globals::OK)
    {
        DEBUG_GT1724("GT1724: GetEDCount: Error reading bit / error counter for lane " << lane << " (" << result << ")")
        emit ShowMessage("Error reading bit / error counts.");
        return;
    }

    // Calculate CHANGE in bit and error counts: this reading - last reading:
    double deltaBits, deltaErrors;
    deltaBits   = currentBits - ed->bitsTotal;
    deltaErrors = currentErrors - ed->errorsTotal;
    if (deltaBits < 0) deltaBits = 0;      // ?? Sanity check
    if (deltaErrors < 0) deltaErrors = 0;  //

    // UPDATE the running bit and error counters with the new values:
    ed->bitsTotal = currentBits;
    ed->errorsTotal = currentErrors;

    // Emit an ED Count signal with the results:
    emit EDCount(lane, true, deltaBits, currentBits, deltaErrors, currentErrors);
}


/*!
 \brief Convert 2 bytes of data read from PRBS checker into a double
 Data are in 16 bit floating point format as described in
 GT1724 Data Sheet.

 \param bytes  Array of two bytes, as read from PRBS checker
               with the "Query PRBS Checker Reading" macro.
               Bytes in the array should be in the same order
               as the bytes returned by the macro.
 \return double  Converted value
*/
double GT1724::edBytesToDouble(const uint8_t bytes[2])
{
    uint16_t mantissa = ( (uint16_t)(bytes[0] & 0x03) << 8 ) | bytes[1];
    uint16_t exponent = (uint16_t)(bytes[0] >> 2);
    return (double)mantissa  *  pow( 2.0, (double)exponent );
}




/*!
 \brief ED Error Inject Slot
        Deliberately generates errors on an ED lane
 \param lane  Lane to inject errors on. This should be
              one of the ED Input lanes, i.e. 1 / 3 / 5 / 7 etc.
*/
void GT1724::EDErrorInject(int lane)
{
    LANE_FILTER(lane);
    DEBUG_GT1724("GT1724: EDErrorInject lane " << lane)
    /**** Polarity Invert Method: *************/
    bool curentInvert = getLaneInverted(LANE_MOD(lane), false);
    setLaneInverted(LANE_MOD(lane), !curentInvert);
    setLaneInverted(LANE_MOD(lane),  curentInvert);
}


/******** LOS and LOL ****************************************/

/*!
 \brief Set the pd_los_all bit and los_det_enable bits to enable or disable LOS
        Note: Sets pd_los_all bit for ALL lanes!
 \param state  0 = Disable LOS and power down
               1 = Enable and power up
 \return globals::OK        Success
 \return globals::OVERFLOW  Input parameter out of allowed range
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::setLosEnable(uint8_t state)
{
    if (state > 1) return globals::OVERFLOW;
    DEBUG_GT1724("GT1724: Setting GT1724 " << laneOffset << " LOS Enable to " << state)
    const uint8_t BIT_PD = (state == 0) ? 1 : 0;  // pd_los_all bit is opposite of state setting.
    uint8_t data;
    int result, lane;
    for (lane=0; lane<4; lane++)
    {
        // ---- pd_los_all Bit (in pd_reg_1): ---------------
        // Get current register contents (Nb: From docs, shouldn't change reserved bits):
        data = 0;
        getRegister(LANE_MOD(lane), GTREG_PD_REG_1, &data);
        data = (data & 0xFE) |   // Use mask to clear bit 0 (preserve other bits)
               (BIT_PD);         // Add new pd_los_all bit.
        result = setRegister(LANE_MOD(lane), GTREG_PD_REG_1, data);
        if (result != globals::OK) return result;
    }
    // ---- los_det_enable Bit (in los_reg_0): ---------------
    data = 0;
    getRegister16(GTREG_LOS_REG_0, &data);
    data = (data & 0xFC) |   // Use mask to clear bit 1,0 (preserve other bits)
           (state);          // Add new los_det_enable bit. (Nb: lp_mode bit always 0).
    result = setRegister16(GTREG_LOS_REG_0, data);
    if (result != globals::OK) return result;
    return globals::OK;
}

/*!
 \brief Get the LOS and LOL latched register values for all lanes
 This method reads the register to get the current value, then writes
 0xFF to the register to clear all latches. (??)
 \param los  OUT: Pointer to an array of uint8_t values used to return results.
             index 0 = Lane 0 LOS value, index 1 = Lane 1 LOS value, etc
 \param lol  OUT: Pointer to an array of uint8_t values used to return results.
             index 0 = Lane 0 LOL value, index 1 = Lane 1 LOL value, etc
 \return globals::OK        Success
 \return [Error Code]       Error from hardware/comms functions
*/
int GT1724::getLosLol(uint8_t los[4], uint8_t lol[4])
{
    uint8_t data = 0;
    int result;
#ifdef BERT_ED_EXTRA_DEBUG
    DEBUG_GT1724("GT1724: Reading LOS and LOL for Core " << laneOffset)
#endif
    result = getRegister16(GTREG_LOSL_OUTPUT, &data);
    if (result != globals::OK) return result;
    int lane;
    for (lane=0; lane<4; lane++)
    {
        los[lane] = (data >> lane)     & 0x01;
        lol[lane] = (data >> (lane+4)) & 0x01;
    }
    // Only if using latched version:
    //return setRegister16(GTREG_LOSL_OUTPUT_LATCHED, 0xFF);

#ifdef BERT_ED_EXTRA_DEBUG
    DEBUG_GT1724("LOS " << laneOffset << ": [" << los[3] << "][" << los[2] << "][" << los[1] << "][" << los[0] << "]")
    DEBUG_GT1724("LOL " << laneOffset << ": [" << lol[3] << "][" << lol[2] << "][" << lol[1] << "][" << lol[0] << "]")
#endif
    return globals::OK;
}
// SLOT: Get LOS and LOL indicators for this chip, and emit "EDLosLol" signals.
// NB: This DOES NOT emit a "Result" signal.
// NB: '1' in LOS and LOL data indicates LOSS of signal or lock for the corresponding lane.
//     'true' = 1 in the generated signals (LOSS of signal / lock)
void GT1724::GetLosLol(int metaLane)
{
    LANE_FILTER(metaLane);
    uint8_t los[4] = { 0 };
    uint8_t lol[4] = { 0 };
    int result = getLosLol(los, lol);
    if (result == globals::OK)
    {
        emit EDLosLol(laneOffset + 1, (los[1] != 0), (lol[1] != 0));
        emit EDLosLol(laneOffset + 3, (los[3] != 0), (lol[3] != 0));

        // Update the internal flags which record LOS / LOL status for each ED lane:
        ed01.los = (los[1] != 0);  ed01.lol = (lol[1] != 0);
        ed23.los = (los[3] != 0);  ed23.lol = (lol[3] != 0);

    }
    else
    {
        emit ShowMessage("Error getting signal status data.");
    }
}







int GT1724::debugPowerStatus(int lane)
{
    int result;
    uint8_t data;
    DEBUG_GT1724("GT1724: Lane Power Debug: Core " << laneOffset << "; Lane " << lane)

    result = getRegister(LANE_MOD(lane), 0, &data);
    DEBUG_GT1724(" -cdr_reg_0 (0) : " << result << ": " << QString("0x%1").arg(data,2,16,QChar('0')))

    result = getRegister(LANE_MOD(lane), 50, &data);
    DEBUG_GT1724(" -drv_reg_0 (50): " << result << ": " << QString("0x%1").arg(data,2,16,QChar('0')))

    result = getRegister(LANE_MOD(lane), 60, &data);
    DEBUG_GT1724(" -pd_reg_0  (60): " << result << ": " << QString("0x%1").arg(data,2,16,QChar('0')))

    result = getRegister(LANE_MOD(lane), 61, &data);
    DEBUG_GT1724(" -pd_reg_1  (61): " << result << ": " << QString("0x%1").arg(data,2,16,QChar('0')))

    result = getRegister(LANE_MOD(lane), 63, &data);
    DEBUG_GT1724(" -pd_reg_3  (63): " << result << ": " << QString("0x%1").arg(data,2,16,QChar('0')))

    result = getRegister(LANE_MOD(lane), 66, &data);
    DEBUG_GT1724(" -pd_reg_6  (66): " << result << ": " << QString("0x%1").arg(data,2,16,QChar('0')))

    return result;
}




/*!
 \brief Load a hex file from disk and convert to binary array
 Uses Intel hex file format

 NOTE: This is a very simple implementation of a hex file reader.
 It is designed to support the macro file used by the GT1724;
 It DOESN'T fully implement all HEX file features!

 \return globals::OK              Macros downloaded OK.
 \return globals::NOT_CONNECTED   Error (comms not open)
 \return globals::FILE_ERROR      Error reading macro file
 \return globals::WRITE_ERROR     Write error sending command / data to USB-I2C module
 \return globals::READ_ERROR      Read error - Error reading data back from USB-I2C module
 \return globals::OVERFLOW        Error (> 64 data bytes in a line)

*/
int GT1724::downloadHexFile()
{
    QString fileName;
    size_t fileIndex;
    size_t fileLines;
    int result;
    bool isOpen = false;
    QFile hexFile;
    QString useMacroVersion;

#ifdef BERT_MACRO_VERSION
    // If the build defines a default macro version to use, search for that file in the resources:
    useMacroVersion = QString(XSTR(BERT_MACRO_VERSION));
    DEBUG_GT1724("GT1724: Looking for Macro Version: " << useMacroVersion)
#else
    // No default macro version. Use the first file we find from the list in globals::MACRO_FILES.
    useMacroVersion = QString("");
#endif

    for (fileIndex=0; fileIndex<globals::N_MACRO_FILES; fileIndex++)
    {
        if (useMacroVersion != "" && useMacroVersion != globals::MACRO_FILES[fileIndex].macroVersionString) continue;   // Not interested in this macro version!

        fileName = QDir( globals::getAppPath() ).absoluteFilePath(globals::MACRO_FILES[fileIndex].hexFileName);
        fileLines = globals::MACRO_FILES[fileIndex].lineCount;
        DEBUG_GT1724("GT1724: Hex File Download...")
        DEBUG_GT1724("Opening file: " << fileName)
        hexFile.setFileName(fileName);
        isOpen = hexFile.open(QIODevice::ReadOnly | QIODevice::Text);
        if (isOpen)
        {
            DEBUG_GT1724("HEX File Opened.")
            break;
        }
        else DEBUG_GT1724("File not found. Trying next. ")
    }
    if (!isOpen)
    {
        DEBUG_GT1724("WARNING: No macro file found!")
        return globals::FILE_ERROR; // No HEX file found!
    }

    int lineNo = 0;
    bool bytesOK;
    bool lineOK;
    uint8_t nBytes;
    int offset;
    uint8_t dataByte;
    uint8_t addressHi;
    uint8_t addressLo;

    uint8_t lineData[256];   // The lazy way to buffer line data (max hex file record length is 255)...
    size_t totalBytes = 0;
    int percent = 0;
    int lastPercent = 0;

    emit ShowMessage(QString("Downloading utilities macro (Core %1)... ").arg(coreNumber));

    while (!hexFile.atEnd()) {
        QByteArray hexLine = hexFile.readLine();
        lineNo++;
        if ( (hexLine.size() < 9) ||
             (hexLine[0] != ':')  ) {
            DEBUG_GT1724("WARNING: Skipped line " << lineNo << " (bad format)")
            continue;
        }

        bytesOK = ( hexCharsToInt( hexLine[1], hexLine[2], &nBytes    ) &
                    hexCharsToInt( hexLine[3], hexLine[4], &addressHi ) &
                    hexCharsToInt( hexLine[5], hexLine[6], &addressLo ) );
        if (!bytesOK) {
            DEBUG_GT1724("WARNING: Skipped line " << lineNo << " (bad byte count or address)")
            continue;
        }
        if (nBytes == 0) {
            continue;
        }
        totalBytes += nBytes;
        lineOK = TRUE;
        offset = 0;
        uint16_t i;
        for (i=9; i<((nBytes*2)+9); i+=2) {
            if ( i > (hexLine.size()-2) ) {
                // Not enough data fields in line!
                DEBUG_GT1724("WARNING: Skipped line " << lineNo << " (Not enough data fields)")
                lineOK = FALSE;
                break;
            }
            bytesOK = hexCharsToInt( hexLine[i], hexLine[i+1], &dataByte);
            if (!bytesOK) {
                DEBUG_GT1724("WARNING: Skipped line " << lineNo << " (bad character found)")
                lineOK = FALSE;
                break;
            }
            lineData[offset] = dataByte;
            offset++;   // Added a byte from the record!
        }  // [for...]

        // Line read. Send to each board:
        if (lineOK)
        {
            result = rawWrite24(0xFB,
                                addressHi,
                                addressLo,
                                lineData,
                                nBytes );
            if (result != globals::OK)
            {
                hexFile.close();
                return result;  // I2C command error!
            }
        }

        // Update status %:
        percent = ( (lineNo*100) / fileLines);
        if (percent >= (lastPercent + 20) )
        {
            lastPercent += 20;
            emit ShowMessage(QString("%1%... ").arg(lastPercent,2,10,QChar('0')), true);
        }

    }  //  [while (!hexFile.atEnd())]

    DEBUG_GT1724(lineNo << " lines read! (" << totalBytes << " bytes).")
    // Finished reading!
    hexFile.close();
    return globals::OK;
}

uint8_t GT1724::hexCharToInt(uint8_t byte)
{
    if (  (byte < 48) ||
          ( (byte > 57) && (byte < 65) ) ||
          (byte > 90)  ) return 255;
    if (byte < 58) return byte - 48;   // Digit 0-9
    return byte - 55;                  // Letter A-Z
}

bool GT1724::hexCharsToInt(uint8_t charHi, uint8_t charLo, uint8_t *result )
{
    uint8_t byteHi = hexCharToInt(charHi);
    if (byteHi > 15) return FALSE;
    uint8_t byteLo = hexCharToInt(charLo);
    if (byteLo > 15) return FALSE;
    *result = (byteHi << 4) + byteLo;
    return TRUE;
}




/*****************************************************************************************
   Run Macros
 *****************************************************************************************/


// MACRO to handle error running macro:
// NOTE: No retry for Macro 18 (it normally
// fails if macros haven't been loaded yet;
// This is used to check whether they have
// been loaded already).
#define MACROERROR_RETRY(ECODE) {                      \
        if (code == 0x18)                              \
        {                                              \
            result = ECODE;                            \
            goto macroFinished;                        \
        }                                              \
        globals::sleep(500);                           \
        errorCounter++;                                \
        if (errorCounter >= 4)                         \
        {                                              \
            result = ECODE;                            \
            goto macroFinished;                        \
        }                                              \
        else                                           \
        {                                              \
            DEBUG_GT1724("  -->RETRY MACRO...")        \
            comms->reset();                            \
            continue;                                  \
        }                                              \
    }


/*!
 \brief Execute a macro

 \param comms       Pointer to I2CComms object to use
 \param i2cAddress  I2C address of IC
 \param code        Macro ID code (see board documentation)
 \param dataIn      Pointer to a buffer containing input parameters for macro.
                    If macro doesn't use any input parameters, set to NULL and
                    set dataInSize to 0.
 \param dataInSize  Number of bytes in dataIn, which will be copied to the
                    macro input buffer. Max. 16 bytes; If > 16, function will
                    return globals::OVERFLOW.
 \param dataOut     Pointer to a buffer allocated by the CALLER for output
                    data from the macro. If macro doesn't use any output
                    parameters, set to NULL and set dataOutSize to 0.
 \param dataOutSize Number of bytes in dataOut, which will be read from the
                    macro output buffer. Max. 16 bytes; If > 16, function will
                    return globals::OVERFLOW.
 \param timeoutMs   Timeout in mS. If the macro doesn't complete and return a
                    result (OK or Error) in this time, TIMEOUT is returned.

 \return globals::OK              Success
 \return globals::NOT_CONNECTED   No connection to BERT
 \return globals::OVERFLOW        dataInSize or dataOutSize too big (>16 bytes)
 \return globals::BUSY_ERROR      Operation already in progress
 \return globals::GEN_ERROR       Read / Write error
 \return globals::TIMEOUT         Timeout waiting for data read/write or macro completion
 \return globals::MACRO_ERROR     The macro completed, but with an "error" result
 \return [Error code]             Error code from BertComms->read or BertComms->write
*/
int GT1724::runMacroStatic(I2CComms      *comms,
                           const uint8_t  i2cAddress,
                           const uint8_t  code,
                           const uint8_t *dataIn,
                           const uint8_t  dataInSize,
                           uint8_t       *dataOut,
                           const uint8_t  dataOutSize,
                           const uint16_t timeoutMs )
{
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;
    if ( (dataInSize > 16) || (dataOutSize > 16) ) return globals::OVERFLOW;

    int result = globals::OK;
    int pollCount;

    int errorCounter = 0;
    while (TRUE)
    {
#ifdef BERT_MACRO_DEBUG
        DEBUG_GT1724(QString("[Macro I2C 0x%1] {0x%2}[0x%3][0x%4]")
                      .arg((int)i2cAddress,2,16,QChar('0'))
                      .arg((int)code,2,16,QChar('0'))
                      .arg((int)( (dataInSize>0) ? dataIn[0] : 0 ),2,16,QChar('0'))
                      .arg((int)( (dataInSize>1) ? dataIn[1] : 0 ),2,16,QChar('0')))
#endif
        // ***** If input data specified, write to the macro input buffer: **********
        // if (dataInSize > 0) DEBUG_GT1724("  Write Input Data...")
        if (dataInSize > 0)  result = comms->write(i2cAddress, 0x0C00, dataIn, dataInSize);
        if (result != globals::OK) goto macroFinished;  // I2C command error
        // if (dataInSize > 0) DEBUG_GT1724("  -->OK")

        // ***** Write the Macro code to the Macro register (this starts macro): *******
        // DEBUG_GT1724("  Write Macro Code: {%1}").arg((int)(code),2,16,QChar('0') )
        result = comms->write(i2cAddress, 0x0C10, &code, 1);
        if (result != globals::OK) goto macroFinished;  // I2C command error
        // DEBUG_GT1724("  -->OK")

        // ***** Poll the macro code register and wait for it to change to 0x00 or 0x01: ******
        // DEBUG_GT1724("  Polling Code/Status Register for result code...")
        uint8_t macroResult;
        pollCount = timeoutMs / 100;  // Note: Polling every 100 mS (see below).
        while (pollCount > 0)
        {
            globals::sleep(100);
            // Read back the Macro code (indicates macro status):
            result = comms->read(i2cAddress, 0x0C10, &macroResult, 1);
            // DEBUG_GT1724("  -->Poll Read Result: " << result)
            if (result != globals::OK) goto macroFinished;  // I2C command error
            // DEBUG_GT1724("  -->Status: " << QString("{%1} (00=OK, 01=Error, xx=Still Running)").arg((int)macroResult,2,16,QChar('0') ))
            if ( (macroResult == 0x00) ||
                 (macroResult == 0x01) ) break;  // Macro finished.
            pollCount--;
        }

#ifdef BERT_MACRO_DEBUG
        // ------ DEBUG --------
        if (pollCount == 0)      DEBUG_GT1724("  -->Macro TIMEOUT!")
        if (macroResult == 0x01) DEBUG_GT1724("  -->Macro ERROR!")
        // ---------------------
#endif

        if (pollCount == 0)
        {
            //result = globals::TIMEOUT;      // Timeout! Macro didn't finish, or returned error.
            //goto macroFinished;
            MACROERROR_RETRY(globals::TIMEOUT)
        }
        if (macroResult == 0x01)
        {
            //result = globals::MACRO_ERROR;  // Macro error
            //goto macroFinished;
            MACROERROR_RETRY(globals::MACRO_ERROR)
        }
        // DEBUG_GT1724("  -->Macro Finished OK")

        // ***** If output data requested, read from the macro output buffer: ********
        if (dataOutSize > 0)
        {
            // DEBUG_GT1724("  Reading back macro output...")
            result = comms->read(i2cAddress, 0x0C11, dataOut, dataOutSize);
            // DEBUG_GT1724("  -->Read Result:" << result << " (0 = Success)")
        }
        break;
    }
macroFinished:
    errorCounter = 0;
    return result;
}


/*!
 \brief Execute a macro with specified timeout
 For parameters and return code, see runMacroStatic
*/
int GT1724::runLongMacro(const uint8_t  code,
                         const uint8_t *dataIn,
                         const uint8_t  dataInSize,
                         uint8_t       *dataOut,
                         const uint8_t  dataOutSize,
                         const uint16_t timeoutMs )
{
    return runMacroStatic(comms, i2cAddress, code, dataIn, dataInSize, dataOut, dataOutSize, timeoutMs);
}


/*!
 \brief Execute a macro with 0.5 second timeout
 This method executes a macro with a default timeout of 0.5 seconds.
 For parameters and return code, see runLongMacro
*/
int GT1724::runMacro(const uint8_t  code,
                     const uint8_t *dataIn,
                     const uint8_t  dataInSize,
                     uint8_t       *dataOut,
                     const uint8_t  dataOutSize )
{
    return runLongMacro(code, dataIn, dataInSize, dataOut, dataOutSize, 800);
}


/*!
 \brief Execute a simple macro
 Runs a macro which uses only 1 byte of data and doesn't return any data.
 See runMacro for more info.
*/
int GT1724::runSimpleMacro(const uint8_t code, const uint8_t dataIn)
{
    return runLongMacro(code, &dataIn, 1, NULL, 0, 800);
}


/*!
 \brief Execute a very simple macro
 Runs a macro which uses no data and doesn't return any data.
 See runMacro for more info.
*/
int GT1724::runVerySimpleMacro(const uint8_t code)
{
    return runLongMacro(code, NULL, 0, NULL, 0, 800);
}






/*****************************************************************************************
   Get / Set Registers
 *****************************************************************************************/

/*!
 \brief Set a register
 \param lane    Lane select - 0 to 3
 \param address Register address (low byte)
 \param data    Data to write to the register
 \return globals::OK              Success
 \return globals::BUSY_ERROR      Comms already in use
 \return globals::NOT_CONNECTED   No connection to BERT
 \return globals::GEN_ERROR       Read / Write error
*/
int GT1724::setRegister(int lane, const uint8_t address, const uint8_t data )
{
    Q_ASSERT(lane >= 0 && lane < 4);
    int result;
    result = comms->write(i2cAddress, ((uint16_t)lane << 8) + (uint16_t)address, &data, 1);
    DEBUG_REG("[Register WRITE]: Lane: " << (int)lane << " Addr: " << (int)address << " Data: " << (int)data << " Result: " << result)
    return result;
}



/*!
 \brief Get the value of a register
 \param lane    Lane select - 0 to 3
                Used as the upper byte of the register address ('page').
 \param address Register address (low byte)
 \param data    Pointer to a uint8_t which will be set to the register
                value on success.
 \return globals::OK              Success
 \return globals::BUSY_ERROR      Comms already in use
 \return globals::NOT_CONNECTED   No connection to BERT
 \return globals::GEN_ERROR       Read / Write error
 \return globals::TIMEOUT         Timeout waiting for data read
*/
int GT1724::getRegister(int lane, const uint8_t address, uint8_t *data )
{
    Q_ASSERT(lane >= 0 && lane < 4);
    int result = comms->read(i2cAddress, ((uint16_t)lane << 8) + (uint16_t)address, data, 1);
    DEBUG_REG("[Register READ]: Lane: " << (int)lane << " Addr: " << (int)address << " Data: " << (int)*data << " Result: " << result)
    return result;
}



/*!
 \brief Set a register (16 bit address)
 \param address Register address (16 bit)
 \param data    Data to write to the register
 \return globals::OK              Success
 \return globals::BUSY_ERROR      Comms already in use
 \return globals::NOT_CONNECTED   No connection to BERT
 \return globals::GEN_ERROR       Read / Write error
*/
int GT1724::setRegister16(const uint16_t address, const uint8_t data )
{
    int result = comms->write(i2cAddress, address, &data, 1);
    DEBUG_REG("[Register WRITE]: Addr (16 bit): " << (int)address << " Data: " << (int)data << " Result: " << result)
    return result;
}


/*!
 \brief Get the value of a register (16 bit address)
 \param address Register address (16 bit)
 \param data    Pointer to a uint8_t which will be set to the register
                value on success.
 \return globals::OK              Success
 \return globals::BUSY_ERROR      Comms already in use
 \return globals::NOT_CONNECTED   No connection to BERT
 \return globals::GEN_ERROR          Read / Write error
 \return globals::TIMEOUT         Timeout waiting for data read
*/
int GT1724::getRegister16(const uint16_t address, uint8_t *data )
{
    int result = comms->read(i2cAddress, address, data, 1);
    DEBUG_REG("[Register READ]: Addr (16 bit): " << (int)address << " Data: " << (int)*data << " Result: " << result)
    return result;
}






/********************************************************************
  Raw memory write to GT1724
  Used to send macro update data
 ********************************************************************/

/*!
 \brief Write raw data to device memory, using a 24 bit address

 \param address   16 bit address of memory to write to
 \param data      Pointer to an array of uint8_t containing data to write (may be only one byte)
 \param nBytes    Number of bytes to write from data.
                  Nb: nBytes is not limited in size - any write size
                  restrictions will be managed in comms class.

 \return globals::OK            Data written
 \return [Error Code]           See BertComms->write24
*/
int GT1724::rawWrite24(const uint8_t addressHi,
                       const uint8_t addressMid,
                       const uint8_t addressLow,
                       const uint8_t *data,
                       const uint8_t nBytes )
{
    uint32_t address = ((uint32_t)addressHi << 16) |
                       ((uint32_t)addressMid << 8) |
                       ((uint32_t)addressLow);
    int result = comms->write24(i2cAddress,
                                address,
                                data,
                                nBytes);
    return result;
}





/*!
 \brief Read raw data from device memory, using a 24 bit address

 \param addressHi   Upper byte of address (MSB)
 \param addressMid  Middle byte of address
 \param addressLow  Lower bite of address (LSB)

 \param data      Pointer to an array of uint8_t to store read data
 \param nBytes    Number of bytes to read. Data array must be large enough.
                  Nb: nBytes is not limited in size - any read size
                  restrictions will be managed in comms class.

 \return globals::OK            Data read
 \return [Error Code]           See BertComms->read24
*/
int GT1724::rawRead24(const uint8_t addressHi,
                      const uint8_t addressMid,
                      const uint8_t addressLow,
                      uint8_t *data,
                      const size_t nBytes )
{
    uint32_t address = ((uint32_t)addressHi << 16) |
                       ((uint32_t)addressMid << 8) |
                       ((uint32_t)addressLow);
    int result = comms->read24(i2cAddress,
                               address,
                               data,
                               nBytes );
    return result;
}



// ==============================================================================
// Eye Scanning
// ==============================================================================

// **** Slots to carry out Eye Scan functons: *************
void GT1724::EyeScanStart(int lane, int type, int hStep, int vStep, int vOffset, int countRes)
{
    LANE_FILTER(lane);
    DEBUG_GT1724("GT1724: (" << this << ") Eye Scan START request for lane " << lane)
    int modLane = LANE_MOD(lane);
    Q_ASSERT(modLane == 1 || modLane == 3);
    if (modLane != 1 && modLane != 3) return;  // Invalid lane.
    EyeMonitor *em;
    if (modLane == 1) em = eyeMonitor01;
    else              em = eyeMonitor23;

    em->startScan(type, hStep, vStep, vOffset, countRes);
}

void GT1724::EyeScanRepeat(int lane)
{
    LANE_FILTER(lane);
    DEBUG_GT1724("GT1724: (" << this << ") Eye Scan REPEAT request for lane " << lane)
    int modLane = LANE_MOD(lane);
    Q_ASSERT(modLane == 1 || modLane == 3);
    if (modLane != 1 && modLane != 3) return;  // Invalid lane.
    if (modLane == 1) eyeMonitor01->repeatScan();
    else              eyeMonitor23->repeatScan();
}

void GT1724::EyeScanCancel(int lane)
{
    if (lane == globals::ALL_LANES || LANE_MOD(lane) == 0)
    {
        // Cancel ALL eye scans on this chip:
        DEBUG_GT1724("GT1724: (" << this << ") Eye Scan CANCEL request for all lanes")
        eyeMonitor01->cancelScan();
        eyeMonitor23->cancelScan();
    }
    else
    {
        LANE_FILTER(lane);
        DEBUG_GT1724("GT1724: (" << this << ") Eye Scan CANCEL request for lane " << lane)
        int modLane = LANE_MOD(lane);
        Q_ASSERT(modLane == 1 || modLane == 3);
        if (modLane != 1 && modLane != 3) return;  // Invalid lane.
        if (modLane == 1) eyeMonitor01->cancelScan();
        else              eyeMonitor23->cancelScan();
    }
}

// ******* Emit signals on behalf of the Eye Scan module: *********

void GT1724::emitEyeScanProgressUpdate(int lane, int type, int percent)                        { emit EyeScanProgressUpdate(lane, type, percent);    }
void GT1724::emitEyeScanError(int lane, int type, int code)                                    { emit EyeScanError(lane, type, code);                }
void GT1724::emitEyeScanFinished(int lane, int type, QVector<double> data, int xRes, int yRes) { emit EyeScanFinished(lane, type, data, xRes, yRes); }

// *** Eye scanner - Call 'processEvents' to check for EyeScanCancel signal:
void GT1724::eyeScanCheckForCancel()
{
    QCoreApplication::processEvents();
}



//==============================================================================
//  Lists and Look-up tables for Selectable Items
//==============================================================================


// -- Amplitudes: ----------------------
// This look up table maps the INDEX of items in the "Amplitude"
// list to voltage swing value for get / set Output Swing
// Nb: This is the register setting, i.e. mV / 5:
const QList<int> GT1724::PG_OUTPUT_SWING_LOOKUP =
    { 40, 60, 80, 100, 120, 140, 160, 180, 200, 220 };
const QStringList GT1724::PG_OUTPUT_SWING_LIST =
    { "200 mV", "300 mV", "400 mV", "500 mV", "600 mV", "700 mV", "800 mV", "900 mV", "1 V", "1.1 V" };

// -- Pattern names: --------------------
const QStringList GT1724::PG_PATTERN_LIST =
  { "PRBS9", "PRBS15", "PRBS31", "Div. by 1 ratio", "Div. by 2 ratio", "Div. by 4 ratio", "Div. by 8 ratio", "Div. by 32 ratio" };


// -- De-Emphasis Levels: ----------------
const QStringList GT1724::PG_EQ_DEEMPH_LIST =
    { "0 dB", "0.3 dB", "0.7 dB", "1.1 dB", "1.5 dB", "1.9 dB", "2.4 dB", "2.8 dB", "3.5 dB", "4.0 dB", "4.6 dB", "5.2 dB", "5.9 dB", "6.6 dB", "7.4 dB", "8.2 dB" };


// -- EQ Pre / Post Cursor selection: -----
const QStringList GT1724::PG_EQ_CURSOR_LIST = { "Post-Cursor", "Pre-Cursor" };


// -- Cross Point Lookup: -------------
// This look-up table maps the index of items in the "Cross Point"
// combo box to a "cross point adjust" register setting (see GT1274 data sheet section 6)
const QList<int> GT1724::PG_CROSS_POINT_LOOKUP =
    { 9, 0, 3, 6, 12, 15 };
const QStringList GT1724::PG_CROSS_POINT_LIST =
    { "50%", "35%", "40%", "45%", "55%" };


// -- ED Pattern Select (ED Page): -----------
const QStringList GT1724::ED_PATTERN_LIST =
    { "PRBS 9", "PRBS 15", "PRBS 31" };
const int GT1724::ED_PATTERN_DEFAULT = 2;


// -- EQ Boost Lookup (ED Page): ---------------
const QList<int> GT1724::ED_EQ_BOOST_LOOKUP =
    { 0, 40, 59, 74, 87, 96, 105, 112, 118, 123, 127 };
const QStringList GT1724::ED_EQ_BOOST_LIST =
    { "0dB", "1.2dB", "2.4dB", "3.6dB", "4.8dB", "6.0dB", "7.2dB", "8.4dB", "9.6dB", "10.8dB", "12.0dB" };


// -- H-Step and V-Step for Eye Scan: -------------
const QList<int> GT1724::EYESCAN_VHSTEP_LOOKUP = { 1, 2, 4, 8 };
const QStringList GT1724::EYESCAN_VHSTEP_LIST = { "1", "2", "4", "8" };
const int GT1724::EYESCAN_VHSTEP_DEFAULT = 1;

// -- V Offset for Bathtub Scan: ----------------
const QList<int>  GT1724::EYESCAN_VOFF_LOOKUP =
    { 127, 115, 103, 90, 77, 65, 52, 39, 26, 14, 1 };
const QStringList GT1724::EYESCAN_VOFF_LIST =
    { "250 mV", "200 mV", "150 mV", "100 mV", "50 mV", "0 mV", "-50 mV", "-100 mV", "-150 mV", "-200 mV", "-250 mV" };
const int GT1724::EYESCAN_VOFF_DEFAULT = 5;

// --- Count Resolution for Eye scan / Bathtub scan: ------
const QStringList GT1724::EYESCAN_COUNTRES_LIST =
    { "1-Bit", "2-Bit", "4-Bit", "8-Bit" };
const int GT1724::EYESCAN_COUNTRES_DEFAULT = 3;


// --- Options for Force CDR Bypass: ---
const QStringList GT1724::CDR_BYPASS_OPTIONS_LIST =
    { "Off", "On", "Auto" };
const int GT1724::CDR_BYPASS_OPTIONS_DEFAULT = 2;  // Default to "Auto"

// --- Options for CDR Mode Freq Divider list: ---
const QStringList GT1724::CDR_FREQDIV_OPTIONS_LIST =
    { "1/2", "1/4", "1/8", "1/16", "1/64", "1/256" };
const int GT1724::CDR_FREQDIV_OPTIONS_DEFAULT = 0;
