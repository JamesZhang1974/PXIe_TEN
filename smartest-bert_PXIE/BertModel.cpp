/*!
 \file   BertModel.cpp
 \brief  Model-specific Constants Class Implementation
 \author J Cole-Baker (For Smartest)
 \date   Mar 2019
*/

#include "BertModel.h"
#include <QDebug>

const QString BertModel::BUILD_VERSION = QString( "3.4.5" );
const QString BertModel::BUILD_DATE = __DATE__ " " __TIME__;

// Component I2C Addresses:
// Each board contains:
//   1 or 2 x GT1725 (or compatible) BERT cores
//       (Pixie models may be 1 or 2 cores; Smartest module has 2 cores)
//   1 x LMX2594 clock synthesizer (connected via SC18IS602 I2C to SPI bridge)
//   2 x PCA9557 I/O expander
//
// When a slave is connected, components appear as if connected to the master,
// except with translated I2C addresses.

// Available model codes (for displaying list of factory options to write to EEPROM)
// Supported Instrument Models:
//  Code                #GT1724 Cores  #PGs    #EDs   Slave Board Option?
//  NONE                   0            0       0      No
//  SB3204C                2            2       2      Yes
//  SB3204C_FS             2            2       2      Yes - Faked, for testing
//  PPG3204D_PIXIE         1            4       0      No
//  SB3202D_PIXIE          1            2       2      No
const QStringList BertModel::BERT_MODELS =
{
    "Select...",      // Place Holder

    "NONE",           // No model selected (for testing)

    "SB3204C",        // Generic Smartest model
    "SB3204C_FS",     // (With fake slave board)

    "PPG3204D_PIXIE", // CS / Pixie Models
    "SB3202D_PIXIE"

    // OLD / Not supported: "PPG3204C"
};


const QStringList BertModel::BERT_FIRMWARES =
{
    "Select...",      // Place Holder

    "NONE",           // No model selected (for testing)

    "SB3204C",        // Generic Smartest model

    "PPG3204D_PIXIE", // CS / Pixie Models

    "SB3202D_PIXIE",

    "PIXIE_TEN"
};


// Defaults:
QString BertModel::modelCode = "Unknown";

// For all models: Define only ONE EEPROM IC, base address 0x50:
QList<uint8_t> BertModel::i2cAddresses_M24M02   = { 0x50 };  // If slave board were present, second EEPROM would appear at 0x54.

// Other defaults: Assume single board with single GT1724 IC:
QList<uint8_t> BertModel::i2cAddresses_GT1724   = { 0x12 };
QList<uint8_t> BertModel::i2cAddresses_LMX2594  = { 0x28 };
QList<uint8_t> BertModel::i2cAddresses_PCA9557B  = { 0x1E };
QList<uint8_t> BertModel::i2cAddresses_PCA9557A  = { 0x1C };
QList<uint8_t> BertModel::i2cAddresses_SI5340   = { 0x76 };
QList<uint8_t> BertModel::i2cAddresses_TLC59108   = { 0x47 };

bool BertModel::fourChanPGMode = true;       // No ED channels in 'NONE' mode!

bool BertModel::showCDRBypassSelect = false;  // Show the "CDR Bypass" Selector on PG page?

/*!
 \brief Select Bert Model based on model code
  Checks the supplied model code (string) to see if it matches
  a known model.
  If so, the UI is set up for the specified model, and true is returned.
  If no match is found, "NONE" is selected as the model (disables all
  PG / ED UI), and false is returned.

 \param code  Model code to test. This is a string matching one of the codes
              defined by BertModel::BERT_MODELS
 \return true  Model found
 \return false Model not found; 'NONE' selected.

*/
bool BertModel::SelectModel(const QString &code)
{
    if (code == "SB3204C")
    {
        // ---- Dual GT1724 Board with SLAVE option: ---------------------------------------
        modelCode = "SB3204C";
        i2cAddresses_GT1724   = { 0x12, 0x14, 0x16, 0x10 };
        i2cAddresses_LMX2594  = { 0x28,       0x2C       };
        i2cAddresses_PCA9557A  = { 0x1C,       0x18       };
        i2cAddresses_SI5340   = { 0x76,       0x72       };
        fourChanPGMode = false;
        qDebug() << "Selected Model: " << modelCode;
        return true;
    }

    if (code == "SB3204C_FS")
    {
        // ---- TEST / Smartest Dual GT1724 Board with Fake Slave: --------------------------------
        modelCode = "SB3204C_FS";
        i2cAddresses_GT1724   = { 0x12, 0x14, 0x12, 0x14 };
        i2cAddresses_LMX2594  = { 0x28,       0x28       };
        i2cAddresses_PCA9557A  = { 0x1C,       0x1C       };
        i2cAddresses_SI5340   = { 0x76,       0x76       };
        fourChanPGMode = false;
        qDebug() << "Selected Model: " << modelCode;
        return true;
    }

    if (code == "PPG3204D_PIXIE")
    {
        modelCode = "PPG3204D_PIXIE";
        i2cAddresses_GT1724   = { 0x12 };
        i2cAddresses_LMX2594  = { 0x28 };
        i2cAddresses_PCA9557B  = { 0x1E };
        i2cAddresses_PCA9557A  = { 0x1C };
        i2cAddresses_SI5340   = { 0x76 };
        i2cAddresses_TLC59108  = { 0x47 };
        fourChanPGMode = true;
        qDebug() << "Selected Model: " << modelCode;
        return true;
    }

    if (code == "SB3202D_PIXIE")
    {
        modelCode = "SB3202D_PIXIE";
        i2cAddresses_GT1724   = { 0x12 };
        i2cAddresses_LMX2594  = { 0x28 };
        i2cAddresses_PCA9557B  = { 0x1E };
        i2cAddresses_PCA9557A  = { 0x1C };
        i2cAddresses_SI5340   = { 0x76 };
         i2cAddresses_TLC59108  = { 0x47 };
        fourChanPGMode = false;
        qDebug() << "Selected Model: " << modelCode;
        return true;
    }

    // Select "NONE" model:
    modelCode = "Unknown";
    i2cAddresses_GT1724   = { 0x12 };
    i2cAddresses_LMX2594  = { 0x28 };
    i2cAddresses_PCA9557B  = { 0x1E };
    i2cAddresses_PCA9557A  = { 0x1C };
    i2cAddresses_SI5340   = { 0x76 };
    fourChanPGMode = true;      // No ED channels in 'NONE' mode!

    if (code != "NONE") qDebug() << "Unrecognised model code: " << code;
    qDebug() << "Selected Model: NONE";

    return false;   // Haven't selected a usable model.
}


// DEPRECATED

// Define the following for SLAVE TEST mode: This simulates a master and slave using only
// a master, by dupicating master I2C addresses:
//   #define I2C_ADDRESS_TEST

// Define the following to force a PIXIE board (no slave board allowed):
//   #define I2C_ADDRESS_PIXIE

// Define the following to force a PIXIE board with ONE GT1724 core:
//   #define I2C_ADDRESS_PIXIE_SINGLE

// DEPRECATED

// Define the following to implement a 4 channel PG with each GT1724 core
// (Default is 2 x PG, 2 x ED).
// If set up as UseFourChanPGMode(), UI will display 4 PG channels for each
// core, and all ED controls and tabs will be disabled.
