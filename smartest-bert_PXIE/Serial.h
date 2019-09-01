/*!
 \file   Serial.h
 \brief  Asynchronous Serial Port Handler for Comms - Header
 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#ifndef SERIAL_H
#define SERIAL_H

#include <memory>
#include <QSerialPort>
#include <QObject>

/*!
 \brief Asynchronous Serial Port Class

 Manages comms via the serial port, using the
 QT serial port (with signals and slots)

*/
class Serial : public QObject
{
Q_OBJECT

public:
    Serial(QObject *parent);
    ~Serial();

    int open(const QString portName);
    void close();
    uint8_t *getData(size_t *nBytes);
    bool isOpen() { return serialPort->isOpen(); }

public slots:
    void transactionStart(const uint8_t *data, const size_t nBytesData, const size_t nBytesResponseExpected);
    void transactionCancel();

signals:
    void transactionFinished();

private slots:
    void dataAvailable();
    void dataWritten();

private:
    std::unique_ptr<QSerialPort> serialPort;

    QByteArray     inputBuffer;
    size_t         nBytesExpected = 0;

};

#endif // SERIAL_H
