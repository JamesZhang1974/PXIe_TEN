/*!
 \file   globals.h
 \brief  Global Constants Class Header
 \author J Cole-Baker (For Smartest)
 \date   Jan 2015
*/

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QSize>

// Some macro string expansion magic...
#define STR(s) #s
#define XSTR(s) STR(s)

// Useful formatting macros
#define INT_AS_HEX(N,PLACES)  QString("0x%1").arg(N, PLACES, 16, QChar('0'))


/*!
 \brief Global Constants Class - Defines static global constants
*/
class globals
{
public:

    // Information about a macro file (.hex format) for the GTXXXX chip:
    struct MacroFileInfo
    {
        QString hexFileName;           // File name, used to load the file; E.g. ":/MACRO_VER_1_E_0_C.hex" (":/..." to load from resources section of binary)
        int     lineCount;             // Number of useful lines in the file (makes parsing more efficient)
        QChar   macroVersion[4];       // Four bytes representing the macro version (as read back from the GTxxxx IC); used to check whether the macro downloaded OK
        QString macroVersionString;    // String representation of the macro version, e.g. "1E0C"
    };

    static const MacroFileInfo MACRO_FILES[];
    static const size_t N_MACRO_FILES;

    /****** Error/Status Codes *******/
    static const int OK                  =  0;    // No Error - Operation completed sucessfully
    static const int GEN_ERROR           = -1;    // General error
    static const int TIMEOUT             = -2;    // Timeout
    static const int OVERFLOW            = -3;    // Data overflow or input data out of range
    static const int NOT_CONNECTED       = -4;    // No connection to board
    static const int MACRO_ERROR         = -5;    // A Macro on the board completed, but with an error
    static const int READ_ERROR          = -6;    // Error reading from serial port
    static const int WRITE_ERROR         = -7;    // Error writing to serial port
    static const int FILE_ERROR          = -8;    // Couldn't open / read file (e.g. extension macro file)
    static const int BAD_LANE_ID         = -9;    // 'Lane' parameter wasn't 0-3 or 5.
    static const int WRITE_TIMEOUT       = -10;   // Timeout waiting for bytes to be written to adaptor
    static const int WRITE_CONF_TIMEOUT  = -11;   // Timeout waiting write confirmation from adaptor
    static const int READ_TIMEOUT        = -12;   // Timeout waiting for bytes to be read from adaptor
    static const int ADAPTOR_READ_ERROR  = -13;   // Adaptor returned internal error code after I2C read
    static const int ADAPTOR_WRITE_ERROR = -14;   // Adaptor returned internal error code after I2C write
    static const int MALLOC_ERROR        = -15;   // Couldn't allocate memory
    static const int BUSY_ERROR          = -16;   // Comms were busy (operation already in progress)
    static const int NOT_INITIALISED     = -17;   // Initialise process has not been carried out, or it failed
    static const int DIRECTORY_NOT_FOUND = -18;   // Specified directory didn't exist on the file system
    static const int INVALID_BOARD       = -19;   // Specified board index doesn't exist (e.g. tried to run a macro on slave board, but this is a single board system)
    static const int DEVICE_NOT_FOUND    = -20;   // Couldn't detect any I2C device on the specified I2C address
    static const int INVALID_DATA        = -21;   // Bad text data or no data to parse, or bad data read from EEPROM
    static const int END_OF_DATA         = -22;   // No more data to read (e.g. from list of items in EEPROM)
    static const int BAD_CHECKSUM        = -23;   // Checksum from EEPROM record didn't match checksum of data in record
    static const int UNKNOWN_MODEL       = -24;   // Couldn't read model code form EEPROM

    static const int MISSING_GT1724      = -50;   // Couldn't detect any GT1724 IC in the system
    static const int MISSING_LMX         = -51;   // Couldn't detect an LMX clock module
    static const int MISSING_LMX_DEFS    = -52;   // Couldn't find any register definition files for LMX clock module
    static const int MISSING_PCA9557B    = -53;   // Couldn't detect a PCA9557 IO controller module
    static const int MISSING_EEPROM      = -54;   // Couldn't detect an M24M02 EEPROM
    static const int MISSING_PCA9557A    = -55;   // Couldn't detect a PCA9557A IO controller module
    static const int MISSING_TLC59108    = -56;   // Couldn't detect a TLC59108 LED module


    static const int NOT_IMPLEMENTED     = -99;   // Feature or method not implemented on this hardware

    // Status Codes:
    static const int READY               = -100;   // Operation ready to start or finished
    static const int IN_PROGRESS         = -101;   // Operation currently in progress
    static const int CANCELLED           = -102;   // Operation was cancelled and didn't finish
    static const int MACROS_LOADED       = -103;   // Macro hex file already downloaded
    static const int MACROS_NOT_LOADED   = -104;   // Macro hex file NOT downloaded yet

    static const int ALL_LANES = -1;  // Use to specify ALL lanes, or where no lane is required.

    static const double BELOW_DETECTION_LIMIT;   // Placeholder for values which are below the "floor" of the bathtub plot

    static const QString FACTORY_KEY_HASH;

    /*!
     \brief General purpose sleep method
     \param milliSeconds  Number of milliseconds to sleep for
    */
    static void sleep(unsigned int milliSeconds) { Sleep(static_cast<DWORD>(milliSeconds)); }


    /*!
      \brief App Path: Path to the directory where the executable is located
             Must be SET from the main window constructor (or similar) before use.
     */
    static void setAppPath(QString path);
    static QString getAppPath();


private:
    static QString appPath;

};

// ****** Debug Output Control: *******************
// See project file PG3204.pro



#endif // GLOBALS_H

