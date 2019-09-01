/*!
 \file   Serial.cpp
 \brief  Asynchronous Serial Port Handler for Comms
 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <memory>
#include <QSerialPort>
#include <QDebug>

#include "globals.h"
#include "Serial.h"

Serial::Serial(QObject *parent)
{
    serialPort = std::unique_ptr<QSerialPort>(new QSerialPort(parent));
    connect( serialPort.get(), &QSerialPort::readyRead,    this, &Serial::dataAvailable );
    connect( serialPort.get(), &QSerialPort::bytesWritten, this, &Serial::dataWritten   );
}

Serial::~Serial()
{
    close();
}



/*!
 \brief Open serial port

 If the port is already open, this method returns globals::OK,
 and no change is made to the serial port (even if portName is
 different to previous open call).

 \param serialPort      QString containing the name of the serial port to use

 \return globals::OK         Port opened OK
 \return globals::GEN_ERROR  Port NOT open. Serial transactions will be ignored.
*/
int Serial::open(const QString portName)
{
    if (serialPort->isOpen()) return globals::OK;  // Already open...

    int result = globals::OK;

    serialPort->setPortName(portName);

    // qDebug() << "SERIAL: Port: " << portName;
    bool bResult;
    if (!serialPort->open(QIODevice::ReadWrite))
    {
        qDebug() << "SERIAL: Port Open FAILED: " << serialPort->error() << ": " << serialPort->errorString();
        result = globals::GEN_ERROR;
    }
    else
    {
        // qDebug() << "SERIAL: Port Open; Setting options...";
        // Options required for USB-ISS adaptor:
        //      19200 baud, 8 data bits, no parity and one stop bit.
        bResult = serialPort->setBaudRate(QSerialPort::Baud19200) &
                  serialPort->setDataBits(QSerialPort::Data8) &
                  serialPort->setParity(QSerialPort::NoParity) &
                  serialPort->setStopBits(QSerialPort::OneStop) &
                  serialPort->setFlowControl(QSerialPort::NoFlowControl);

#ifdef BERT_SERIAL_DEBUG
        qDebug() << "SERIAL: Port Open: " << (bResult ? "OK" : "FAIL");
#endif
        (void)bResult;
        result = globals::OK;
    }

    return result;
}




/*!
 \brief Close serial port
 This method closes the port if open.
*/
void Serial::close()
{
#ifdef BERT_SERIAL_DEBUG
    qDebug() << "SERIAL: Port Close";
#endif
    if (serialPort->isOpen())  serialPort->close();
}



/*!
\brief Return a reference to data buffer

 Note: This method is not thread-safe and should
 only be called AFTER transactionFinished signal is
 emitted, and BEFORE another transactionStart signal
 is sent. At other times the contents and size of
 the buffer are unpredictable.

 The buffer is NOT copied by this call; the caller
 must make sure they are finished with the data
 before issuing another transactionStart signal.

 \param nBytes    Set to the number of bytes
                  in the buffer
 \return pointer  Pointer to the buffer data
*/
uint8_t *Serial::getData( size_t *nBytes )
{
    *nBytes = (size_t)inputBuffer.size();
    return (uint8_t *)inputBuffer.data();
}


////// SLOTS /////////////////////////////////////////////////////////////////

void Serial::transactionStart( const uint8_t *data, const size_t nBytesData, const size_t nBytesResponseExpected )
{
    if (nBytesExpected > 0)
    {
        // Transaction already in progress!
        qDebug() << "SERIAL: Port BUSY!";
    }
    else
    {
#ifdef BERT_SERIAL_DEBUG
        qDebug() << "SERIAL: -->Data Write " << nBytesData << " bytes; Expecting " << nBytesResponseExpected << " bytes in response";
#endif
        inputBuffer.truncate(0);
        nBytesExpected = nBytesResponseExpected;
        serialPort->write( (const char *)data, nBytesData );
#ifdef BERT_SERIAL_DEBUG
        qDebug() << "SERIAL: Data written";
#endif
    }
}


void Serial::transactionCancel()
{
#ifdef BERT_SERIAL_DEBUG
        qDebug() << "SERIAL: Transaction Cancel";
#endif
    inputBuffer.truncate(0);
    nBytesExpected = 0;
}


// *******************************************************************************************
// ******** PRIVATE **************************************************************************
// *******************************************************************************************



void Serial::dataAvailable()
{
#ifdef BERT_SERIAL_DEBUG
    qDebug() << "SERIAL: Data Available.";
#endif
    QByteArray data = serialPort->readAll();
    size_t bytesThisRead = data.size();
#ifdef BERT_SERIAL_DEBUG
    qDebug() << "SERIAL: -->Data Read: " << bytesThisRead
             << " bytes [" << QString(data) << "] ";
#endif

    if (nBytesExpected > 0)
    {
        inputBuffer.append(data);
        if ((size_t)inputBuffer.size() > nBytesExpected)
        {
            size_t overflowSize = inputBuffer.size() - nBytesExpected;
            inputBuffer.truncate(nBytesExpected);
            qDebug() << "SERIAL: INPUT BUFFER OVERFLOW! Dropped "
                     << overflowSize  << " bytes.";
        }
        if ((size_t)inputBuffer.size() == nBytesExpected)
        {
            // We now have the expected amount of data!
            //qDebug() << "SERIAL: Read Finished!";
            emit transactionFinished();
            nBytesExpected = 0;
        }
        else
        {
            // Still waiting for more data...
            //qDebug() << "SERIAL: Still waiting for " << (nBytesExpected - inputBuffer.size()) << " bytes.";
        }
    }
    else
    {
        // Data arrived, but we weren't expecting any.
        qDebug() << "SERIAL: UNEXPECTED DATA! Dropped "
                 << bytesThisRead << " bytes.";
    }
}


void Serial::dataWritten()
{
#ifdef BERT_SERIAL_DEBUG
    qDebug() << "SERIAL: -->Data Written OK.";
#endif
}

