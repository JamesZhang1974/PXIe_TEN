/*!
 \file   globals.cpp
 \brief  Global Constants Class Implementation
 \author J Cole-Baker (For Smartest)
 \date   Jan 2015
*/

#include "globals.h"

// Macro file information:
const globals::MacroFileInfo globals::MACRO_FILES[] =
{
    // [File Name],    [Line Count], [Version (byte array)], [Version (String)]
    { ":/UNKNOWN.hex",           0,   { 0x00, 0x00, 0x00, 0x00 }, "Unknown" },   // Placeholder for unrecognised macro version
    { ":/MACRO_VER_1_E_0_C.hex", 309, { 0x01, 0x45, 0x00, 0x43 }, "1E0C"    },
    { ":/MACRO_VER_1_E_1_C.hex", 317, { 0x01, 0x45, 0x01, 0x43 }, "1E1C"    }
};
const size_t globals::N_MACRO_FILES = sizeof(MACRO_FILES) / sizeof(MACRO_FILES[0]);

const double globals::BELOW_DETECTION_LIMIT = -999999.0;

const QString globals::FACTORY_KEY_HASH = QString("BAo3yhbilJzwY8VJ9C2KbFjCwRBJlGJyTrOUDfMyW1ZYd9YJR5Je5SCg8pF9aP1QXrzeOLORI3vVjI6MzonbwqkVvAzuVathmNia");

//const QString globals::FACTORY_KEY_HASH = QString("77feacb4228cb24a8cfd372f2a7d6d920052f48f38f5d6b84e99350a094aaba3");

QString globals::appPath = QString("");

void globals::setAppPath(QString path)
{
    globals::appPath = QString(path);
}

QString globals::getAppPath()
{
    return globals::appPath;
}

