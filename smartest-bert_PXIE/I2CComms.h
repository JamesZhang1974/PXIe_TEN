/*!
 \file   I2CComms.h
 \brief  I2C Comms Class Header
         This class provides I2C comms via a USB to I2C Adaptor (P18F14K50-I/SS)
         The adaptor appears in the host system as a virtual serial port.
         Various I2C operations are implemented by sending commands to the port.

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#ifndef I2CCOMMS_H
#define I2CCOMMS_H

#include <memory>
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QThread>

#include "globals.h"
#include "Serial.h"

class I2CCommsWorker;

/*!
 \brief I2C Comms Class
*/
class I2CComms : public QObject
  {
  Q_OBJECT

  public:
    I2CComms();
    virtual ~I2CComms();

    static std::unique_ptr<QStringList> getPortList();

    int   open(const QString port);  // E.g.: "COM1"
    void  close();
    void  reset();
    bool  portIsOpen();
    int   pingAddress(const uint8_t slaveAddress);

    int   writeRaw(const uint8_t slaveAddress,
                   const uint8_t *data,
                   const uint8_t nBytes);

    int   write8(const uint8_t slaveAddress,
                 const uint8_t regAddress,
                 const uint8_t *data,
                 const uint8_t nBytes);

    int   write(const uint8_t slaveAddress,
                const uint16_t regAddress,
                const uint8_t *data,
                const uint8_t nBytes);

    int   write24(const uint8_t slaveAddress,
                  const uint32_t regAddress,
                  const uint8_t *data,
                  const size_t nBytes);


    int   readRaw(const uint8_t slaveAddress,
                  uint8_t *data,
                  const uint8_t nBytes);

    int   read8(const uint8_t slaveAddress,
                const uint8_t regAddress,
                uint8_t *data,
                const uint8_t nBytes);

    int   read(const uint8_t slaveAddress,
               const uint16_t regAddress,
               uint8_t *data,
               const uint8_t nBytes);

    int   read24(const uint8_t slaveAddress,
                 const uint32_t regAddress,
                 uint8_t *data,
                 const size_t nBytes);

signals:
    void I2CWorkerConnect(QString port);
    void I2CWorkerDisconnect();
    void I2CProbeAdaptor();
    void I2CConfigureAdaptor();
    void I2CWorkerOp(int nBytesToWrite,
                     const char *dataWrite,
                     int nBytesToRead,
                     char *dataRead);
    void I2CClearPort();
    void I2CWorkerExit();

private:
    void commsClose();
    int  i2cOp(const uint8_t  nBytesToWrite,
               const uint8_t *dataWrite,
               const uint8_t  nBytesToRead,
                     uint8_t *dataRead);

    static const int MAX_RETRIES   = 5;    // Maximum number of times to retry on I2C Error

    // Adaptor Command Bytes:
    static const uint8_t I2C_SGL = 0x53;   // Read/Write single byte for non-registered devices
    static const uint8_t I2C_AD0 = 0x54;   // Read/Write multiple bytes without address
    static const uint8_t I2C_AD1 = 0x55;   // Read/Write single or multiple bytes for 1 byte addressed devices
    static const uint8_t I2C_AD2 = 0x56;   // Read/Write single or multiple bytes for 2 byte addressed devices
    static const uint8_t I2C_DIR = 0x57;   // Build custom I2C sequences
    static const uint8_t I2C_TST = 0x58;   // Check for the existence of an I2C device on the bus
    static const uint8_t ISS_CMD = 0x5A;   // Custom commands for the USB-ISS adaptor

    bool isOpen;
    std::unique_ptr<I2CCommsWorker> commsWorker;

};


class I2CCommsWorker : public QThread
{
    Q_OBJECT

public:
    I2CCommsWorker();
    ~I2CCommsWorker();

    int getStatus() const { return commsStatus; }
    int getLastResult() const { return lastResult; }

    // Comms Status:
    static const int COMMS_OK    =  0;
    static const int COMMS_ERROR = -1;
    static const int COMMS_BUSY  = -2;

public slots:
    void I2CWorkerConnect(QString port);
    void I2CWorkerDisconnect();
    void I2CProbeAdaptor();
    void I2CConfigureAdaptor();
    void I2CWorkerOp(int nBytesToWrite,
                     const char *dataWrite,
                     int nBytesToRead,
                     char *dataRead);
    void I2CClearPort();
    void I2CWorkerExit();

    void transactionFinished();

private slots:

    void serialTimeout();

private:
    static const int COMMS_TIMEOUT = 50;   // Maximum time when reading data back from serial transaction (mS)

    static const int I2COP_SLEEP_TIME = 3;           // Delay (mS) after I2C op to allow adaptor to reset: 5 found to be reliable.
    static const int I2COP_ERR_RECOVERY_TIME = 100;  // Delay (mS) after I2C error condition

    // Pre-defined I2C adaptor ops:
    static const uint8_t ISS_CMD = 0x5A;   // Custom commands for the USB-ISS adaptor
    static const uint8_t I2C_OP_GET_VERSION[];
    static const uint8_t I2C_OP_GET_VERSION_SIZE;

    static const uint8_t I2C_OP_GET_SERIAL[];
    static const uint8_t I2C_OP_GET_SERIAL_SIZE;

    static const uint8_t I2C_OP_SET_MODE[];
    static const uint8_t I2C_OP_SET_MODE_SIZE;


    // Expected I2C Adaptor Version Info:
    static const uint8_t I2C_ADAPTOR_VERSION[];

    void run();

    int commsStatus = COMMS_OK;
    int lastResult = globals::OK;
    bool flagStop;

    std::unique_ptr<Serial> serial;
    std::unique_ptr<QTimer> serialTimer;


};


#endif // I2CCOMMS_H
