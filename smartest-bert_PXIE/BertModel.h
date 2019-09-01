/*!
 \file   BertModel.h
 \brief  Model-specific Constants Class Header
 \author J Cole-Baker (For Smartest)
 \date   Mar 2019
*/

#ifndef BERTMODEL_H
#define BERTMODEL_H

#include <stdint.h>
#include <QStringList>
#include <QList>
#include <QString>

class BertModel
{
public:

    static bool SelectModel(const QString &code);

    // Available model codes (for displaying list of factory options to write to EEPROM)
    static const QStringList BERT_MODELS;
    static const QStringList BERT_FIRMWARES;

    static const QString &GetModelCode() { return modelCode; }

    // Component I2C Addresses:
    static const QList<uint8_t> &GetI2CAddresses_GT1724()   { return i2cAddresses_GT1724;  }
    static const QList<uint8_t> &GetI2CAddresses_LMX2594()  { return i2cAddresses_LMX2594; }
    static const QList<uint8_t> &GetI2CAddresses_PCA9557B()  { return i2cAddresses_PCA9557B; }
    static const QList<uint8_t> &GetI2CAddresses_PCA9557A()  { return i2cAddresses_PCA9557A; }
    static const QList<uint8_t> &GetI2CAddresses_M24M02()   { return i2cAddresses_M24M02;  }
    static const QList<uint8_t> &GetI2CAddresses_SI5340()   { return i2cAddresses_SI5340;  }
    static const QList<uint8_t> &GetI2CAddresses_TLC59108()   { return i2cAddresses_TLC59108;  }

    // Optional features
    static bool UseFourChanPGMode() { return fourChanPGMode; }
    static bool ShowCDRBypassSelect() { return showCDRBypassSelect; }

    static const QString BUILD_VERSION;
    static const QString BUILD_DATE;

private:

    // Selected Model
    static QString modelCode;

    // Component I2C Addresses:
    static QList<uint8_t> i2cAddresses_GT1724;  // GetI2CAddresses_GT1724;
    static QList<uint8_t> i2cAddresses_LMX2594;
    static QList<uint8_t> i2cAddresses_PCA9557B;
    static QList<uint8_t> i2cAddresses_PCA9557A;
    static QList<uint8_t> i2cAddresses_M24M02;
    static QList<uint8_t> i2cAddresses_SI5340;
    static QList<uint8_t> i2cAddresses_TLC59108;

    // Optional features
    static bool fourChanPGMode;
    static bool showCDRBypassSelect;

};

#endif // BERTMODEL_H
