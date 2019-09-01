/*!
 \file   I2CComms.cpp
 \brief  I2C Comms Class Implementation
 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <memory>
#include <QDebug>
#include <QTimer>
#include <QStringList>
#include <QtSerialPort/QSerialPortInfo>

#include "globals.h"
#include "Serial.h"

#include "I2CComms.h"


// Debug Macro for Comms:
// #define BERT_I2C_DEBUG
// #define BERT_I2C_EXTRA_DEBUG
#ifdef BERT_I2C_DEBUG
  #define DEBUG_I2C(MSG) qDebug() << "\t\t\t\t" << MSG;
  #ifdef BERT_I2C_EXTRA_DEBUG
    #define DEBUG_I2C_EXTRA(MSG) qDebug() << "\t\t\t\t" << MSG;
  #endif
#endif

#ifndef DEBUG_I2C
  #define DEBUG_I2C(MSG)
#endif
#ifndef DEBUG_I2C_EXTRA
  #define DEBUG_I2C_EXTRA(MSG)
#endif



// I2C Slave Address Macros: Add the appropriate
// read / write bit to a 7 bit slave address:
#define I2CWRITE(A) (uint8_t)(A << 1)       // Make 7 bit address into Master Write
#define I2CREAD(A)  (uint8_t)((A << 1) + 1) // Make 7 bit address into Master Read

// MACRO to handle comms error:
#define COMMSERROR_RETRY(ECODE) {                      \
        emit I2CClearPort();                           \
        errorCounter++;                                \
        if (errorCounter >= MAX_RETRIES)               \
        {                                              \
            return(ECODE);                             \
        }                                              \
        qDebug() << "\t\t\t\t   -->RETRY...";          \
        continue;                                      \
        }


/*
 Nb: For comms via USB-ISS INTERFACE ADAPTER, see:
   http://www.robot-electronics.co.uk/htm/usb_iss_tech.htm
*/

I2CComms::I2CComms()
{
    DEBUG_I2C("I2CComms: Constructor")
    isOpen = false;

    // Start the I2C worker thread:
    DEBUG_I2C("Creating I2CCommsWorker FROM thread " << QThread::currentThreadId())
    commsWorker = std::unique_ptr<I2CCommsWorker>(new I2CCommsWorker());
    commsWorker.get()->moveToThread(commsWorker.get());

    connect(this, SIGNAL(I2CWorkerConnect(QString)), commsWorker.get(), SLOT(I2CWorkerConnect(QString)), Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(I2CWorkerDisconnect()),     commsWorker.get(), SLOT(I2CWorkerDisconnect()),     Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(I2CProbeAdaptor()),         commsWorker.get(), SLOT(I2CProbeAdaptor()),         Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(I2CConfigureAdaptor()),     commsWorker.get(), SLOT(I2CConfigureAdaptor()),     Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(I2CWorkerOp(int, const char *, int, char *)),
                                                     commsWorker.get(), SLOT(I2CWorkerOp(int, const char *, int, char *)),
                                                                                                         Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(I2CClearPort()),            commsWorker.get(), SLOT(I2CClearPort()),            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(I2CWorkerExit()),           commsWorker.get(), SLOT(I2CWorkerExit()),           Qt::BlockingQueuedConnection);

    commsWorker->start();
}

I2CComms::~I2CComms()
{
    commsClose();

    // Shut down the comms wrker:
    emit I2CWorkerExit();
}


/*!
 \brief Get a list of available serial ports on the host system
 \return Pointer to new string list containing a list of serial ports on the
         host system. These port names can be passed to 'open'.
*/
std::unique_ptr<QStringList> I2CComms::getPortList()
{
    DEBUG_I2C("I2CComms: Get list of serial ports...")
    std::unique_ptr<QStringList> portList(new QStringList());
    QList<QSerialPortInfo>portInfoList;
    portInfoList = QSerialPortInfo::availablePorts();
    DEBUG_I2C("I2CComms: " << portInfoList.count() << " ports found.")
    QSerialPortInfo portInfo;
    foreach (portInfo, portInfoList)
    {
        DEBUG_I2C_EXTRA("Serial Port: description: '" << portInfo.description() << "'; "
                        << "portName: '" << portInfo.portName() << "'; "
                        << "serialNumber: '" << portInfo.serialNumber() << "'; "
                        << "systemLocation: '" << portInfo.systemLocation() << "'")
        portList->append(portInfo.portName());
    }
    return portList;
}


/*!
 \brief Open comms
 Creates a serial class (Serial.cpp) and calls
 the 'open' method to open comms.

 Also sets up a QEventLoop to allow comms methods to wait
 for serial port operations to finish. The BertSerial
 "transactionFinished" signal is connected to our slot so that
 we get notified when the serial port has finished an operation.

 After opening the serial port, the I2C adaptor is probed
 to make sure it is responding (see probeAdaptor).

 \param port  QString containing the name of the serial port to use

 \return globals::OK         Success. Comms open.
 \return globals::GEN_ERROR  Error. Comms not open; calls to
                             read and write will be ignored.
*/
int I2CComms::open(const QString port)
{
    DEBUG_I2C("I2CComms: OPEN")
    isOpen = false;
    commsClose();  // In case the comms were already open.

    emit I2CWorkerConnect(port);
    if (commsWorker->getLastResult() == globals::OK) emit I2CProbeAdaptor();

    if (commsWorker->getLastResult() != globals::OK)
    {
        DEBUG_I2C("Error: I2C Adaptor didn't respond!")
        commsClose();
        return globals::GEN_ERROR;
    }
    emit I2CConfigureAdaptor();
    isOpen = true;
    return globals::OK;
}


/*!
 \brief Close comms
 This method closes the comms if open.
*/
void I2CComms::close()
{
    DEBUG_I2C("I2CComms: CLOSE")
    commsClose();
}




/*!
 \brief Reset I2C Adaptor
 Used after error
 \return
*/
void I2CComms::reset()
{
    DEBUG_I2C("I2CComms: RESET")
    emit I2CClearPort();
    globals::sleep(600);
    emit I2CProbeAdaptor(); // Hopefully this will clear the adaptor's serial buffer.
}


/*!
 \brief Is the serial port is open?
 \return true  Port is open
 \return false Port NOT open
*/
bool I2CComms::portIsOpen()
{
   return isOpen;
}


/*!
 \brief Ping an I2C Slave Address to see whether there is a device present
        Looks for the "ACK" I2C signal from the device.
 \param slaveAddress
 \return globals::OK                Device responded
 \return globals::DEVICE_NOT_FOUND  No response on that I2C address
 \return [error code]               Comms error or comms not connected
*/
int I2CComms::pingAddress(const uint8_t slaveAddress)
{
    DEBUG_I2C("I2CComms: Ping Address " << INT_AS_HEX(slaveAddress,2))
    int errorCounter = 0;
    uint8_t i2cData[2];  // Buffer for data to be sent
    i2cData[0] = I2C_TST;
    i2cData[1] = I2CWRITE(slaveAddress);
    uint8_t adaptorResponse;
    int result;
    while (true)
    {
        result = i2cOp(sizeof(i2cData),
                       i2cData,
                       1,
                       &adaptorResponse);
        if (result != globals::OK) COMMSERROR_RETRY(result)

        if (adaptorResponse == 0x00)
        {

            DEBUG_I2C("   -->Response code 0x00: I2C adaptor reports No ACK (Device not found).")
            errorCounter = 0;
            return globals::DEVICE_NOT_FOUND;
        }
        else
        {
            DEBUG_I2C("   -->Response code " << (int)adaptorResponse << ": I2C adaptor reports ACK (Device found).")
            errorCounter = 0;
            return globals::OK;
        }
    }
    return globals::ADAPTOR_WRITE_ERROR;
}



/*!
 \brief Write raw bytes out I2C device
 This is used for a device without any internal address.
 Max 255 bytes per write.
 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param data          Pointer to an array of uint8_t data bytes (0 to 255 bytes)
 \param nBytes        Number of bytes to write from data (0 to 255 bytes).
 \return globals::OK              Success - nBytes were read back into data buffer
 \return globals::NOT_CONNECTED   Error (comms not open)
 \return globals::OVERFLOW        Error (nBytes > 255)
 \return [error code]             Error from i2cOp
*/
int I2CComms::writeRaw(const uint8_t  slaveAddress,
                       const uint8_t *data,
                       const uint8_t  nBytes)
{
    DEBUG_I2C("I2CComms: WRITE RAW")
    int errorCounter = 0;
    uint8_t i2cData[nBytes+3];  // Buffer for data to be sent, plus header
    i2cData[0] = I2C_AD0;
    i2cData[1] = I2CWRITE(slaveAddress);
    i2cData[2] = nBytes;
    if (nBytes > 0) memcpy(i2cData + 3, data, nBytes);
    uint8_t adaptorResponse;
    int result;
    while (true)
    {
        result = i2cOp(sizeof(i2cData),
                       i2cData,
                       1,
                       &adaptorResponse);
        if (result != globals::OK) COMMSERROR_RETRY(result)
                if (adaptorResponse == 0x00)
        {

            DEBUG_I2C("   -->Response code 0x00: I2C adaptor reports write failed!")
            COMMSERROR_RETRY(globals::ADAPTOR_WRITE_ERROR)
        }
            errorCounter = 0;
        return globals::OK;
    }
    return globals::ADAPTOR_WRITE_ERROR;
}



/*!
 \brief Write data to I2C Device (8 bit address)

 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param regAddress 8 bit address of register or memory to write to
 \param data       Pointer to an array of uint8_t data bytes (may be only one)
 \param nBytes     Number of bytes to write from data.
                   nBytes must be <= 59, otherwise 'OVERFLOW' error will be returned.

 \return globals::OK                   Data written
 \return globals::NOT_CONNECTED        Error (comms not open)
 \return globals::OVERFLOW             Data too big (nBytes > 59)
 \return globals::ADAPTOR_WRITE_ERROR  Adaptor returned internal fault code
 \return [error code]                  Error from i2cOp
*/
int I2CComms::write8(const uint8_t slaveAddress,
                     const uint8_t regAddress,
                     const uint8_t *data,
                     const uint8_t nBytes)
{
    DEBUG_I2C("I2CComms: WRITE (8 Bit Address)")
    Q_ASSERT(nBytes <= 59);
    if (nBytes > 59) return globals::OVERFLOW;       // Transmission buffer is limited to 59 bytes.
    // PROFILING: runTime.start();
    int errorCounter = 0;
    uint8_t i2cData[nBytes+4];  // Buffer for data to be sent, plus header
    i2cData[0] = I2C_AD1;
    i2cData[1] = I2CWRITE(slaveAddress);
    i2cData[2] = regAddress;
    i2cData[3] = nBytes;
    memcpy(i2cData + 4, data, nBytes);
    uint8_t adaptorResponse;
    int result;
    while (true)
    {
        DEBUG_I2C_EXTRA("   Write to I2C Device " << QString("0x%1").arg((int)(slaveAddress),2,16,QChar('0'))
                        << " (8 bit address): "
                        << QString("[0x%1]; %2 bytes")
                        .arg((int)(regAddress),2,16,QChar('0'))
                        .arg((int)nBytes))
        result = i2cOp(sizeof(i2cData),
                       i2cData,
                       1,
                       &adaptorResponse);
        if (result != globals::OK) COMMSERROR_RETRY(result)
                if (adaptorResponse == 0x00)
        {
            DEBUG_I2C("   -->Response code 0x00: I2C adaptor reports write failed!")
            COMMSERROR_RETRY(globals::ADAPTOR_WRITE_ERROR)
        }
            errorCounter = 0;
        return globals::OK;
    }
    return globals::ADAPTOR_WRITE_ERROR;
}







/*!
 \brief Write data to I2C Device (16 bit address)

 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param regAddress 16 bit address of register or memory to write to
 \param data       Pointer to an array of uint8_t data bytes (may be only one)
 \param nBytes     Number of bytes to write from data.
                   nBytes must be <= 59, otherwise 'OVERFLOW' error will be returned.

 \return globals::OK                   Data written
 \return globals::NOT_CONNECTED        Error (comms not open)
 \return globals::OVERFLOW             Data too big (nBytes > 59)
 \return globals::ADAPTOR_WRITE_ERROR  Adaptor returned internal fault code
 \return [error code]                  Error from i2cOp
*/
int I2CComms::write(const uint8_t slaveAddress,
                    const uint16_t regAddress,
                    const uint8_t *data,
                    const uint8_t nBytes)
{
    DEBUG_I2C("I2CComms: WRITE (16 Bit Address)")
    Q_ASSERT(nBytes <= 59);
    if (nBytes > 59) return globals::OVERFLOW;       // Transmission buffer is limited to 59 bytes.
    // PROFILING: runTime.start();
    int errorCounter = 0;
    uint8_t i2cData[nBytes+5];  // Buffer for data to be sent, plus header
    i2cData[0] = I2C_AD2;
    i2cData[1] = I2CWRITE(slaveAddress);
    i2cData[2] = (uint8_t)(regAddress >> 8);
    i2cData[3] = (uint8_t)(regAddress);
    i2cData[4] = nBytes;
    memcpy(i2cData + 5, data, nBytes);
    uint8_t adaptorResponse;
    int result;
    while (true)
    {
        DEBUG_I2C_EXTRA("   Write to I2C Device " << QString("0x%1").arg((int)(slaveAddress),2,16,QChar('0'))
                        << " (16 bit address): "
                        << QString("[0x%1%2]; %3 bytes")
                        .arg((int)(regAddress >> 8),2,16,QChar('0'))
                        .arg((int)(regAddress & 0xFF),2,16,QChar('0'))
                        .arg((int)nBytes))
        result = i2cOp(sizeof(i2cData),
                       i2cData,
                       1,
                       &adaptorResponse);
        if (result != globals::OK) COMMSERROR_RETRY(result)
                if (adaptorResponse == 0x00)
        {
            DEBUG_I2C("   -->Response code 0x00: I2C adaptor reports write failed!")
            COMMSERROR_RETRY(globals::ADAPTOR_WRITE_ERROR)
        }
            errorCounter = 0;
        return globals::OK;
    }
    return globals::ADAPTOR_WRITE_ERROR;
}





/*!
 \brief Write data to device memory, using a 24 bit address

 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)

 \param regAddress Device register address (32 bit unsigned; upper 8 bits ignored).

 \param data       Pointer to an array of uint8_t - data to write
 \param nBytes     Number of bytes to write
                   Nb: nBytes is not limited in size - any read size
                   restrictions will be managed internally.

 \return globals::OK                  Data written
 \return globals::NOT_CONNECTED       Error (comms not open)
 \return globals::ADAPTOR_WRITE_ERROR Adaptor returned internal fault code

*/
int I2CComms::write24(const uint8_t  slaveAddress,
                      const uint32_t regAddress,
                      const uint8_t *data,
                      const size_t   nBytes)
{
    DEBUG_I2C("I2CComms: WRITE (24 Bit Address)")
    //    qDebug() << "   Write to I2C Device (24 bit address): "
    //             << QString("[0x%1%2%3]; %4 bytes")
    //                .arg((int)((regAddress >> 16) & 0x000000FF),2,16,QChar('0'))
    //                .arg((int)((regAddress >> 8)  & 0x000000FF),2,16,QChar('0'))
    //                .arg((int)(regAddress         & 0x000000FF),2,16,QChar('0'))
    //                .arg((int)nBytes);
    // Use custom I2C command to handle 3 byte address:
    // We add a header with address, etc, followed by
    // a series of 'write' sub commands which can each send
    // 16 bytes (see docs for adaptor). Max size data frame
    // for the adaptor is 60 bytes so we may need to send
    // multiple requests, remembering to update the address
    // we are writing to each time.
    uint8_t  i2cFrame[60];   // Maximum data size the adaptor can handle
    uint32_t writeAddress = regAddress;
    size_t bytesRemaining = nBytes;
    size_t thisFrameSize;
    size_t bytesThisSubFrame;
    size_t frameBytesRemaining;
    int frameWritePtr;
    while (bytesRemaining > 0)
    {
        i2cFrame[0] = I2C_DIR;              // USB-I2C adaptor command (I2C DIRECT)
        i2cFrame[1] = 0x01;                 //   SUB COMMAND: I2C Start
        i2cFrame[2] = 0x33;                 //   SUB COMMAND: Write next 4 bytes
        i2cFrame[3] = I2CWRITE(slaveAddress); // Address of device + R/W bit (=0 for write)
        i2cFrame[4] = writeAddress >> 16;   // Write to address - High byte
        i2cFrame[5] = writeAddress >> 8;    //
        i2cFrame[6] = writeAddress;         //
        frameBytesRemaining = (60 - 7);     // Already used 7 bytes above.
        frameWritePtr = 7;                  // Loop below starts adding more sub-commands and data here.
        // 18 bytes is room for one more 'write' sub-command, plus the
        // 'stop bit' command. If there's fewer than 18 bytes left in the
        // buffer, finish the frame and sent it; Otherwise, add another
        // write sub-command and more data.
        while ( (frameBytesRemaining >= 18) && (bytesRemaining > 0) )
        {
            // Make a sub-frame with the next (up to) 16 bytes:
            bytesThisSubFrame = bytesRemaining;
            if (bytesThisSubFrame > 16) bytesThisSubFrame = 16;
            i2cFrame[frameWritePtr] = (0x30 + bytesThisSubFrame) - 1; //   SUB COMMAND: Write next n bytes
            frameWritePtr++;
            memcpy(i2cFrame + frameWritePtr, data, bytesThisSubFrame);
            bytesRemaining -= bytesThisSubFrame;
            frameBytesRemaining -= (bytesThisSubFrame + 1);
            frameWritePtr += bytesThisSubFrame;
        }
        i2cFrame[frameWritePtr] = 0x03;  // Add I2C STOP
        frameWritePtr++;
        thisFrameSize = frameWritePtr;
        uint8_t adaptorResponse[2] = { 0,0 };
        //qDebug() << "   -Send Frame: " << thisFrameSize << " bytes";
        int result = i2cOp(thisFrameSize,
                           i2cFrame,
                           2,
                           adaptorResponse);
        if (result != globals::OK) return result;
        if (adaptorResponse[0] == 0x00)
        {
            DEBUG_I2C("   -->NACK (0x00): I2C adaptor reports error!")
            DEBUG_I2C("   -->Error Code: " << adaptorResponse[1])
            emit I2CClearPort();
            return globals::ADAPTOR_WRITE_ERROR;
        }
        writeAddress += (nBytes - bytesRemaining);  // Advance the write-to address by the number of bytes we just sent.
    }
    globals::sleep(5);
    return globals::OK;
}










/*!
 \brief Read raw bytes from I2C device
 This is used for a device without any internal address
 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param data          Pointer to an array of uint8_t data bytes (may be only one)
 \param nBytes        Number of bytes to write from data.
 \return globals::OK              Success - nBytes were read back into data buffer
 \return globals::NOT_CONNECTED   Error (comms not open)
 \return globals::OVERFLOW        Error (nBytes > 255)
 \return [error code]             Error from i2cOp
*/
int I2CComms::readRaw(const uint8_t slaveAddress,
                      uint8_t      *data,
                      const uint8_t nBytes)
{
    DEBUG_I2C("I2CComms: READ RAW")
    int errorCounter = 0;
    while (true)
    {
        uint8_t i2cData[] = { I2C_AD0,
                              I2CREAD(slaveAddress),
                              (uint8_t)nBytes };
        int result = i2cOp(sizeof(i2cData),
                           i2cData,
                           nBytes,
                           data);
        if (result != globals::OK) COMMSERROR_RETRY(result)
                errorCounter = 0;
        return globals::OK;
    }
    return globals::ADAPTOR_READ_ERROR;
}



/*!
 \brief Read data from I2C Device (8 bit address)

 Reads bytes from the I2C Device and stores them to the
 supplied buffer.

 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param regAddress 8 bit address of register or memory to read from
 \param data       Pointer to a buffer allocated by the caller.
                   Must be at least nBytes in size.
 \param nBytes     Number of bytes to read from device.
                   nBytes must be <= 64, otherwise 'OVERFLOW' error
                   will be returned.

 \return globals::OK              Success - nBytes were read back into data buffer
 \return globals::NOT_CONNECTED   Error (comms not open)
 \return globals::OVERFLOW        Error (nBytes > 64)
 \return [error code]             Error from i2cOp
*/
int I2CComms::read8(const uint8_t slaveAddress,
                    const uint8_t regAddress,
                    uint8_t *data,
                    const uint8_t nBytes)
{
    DEBUG_I2C("I2CComms: READ")
    Q_ASSERT(nBytes <= 64);
    if (nBytes > 64) return globals::OVERFLOW; // Transmission buffer is limited to 64 bytes.
    int errorCounter = 0;
    while (true)
    {
        DEBUG_I2C_EXTRA("   Read from I2C Device " << QString("0x%1").arg((int)(slaveAddress),2,16,QChar('0'))
                        << " (8 bit address): "
                        << QString("[0x%1]; %2 bytes")
                           .arg((int)(regAddress),2,16,QChar('0'))
                           .arg((int)nBytes))

        uint8_t i2cData[] = { I2C_AD1,
                              I2CREAD(slaveAddress),
                              (uint8_t)regAddress,
                              (uint8_t)nBytes };

        int result = i2cOp(sizeof(i2cData),
                           i2cData,
                           nBytes,
                           data);

        if (result != globals::OK) COMMSERROR_RETRY(result)
                errorCounter = 0;
        return globals::OK;
    }
    return globals::ADAPTOR_READ_ERROR;
}




/*!
 \brief Read data from I2C Device (16 bit address)

 Reads bytes from the I2C Device and stores them to the
 supplied buffer.

 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param regAddress 16 bit address of register or memory to read from
 \param data       Pointer to a buffer allocated by the caller.
                   Must be at least nBytes in size.
 \param nBytes     Number of bytes to read from device.
                   nBytes must be <= 64, otherwise 'OVERFLOW' error
                   will be returned.

 \return globals::OK              Success - nBytes were read back into data buffer
 \return globals::NOT_CONNECTED   Error (comms not open)
 \return globals::OVERFLOW        Error (nBytes > 64)
 \return [error code]             Error from i2cOp
*/
int I2CComms::read(const uint8_t  slaveAddress,
                   const uint16_t regAddress,
                   uint8_t       *data,
                   const uint8_t  nBytes)
{
    DEBUG_I2C("I2CComms: READ")
    Q_ASSERT(nBytes <= 64);
    if (nBytes > 64) return globals::OVERFLOW; // Transmission buffer is limited to 64 bytes.
    int errorCounter = 0;
    while (true)
    {
        DEBUG_I2C_EXTRA("   Read from I2C Device " << QString("0x%1").arg((int)(slaveAddress),2,16,QChar('0'))
                        << " (16 bit address): "
                        << QString("[0x%1%2]; %3 bytes")
                           .arg((int)(regAddress >> 8),2,16,QChar('0'))
                           .arg((int)(regAddress & 0xFF),2,16,QChar('0'))
                           .arg((int)nBytes))

        uint8_t i2cData[] = { I2C_AD2,
                              I2CREAD(slaveAddress),
                              (uint8_t)(regAddress >> 8),
                              (uint8_t)regAddress,
                              (uint8_t)nBytes };

        int result = i2cOp(sizeof(i2cData),
                           i2cData,
                           nBytes,
                           data);

        if (result != globals::OK) COMMSERROR_RETRY(result)
                errorCounter = 0;
        return globals::OK;
    }
    return globals::ADAPTOR_READ_ERROR;
}






/*!
 \brief Read raw data from device memory, using a 24 bit address
 \param slaveAddress  I2C Slave address of the device
                      (NB: 7 bits, i.e. not including R/W bit)
 \param regAddress Device register address (32 bit unsigned; upper 8 bits ignored).
 \param data       Pointer to an array of uint8_t to store read data
 \param nBytes     Number of bytes to read. Data array must be large enough.
                   Nb: nBytes is not limited in size - any read size
                   restrictions will be managed internally.

 \return globals::OK                   Data written
 \return globals::NOT_CONNECTED        Error (comms not open)
 \return globals::ADAPTOR_WRITE_ERROR  Adaptor returned internal fault code
 \return globals::READ_ERROR           Data size returned by adaptor read was wrong
 \return [error code]                  Error from i2cOp
*/
int I2CComms::read24(const uint8_t  slaveAddress,
                     const uint32_t regAddress,
                     uint8_t       *data,
                     const size_t   nBytes )
{
    DEBUG_I2C("I2CComms: READ 24 Bit Address")
    // Use custom I2C command to handle 3 byte address:
    // We add a header with address, etc, followed by
    // a series of 'read' sub commands which can each fetch
    // 16 bytes (see docs for adaptor). Max size data frame
    // for the adaptor is 60 bytes so we may need to send
    // multiple requests, remembering to update the address
    // we are reading from each time.
    uint32_t readAddress;
    size_t bytesTotalRead = 0;
    size_t bytesRemaining = nBytes;
    size_t thisFrameSize;
    size_t bytesRequestedThisRead;

    int errorCounter = 0;
    uint8_t adaptorResponse[18] = { 0 };
    // Nb: Max data per read is 16 bytes, plus 2 byte header from adaptor
    while (bytesTotalRead < nBytes)
    {
        bytesRemaining = nBytes - bytesTotalRead;
        // Number of bytes to read this time: We must leave at least
        // two for the last read operation.
        if (bytesRemaining >= 18)
        {
            bytesRequestedThisRead = 16;
        }
        else
        {
            if (bytesRemaining >= 17) bytesRequestedThisRead = 15;
            else                      bytesRequestedThisRead = bytesRemaining;
        }
        readAddress = regAddress + bytesTotalRead;
        //        qDebug() << "   -->Next frame read from "
        //                 << QString("[0x%1%2%3]")
        //                    .arg((int)((regAddress >> 16) & 0x000000FF),2,16,QChar('0'))
        //                    .arg((int)((regAddress >> 8)  & 0x000000FF),2,16,QChar('0'))
        //                    .arg((int)(regAddress         & 0x000000FF),2,16,QChar('0'));
        uint8_t i2cFrame[] =
        {
            I2C_DIR,     // USB-I2C adaptor command (I2C DIRECT)
            0x01,                   //   SUB COMMAND: I2C Start
            0x33,                   //   SUB COMMAND: Write next 4 bytes
            I2CWRITE(slaveAddress), //   Address of device + R/W bit (=0 for write)
            (uint8_t)(readAddress >> 16),  //   Write to address - High byte
            (uint8_t)(readAddress >> 8),   //
            (uint8_t)(readAddress),        //
            0x02,                   // Add I2C Restart
            0x30,                   //  SUB COMMAND: Write next 1 byte
            I2CREAD(slaveAddress),  // Address of device + R/W bit (=1 for READ)
            (uint8_t)(0x20 + (bytesRequestedThisRead - 2)),  //   SUB COMMAND: Read all but one of requested bytes
            0x04,                   // Add I2C NACK
            (uint8_t)(0x20),        //   SUB COMMAND: Read last byte
            0x03                    // Add I2C STOP
        };
        thisFrameSize = sizeof(i2cFrame);
        //qDebug() << "   -Send Frame: " << thisFrameSize << " bytes; Requests read of " << bytesRequestedThisRead << " bytes";
        int result = i2cOp(thisFrameSize,
                           i2cFrame,
                           bytesRequestedThisRead + 2,
                           adaptorResponse);
        if (result != globals::OK)
        {
            COMMSERROR_RETRY(result)
        }
        if (adaptorResponse[0] == 0x00)
        {
            DEBUG_I2C("   -->NACK (0x00): I2C adaptor reports error!")
            DEBUG_I2C("   -->Error Code: " << adaptorResponse[1])
            COMMSERROR_RETRY(globals::ADAPTOR_WRITE_ERROR)
        }
        memcpy(data + bytesTotalRead, adaptorResponse + 2, bytesRequestedThisRead);
        bytesTotalRead += bytesRequestedThisRead;
        errorCounter = 0;
    }
    return globals::OK;
}



////////////////////////////////////////////////////////////////////////////
//// PRIVATE Methods ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////



/*!
 \brief Close comms, including serial port
*/
void I2CComms::commsClose()
{
    emit I2CWorkerDisconnect();
    isOpen = false;
}



/*!
 \brief Carry out I2C operation with USB-ISS Module

 Output data should be a module command, followed by sub commands
 and data to send - see:
  http://www.robot-electronics.co.uk/htm/usb_iss_tech.htm

 This method writes the output data to the module via the serial
 class.

 It then waits for the "transactionFinished" signal from the
 serial class, which indicates that the expected response data
 has been received. The wait operation is also broken by
 the "commsTimeout" signal. The commsStatus flag is checked
 to see which of these conditions occurred.

 If the comms were successful, the method makes a copy of the
 recieved data and stores it to the buffer supplied by dataRead.

 \param nBytesToWrite  Number of bytes to write from dataWrite
 \param dataWrite      Pointer to output data

 \param nBytesToRead  Number of bytes expected in the response.
                      Nb: ALL I2C transactions must generate at
                      least one response byte.

 \param dataRead      Pointer to input data buffer. Must be at least
                      nBytesToRead bytes in size. Should be NULL if
                      nBytesToRead is 0.

 \return globals::OK               Operation completed successfully.
                                   Nb: This means the data was written out to
                                   the adaptor, and (if required) the correct
                                   number of bytes were read back. Check the
                                   response to see whether the I2C operation
                                   actually worked as expected!

 \return globals::NOT_CONNECTED    Comms not open yet
 \return globals::READ_ERROR       Expected number of bytes were not recevied
*/
int I2CComms::i2cOp(const uint8_t  nBytesToWrite,
                    const uint8_t *dataWrite,
                    const uint8_t  nBytesToRead,
                    uint8_t *dataRead)
{
    if (!isOpen) return globals::NOT_CONNECTED;  // Comms not open!

    DEBUG_I2C("I2CComms: Emitting I2CWorkerOp signal")
    DEBUG_I2C("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv")

    emit I2CWorkerOp((int)nBytesToWrite,
                     (const char *)dataWrite,
                     (int)nBytesToRead,
                     (char *)dataRead);

    DEBUG_I2C("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^")
    DEBUG_I2C("I2CComms: I2CWorkerOp signal returned.")

    return commsWorker->getLastResult();
}











// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////
//   I2C Comms Worker
// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////


// ***** Pre-defined I2C Operations Data Blocks: ****
const uint8_t I2CCommsWorker::I2C_OP_GET_VERSION[] = { ISS_CMD, 0x01 };
const uint8_t I2CCommsWorker::I2C_OP_GET_VERSION_SIZE = sizeof(I2C_OP_GET_VERSION);

// Expected I2C Adaptor Version Info:
// Should be Module ID:  7; FW Version:  7; Mode:  64
const uint8_t I2CCommsWorker::I2C_ADAPTOR_VERSION[] = { 7, 7, 64 };

const uint8_t I2CCommsWorker::I2C_OP_GET_SERIAL[] = { ISS_CMD, 0x03 };
const uint8_t I2CCommsWorker::I2C_OP_GET_SERIAL_SIZE = sizeof(I2C_OP_GET_SERIAL);

#define ISS_IO_MODE_I2C_S_20KHZ    0x20
#define ISS_IO_MODE_I2C_S_50KHZ    0x30
#define ISS_IO_MODE_I2C_S_100KHZ   0x40
#define ISS_IO_MODE_I2C_S_400KHZ   0x50
#define ISS_IO_MODE_I2C_H_100KHZ   0x60
#define ISS_IO_MODE_I2C_H_400KHZ   0x70
#define ISS_IO_MODE_I2C_H_1000KHZ  0x80

// WORKS! const uint8_t I2CComms::I2C_OP_SET_MODE[] = { ISS_CMD, 0x02, 0x40, 0x04 };
//  Nb: Above I2C_OP_SET_MODE sets 100 KHz I2C Mode (software driver), with IO pins set high (not used).

const uint8_t I2CCommsWorker::I2C_OP_SET_MODE[] = { ISS_CMD, 0x02, ISS_IO_MODE_I2C_S_100KHZ, 0x55 };
const uint8_t I2CCommsWorker::I2C_OP_SET_MODE_SIZE = sizeof(I2C_OP_SET_MODE);



I2CCommsWorker::I2CCommsWorker()
{}

I2CCommsWorker::~I2CCommsWorker()
{}


void I2CCommsWorker::run()
{
    DEBUG_I2C("------- I2CCommsWorker Start on thread: " << currentThreadId() << "-------------")
    flagStop = false;

    serialTimer = std::unique_ptr<QTimer>(new QTimer(this));
    serialTimer->setSingleShot(true);
    serialTimer->stop();
    connect(serialTimer.get(), SIGNAL(timeout()), this, SLOT(serialTimeout()));

    while(!flagStop) exec();

    commsStatus = COMMS_ERROR;
    disconnect(serialTimer.get(), SIGNAL(timeout()), this, SLOT(serialTimeout()));
    serialTimer->stop();
    DEBUG_I2C("------- I2CCommsWorker Finished -------------")
}



// SLOTS //////////////////////////////////////////////

void I2CCommsWorker::I2CWorkerConnect(QString port)
{
    DEBUG_I2C("I2CCommsWorker: Open serial port " << port)
    serial = std::unique_ptr<Serial>(new Serial(this));
    connect(serial.get(), SIGNAL(transactionFinished()), this, SLOT(transactionFinished()));
    lastResult = serial->open(port);
    if (lastResult != globals::OK)
    {
        DEBUG_I2C("I2CCommsWorker: Error opening serial port (" << lastResult << ")")
    }
    else
    {
        DEBUG_I2C("I2CCommsWorker: Serial port " << port << " opened OK")
    }
    globals::sleep(100);
}


void I2CCommsWorker::I2CWorkerDisconnect()
{
    if (serial.get())
    {
        DEBUG_I2C("I2CCommsWorker: Close serial port")
        serial->close();
        disconnect(serial.get(), SIGNAL(transactionFinished()), this, SLOT(transactionFinished()));
    }
}


void I2CCommsWorker::I2CWorkerOp(int nBytesToWrite,
                                 const char *dataWrite,
                                 int nBytesToRead,
                                 char *dataRead)
{

    DEBUG_I2C_EXTRA("I2CWorkerOp: Starting comms timer on thread: " << QThread::currentThreadId())

    commsStatus = COMMS_BUSY;
    serialTimer->start(COMMS_TIMEOUT);

    DEBUG_I2C_EXTRA("I2CWorkerOp: Emitting transactionStart signal")
    emit serial->transactionStart((const uint8_t *)dataWrite, nBytesToWrite, nBytesToRead );

    DEBUG_I2C_EXTRA("I2CWorkerOp: ** Start WAIT event loop... **")
    exec();
    DEBUG_I2C_EXTRA("I2CWorkerOp: ** WAIT event loop finished **")

    size_t nBytes = 0;
    uint8_t *data = serial->getData( &nBytes );

    if ( (commsStatus == COMMS_OK) &&
         (nBytes == (size_t)nBytesToRead) )
    {
        memcpy(dataRead, data, nBytes);
        DEBUG_I2C("** Got back data: " << nBytes << " bytes; " << " [" << QString( (const char *)data) << "]")
        lastResult = globals::OK;
        globals::sleep(I2COP_SLEEP_TIME);
        return;
    }
    else
    {
        DEBUG_I2C("  I2C Op: TIMEOUT or Comms Error!")
        globals::sleep(I2COP_ERR_RECOVERY_TIME);
        emit serial->transactionCancel();
        lastResult = globals::READ_ERROR;
        return;
    }
}


/*!
 \brief Slot: Probe the USB to I2C Adaptor
 If a suitable response is detected, lastResult is set to globals::OK.
*/
void I2CCommsWorker::I2CProbeAdaptor()
{
    lastResult = globals::NOT_CONNECTED;
    if (!serial->isOpen()) return;
    DEBUG_I2C("Probing USB to I2C Adaptor")
    // Check for the USB-ISS adaptor:
    uint8_t responseData[3] = { 0,0,0 };
    I2CWorkerOp((int)I2C_OP_GET_VERSION_SIZE,
                (const char *)I2C_OP_GET_VERSION,
                3, (char *)responseData);
    if (lastResult != globals::OK)
    {
        qDebug() << "I2C Adaptor not found on serial port!";
        return;
    }
    qDebug() << "I2C Adaptor Found: USB-ISS" << endl
             << "  Module ID: " << responseData[0]
             << "; FW Version: " << responseData[1]
             << "; Mode: " << responseData[2];

    // Check version info to make sure this is a recognised adaptor:
    if (responseData[0] != I2C_ADAPTOR_VERSION[0]
    ||  responseData[1] != I2C_ADAPTOR_VERSION[1]
    ||  responseData[2] != I2C_ADAPTOR_VERSION[2])
    {
        qDebug() << "I2C Adaptor: Module ID or firmware version was invalid!";
        lastResult = globals::GEN_ERROR;
    }
}

/*!
 \brief Slot: Configure I2C Adaptor
 Loads the adaptor settings pre-configured in I2C_OP_SET_MODE (see defn above)
*/
void I2CCommsWorker::I2CConfigureAdaptor()
{
    if (!serial->isOpen()) return;
    DEBUG_I2C("Configuring USB to I2C Adaptor")
    uint8_t responseData[2] = { 0,0 };
    I2CWorkerOp((int)I2C_OP_SET_MODE_SIZE,
                (const char *)I2C_OP_SET_MODE,
                2, (char *)responseData);
    if (lastResult != globals::OK)
    {
        qDebug() << "Error configuring I2C Adaptor!";
        return;
    }
    if (responseData[0] != 0xFF)
    {
        qDebug() << "Error configuring I2C Adaptor: NACK Response!";
    }
    else
    {
        qDebug() << "I2C Adaptor Configured OK!";
    }
}


/*!
 \brief Slot: Clear Comms Port

 This method flushes the input buffer and stops comms for a delay
 period, to allow error conditions to clear. Hopefully, if the
 board has become confused, it will time out and go back to an
 idle state.
*/
void I2CCommsWorker::I2CClearPort()
{
    emit serial->transactionCancel();
    globals::sleep(50);
    lastResult = globals::OK;
}


/*!
 \brief Slot: Tell the I2CCommsWorker to shut down
*/
void I2CCommsWorker::I2CWorkerExit()
{
    flagStop = true;
    exit();
}





// PRIVATE Slots ////////////////////////////////////////////

void I2CCommsWorker::transactionFinished()
{
    DEBUG_I2C("I2CCommsWorker: transactionFinished slot called")
    if (commsStatus == COMMS_BUSY)
    {
        commsStatus = COMMS_OK;
        serialTimer->stop();
        exit();
    }
}

void I2CCommsWorker::serialTimeout()
{
    DEBUG_I2C("I2CCommsWorker: commsTimeout slot called")
    if (commsStatus == COMMS_BUSY)
    {
        commsStatus = COMMS_ERROR;
        serialTimer->stop();
        exit();
    }
}








