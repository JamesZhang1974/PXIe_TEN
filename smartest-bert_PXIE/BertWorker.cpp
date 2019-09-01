/*!
 \file   BertWorker.cpp
 \brief  Worker Thread Implementation

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QEventLoop>
#include <QStringList>

#include "globals.h"
#include "I2CComms.h"
#include "BertModel.h"
#include "BertWorker.h"


BertWorker::BertWorker()
{
    qDebug() << "BertWorker Constructor on thread " << QThread::currentThreadId();
    flagStop = false;
    flagWorkerReady = false;
}

BertWorker::~BertWorker()
{}


// Public Slots: ///////////////////////////////////////////////////////////

/*!
 \brief Connect To Instrument
        On succesful connection, sigStatusConnect(true) signal will be emitted.
        If an error occurs, sigStatusConnect(false) will be emitted, along with
        WorkerShowMessage containing an error message.
 \param port  Name of serial port to use (e.g. "COM1")
*/
void BertWorker::CommsConnect(QString port)
{
    qDebug() << "Worker: Connect signal recv on thread " << QThread::currentThreadId();
    Q_ASSERT(flagWorkerReady);       // Worker thread should be ready before calling any slots
    Q_ASSERT(!comms->portIsOpen());
        // If connect is called again when comms are already connected,
        // the client must be in a confused state. This is probably bad!

    int result;
    if (!flagWorkerReady) return;  // Thread not running yet?
    if (comms->portIsOpen())       // Already connected?
    {
        emit WorkerShowMessage("Connected.");
        emit StatusConnect(true);
        return;
    }

    result = comms->open(port);   // Connect...
    if (result != globals::OK)
    {
        emit WorkerResult(result);
        emit StatusConnect(false);
        QString message = QString("Couldn't connect to instrument on %1 (%2)").arg(port).arg(result);
        emit WorkerShowMessage(message, false);
        return;
    }

    emit WorkerShowMessage("Comms Open. Checking system components...");
    result = findAndInitEEPROM();
    if (result == globals::OK)
    {
        qDebug() << "Worker: EEPROM found; Reading model code...";
        QString modelCode = m24m02Set[0]->ReadModelCode();
        qDebug() << "Worker: Model code: " << modelCode;
        bool validModel = BertModel::SelectModel(modelCode);
        result = findComponents();
        if (result == globals::OK && !validModel) result = globals::UNKNOWN_MODEL;
    }

    // Check hardware set up result:
    switch (result)
    {
    case globals::OK:
        // Connected and hardware set up OK!
        emit WorkerShowMessage("Connected.");
        emit StatusConnect(true);
        break;
    case globals::MISSING_LMX_DEFS:
    case globals::MISSING_GT1724:
        // These errors are fatal for the connect.
        // Nothing was connected, so disconnect again.
        CommsDisconnect();  // This closes the port and shuts down any hardware components which were set up.
        emit WorkerShowMessage("Connect FAILED: Error setting up system components!");
        break;
    case globals::MISSING_EEPROM:
    case globals::UNKNOWN_MODEL:
        // EEPROM not found or couldn't determine model!
        // Lazy way to prevent crash due to "M24M02Added" signal arricing in UI after we've gievn up and deleted the hardware components:
        // DON'T Call CommsDisconnect() here...
        emit WorkerShowMessage("Initialisation FAILED: Couldn't find EEPROM!");
        emit StatusConnect(true);  // Connect anyway; Allows EEPROM write in factory mode.
        break;
    default:
        // Other error code: These conditions may indicate problems
        // (e.g. globals::MISSING_LMX), but we can't immediately disconnect,
        // because we've probably alreary emitted "GT1724Added..." signals
        // with a pointer to the new GT1724 object, and disconnect would delete
        // those objects leading to a dangling pointer and a crash.
        emit StatusConnect(true);
    }
}


/*!
 \brief Disconnect from Instrument
        Immediately after emitting this signal, clients should assume
        the instrument is disconnected and should not issue any more commands.
        On completion, a sigStatusConnect(false) signal will be emitted.
*/
void BertWorker::CommsDisconnect()
{
    qDebug() << "Worker: Disconnect signal recv on thread " << QThread::currentThreadId();
    Q_ASSERT(flagWorkerReady);
    if (!flagWorkerReady) return;  // Thread not running yet?
    shutdownComponents();
    if (comms->portIsOpen()) comms->close();
    emit WorkerResult(globals::OK);
    emit WorkerShowMessage("Disconnected.");
    emit StatusConnect(false);
}


/*!
 \brief Refresh the Serial Ports list
 Requests a list of serial ports from the operating system and
 emits a ListSerialPorts message containing available ports
*/
void BertWorker::RefreshSerialPorts()
{
    // Get a list of serial ports:
    std::unique_ptr<QStringList> serialPorts = I2CComms::getPortList();
    emit ListSerialPorts(*(serialPorts.get()));
    serialPorts.reset();
}


/*!
 \brief Get Hardware Component Options
        Send out signals to tell the client how to populate lists of options.
*/
void BertWorker::GetOptions()
{
    qDebug() << "BertWorker: Get hardware component options...";
    getComponentOptions();
}


/*!
 \brief Initialise Hardware Components
*/
void BertWorker::InitComponents()
{
    qDebug() << "Worker: InitComponents recv on thread " << QThread::currentThreadId();
    Q_ASSERT(flagWorkerReady);
    if (!flagWorkerReady) return;  // Thread not running yet?
    emit WorkerShowMessage("Comms Open. Initializing system components...");
    int result = initComponents();
    emit WorkerResult(result);
    emit WorkerShowMessage("Ready.");
}


/*!
 \brief Signal the worker thread to stop.
*/
void BertWorker::WorkerStop()
{
    flagStop = true;
    exit();  // Break the wait loop
    emit WorkerResult(globals::OK);
}


// Private Slots: ///////////////////////////////////////////////////////////

/*!
 \brief Timer Tick Slot
        Regular update timer has fired. Carry out various actions
        depending on the current state...
*/
void BertWorker::slotTimerTick()
{
    /* Example...
    static int counterTempUpdate = 0;

    if (comms->portIsOpen())
    {
    counter++;
    }
    */
}




// Private Methods: /////////////////////////////////////////////////////////

/*!
 \brief Find And Init EEPROM
 The EEPROM has it's own find and initialise step, because we need to find out the
 model code string from the EEPROM before we can initialise other components.
 \return globals::OK   At least one EEPROM found OK
 \return [Error code]
*/
int BertWorker::findAndInitEEPROM()
{
    qDebug() << "BertWorker: Search for EEPROM...";

    // ====== M24M02 EEPROM: ===============================================
    // Ping to see if there are M24M02 ICs:
    M24M02 *m24m02;
    int deviceID = 0;
    foreach(uint8_t address, BertModel::GetI2CAddresses_M24M02())
    {
        if (M24M02::ping(comms, address))
        {
            qDebug() << "BertWorker: M24M02 EEPROM found on address " << INT_AS_HEX(address,2) << ", ID " << deviceID;
            m24m02 = new M24M02(comms, address, deviceID);
            m24m02Set.append(m24m02);
            emit M24M02Added(m24m02, deviceID);
            deviceID++;
        }
    }
    if (m24m02Set.count() == 0)
    {
        // No EEPROM ICs found!
        qDebug() << "BertWorker: At least ONE M24M02 EEPROM must be present, but none were found!";
        emit WorkerShowMessage("Data EEPROM not found!");
        return globals::MISSING_EEPROM;
    }

    // ===== INIT the EEPROMs: =========
    // Call init for each M24M02 IC:
    int result;
    foreach(M24M02 *m24m02, m24m02Set)
    {
        qDebug() << "BertWorker: Initialise M24M02";
        result = m24m02->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up M24M02 (" << result << ")";
            emit WorkerShowMessage("Error configuring system!");
            return result;
        }
    }
    return globals::OK;
}



/*!
 \brief Find Instrument Components
        This method checks for hardware components of the system, e.g. GT1724 ICs
        and LMX clock interface. It instantiates objects as needed to represent the
        hardware components.
 \return globals::OK   Enough components were detected to make up a working instrument!
 \return [error code]  A critical component was not detected.
 Emits: ShowWorkerMessage to show progress / errors
        XXXXAdded to inform clients of hardware components which have been added
*/
int BertWorker::findComponents()
{
    qDebug() << "BertWorker: Search for hardware components...";
    int deviceID = 0;
//    int laneOffset = 0;

/*
    // ====== TLC59108 ICs: =========================================================
    // Ping to see if there are TLC59108 ICs:


    TLC59108 *tlc59108;
    foreach(uint8_t address, BertModel::GetI2CAddresses_TLC59108())
    {
        if (TLC59108::ping(comms, address))
        {
            qDebug() << "BertWorker: TLC59108 IC Found on address " << INT_AS_HEX(address,2) << ", Lane Offset " << laneOffset;
            tlc59108 = new TLC59108(comms, address, deviceID);
            tlc59108Set.append(tlc59108);
            emit TLC59108Added(tlc59108, deviceID);
            deviceID ++;
        }
    }
    if (tlc59108Set.count() == 0)
    {
        // No TLC59108 ICs found!
        qDebug() << "BertWorker: At least ONE TLC59108 IC must be present, but none were found!";
        emit WorkerShowMessage("LED not found!");
        return globals::MISSING_TLC59108;
    }

    // ====== GT1724 ICs: =========================================================
    // Ping to see if there are GT1724 ICs:
    // 2019-07-25 REMOVED for Federico to test boards:

    GT1724 *gt1724;
    foreach(uint8_t address, BertModel::GetI2CAddresses_GT1724())
    {
        if (GT1724::ping(comms, address))
        {
            qDebug() << "BertWorker: GT1724 IC Found on address " << INT_AS_HEX(address,2) << ", Lane Offset " << laneOffset;
            gt1724 = new GT1724(comms, address, static_cast<uint8_t>(laneOffset));
            gt1724Set.append(gt1724);
            emit GT1724Added(gt1724, laneOffset);
            laneOffset += 4;
        }
    }
    if (gt1724Set.count() == 0)
    {
        // No GT1724 ICs found!
        qDebug() << "BertWorker: At least ONE GT1724 IC must be present, but none were found!";
        emit WorkerShowMessage("Core module not found!");
        return globals::MISSING_GT1724;
    }
*/

    // ====== LMX Clock Modules: ==================================================
    // Ping to see if there are LMX2594 ICs:
    LMX2594 *lmxClock;
    deviceID = 0;
    foreach(uint8_t address, BertModel::GetI2CAddresses_LMX2594())
    {
        if (LMX2594::ping(comms, address))
        {
            qDebug() << "BertWorker: LMX2594 Clock found on address " << INT_AS_HEX(address,2) << ", ID " << deviceID;
            lmxClock = new LMX2594(comms, address, deviceID, m24m02Set.at(0));
            lmxClockSet.append(lmxClock);
            emit LMX2594Added(lmxClock, deviceID);
            deviceID++;
        }
    }
    if (lmxClockSet.count() == 0)
    {
        // No Clock modules found!
        qDebug() << "BertWorker: At least ONE LMX clock synthesizer must be present, but none were found!";
        emit WorkerShowMessage("Clock synthesizer module not found!");
        return globals::MISSING_LMX;
    }

    // ====== PCA9557B IO Controllers: =======================================
    // Ping to see if there are PCA9557 ICs:
    PCA9557B *pca9557b;
    deviceID = 0;
    foreach(uint8_t address, BertModel::GetI2CAddresses_PCA9557B())
    {
        if (PCA9557B::ping(comms, address))
        {
            qDebug() << "BertWorker: PCA9557B IO Controller found on address " << INT_AS_HEX(address,2) << ", ID " << deviceID;
            pca9557b = new PCA9557B(comms, address, deviceID);
            pca9557bSet.append(pca9557b);
            emit PCA9557B_Added(pca9557b, deviceID);
            deviceID++;

        }
    }
    if (pca9557bSet.count() == 0)
    {
        // No IO modules found!
        qDebug() << "BertWorker: At least ONE PCA9557B IO controller must be present, but none were found!";
        emit WorkerShowMessage("IO controller module not found!");
        return globals::MISSING_PCA9557B;
    }


    // ====== PCA9557A IO Controllers: =======================================
    // Ping to see if there are PCA9557A ICs:
    PCA9557A *pca9557a;
    deviceID = 0;
    foreach(uint8_t address, BertModel::GetI2CAddresses_PCA9557A())
    {
        if (PCA9557A::ping(comms, address))
        {
            qDebug() << "BertWorker: PCA9557A IO Controller found on address " << INT_AS_HEX(address,2) << ", ID " << deviceID;
            pca9557a = new PCA9557A(comms, address, deviceID);
            pca9557aSet.append(pca9557a);
            emit PCA9557A_Added(pca9557a, deviceID);
            deviceID++;

        }
    }
    if (pca9557aSet.count() == 0)
    {
        // No IO modules found!
        qDebug() << "BertWorker: At least ONE PCA9557A IO controller must be present, but none were found!";
        emit WorkerShowMessage("IO controller module not found!");
        return globals::MISSING_PCA9557A;
    }


    // ====== SI5340 Low jitter reference clock IC: ===========================
    // Ping to see if there are SI5340 ICs:
    SI5340 *si5340;
    deviceID = 0;
    foreach(uint8_t address, BertModel::GetI2CAddresses_SI5340())
    {
        if (SI5340::ping(comms, address))
        {
            qDebug() << "BertWorker: SI5340 Ref Clock generator found on address " << INT_AS_HEX(address,2) << ", ID " << deviceID;
            si5340 = new SI5340(comms, address, deviceID);
            si5340Set.append(si5340);
            emit SI5340Added(si5340, deviceID);
            deviceID++;
        }
    }
    if (si5340Set.count() == 0)
    {
        // No Ref Clock module... TODO??
    //    qDebug() << "BertWorker: At least ONE ref clock generator must be present, but none were found!";
    //    emit WorkerShowMessage("Reference clock module not found!");
    //    return globals::MISSING_???;
    }


    // Hardware found OK!
    return globals::OK;
}


/*!
 \brief Get Component Options
        Instruct each hardware component to send its option settings / lists to the client
*/
void BertWorker::getComponentOptions()
{
    // ====== GT1724 ICs: =========================================================
    foreach(GT1724 *gt1724, gt1724Set)
    {
        qDebug() << "BertWorker: Get options for GT1724";
        gt1724->getOptions();
    }

    // ====== LMX Clock Module: =======================================================
    // NOTE: SIMPLIFICATION: We will assume the client has only one set of
    // clock control UI; If there is more than one clock module, they will
    // all be configured identically by the same signals. Therefore we only
    // send out options signals for the first clock module.
    qDebug() << "BertWorker: Get options for clock part: LMX2594";
    if (lmxClockSet.count() > 0)
    {
        lmxClockSet[0]->getOptions();
    }


    // ====== PCA 9557B IO Module: =====================================================
    // NOTE: SIMPLIFICATION: As for above (PCA only controls trigger divide at the moment).
    qDebug() << "BertWorker: Get options for IO Controller: PCA9557B";
    if (pca9557bSet.count() > 0)
    {
        pca9557bSet[0]->getOptions();
    }

    /*
    // ====== PCA 9557A IO Module: =====================================================
    // NOTE: SIMPLIFICATION: GT1706 reset control and firmware load .

      qDebug() << "BertWorker: Get options for IO Controller: PCA9557A";
      if (pca9557aSet.count() > 0)
      {
          pca9557aSet[0]->getOptions();
      }

     */

    // ====== SI5340 Low jitter reference clock IC: ===========================
    // NOTE: SIMPLIFICATION: As for LMX clock module (assume UI only has one set of controls for SI5340)
    qDebug() << "BertWorker: Get options for ref clock part: SI5340";
    if (si5340Set.count() > 0)
    {
        si5340Set[0]->getOptions();
    }

    // === Options sent for all components. =====
    // Signal that we have finished sending option.
    emit OptionsSent();
}




/*!
 \brief Initialise Instrument Components
        This method carries out the initialisation work for each hardware component.
        Note this is seperated from find Components so that clients can connect up
        signals AFTER components have been created and BEFORE the init is carried
        out - this allows signals from the init process to be captured by the client.

 \return globals::OK   Init process was successful.
 \return [error code]  A critical error occurred setting up the hardware.

 Emits: WorkerShowMessage to show progress / errors
*/
int BertWorker::initComponents()
{
    qDebug() << "BertWorker: Initialise hardware components...";
    int result;

    // Initialisation Order:
    //  -SI5340 Clock Ref generator (if present)
    //      Provides input clock to LMX2594, so should be initialised first
    //      if present (selected models only).
    //  -LMX2594 Clock Module (Nb: Assumes presence of EEPROM from earlier init code!)
    //      Provides clock signal to BERT IC
    //  -PCA9557 I/O Contoller
    //      Controls various GPIO pins, including (possibly) a clock divider
    //      part which comes after the LMX2594
    //  -GT1724 BERT IC

    // ====== TLC59108 LED : ==============================
    // Call init for each TLC59108 LED:
    foreach(TLC59108 *tlc59108, tlc59108Set)
    {
        qDebug() << "BertWorker: Initialise TLC59108";
        result = tlc59108->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up TLC59108 (" << result << ")";
            emit WorkerShowMessage("Error configuring system!");
            // REMOVE for testing:
            return result;
        }
    }

    // ====== SI5340 Low jitter reference clock IC: ==============================
    // Call init for each SI5340 IC:
    foreach(SI5340 *si5340, si5340Set)
    {
        qDebug() << "BertWorker: Initialise SI5340";
        result = si5340->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up SI5340 (" << result << ")";
            emit WorkerShowMessage("Error configuring system!");
            // REMOVE for testing:
            return result;
        }
    }

    // ====== LMX Clock Modules: =================================================
    // Call init for each LMX clock module:
    foreach(LMX2594 *lmxClock, lmxClockSet)
    {
        qDebug() << "BertWorker: Initialise clock part: LMX2594";
        result = lmxClock->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up LMX clock module (" << result << ")";
            emit WorkerShowMessage("Frequency synthesizer set up error!");
            return result;
        }
    }

    // ====== PCA9557 IO Modules: ================================================
    // Call init for each PCA IO module:
    foreach(PCA9557B *pca9557b, pca9557bSet)
    {
        qDebug() << "BertWorker: Initialise IO Controller: PCA9557B";
        result = pca9557b->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up PCA9557B (" << result << ")";
            emit WorkerShowMessage("IO Controller set up error!");
            return result;
        }
    }


    // ====== PCA9557A IO Modules: ================================================
    // Call init for each PCA9557A IO module:
    foreach(PCA9557A *pca9557a, pca9557aSet)
    {
        qDebug() << "BertWorker: Initialise IO Controller: PCA9557A";
        result = pca9557a->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up PCA9557B (" << result << ")";
            emit WorkerShowMessage("IO Controller set up error!");
            return result;
        }
    }


    // ====== GT1724 ICs: =========================================================
    // Call init for each GT1724 IC:
    foreach(GT1724 *gt1724, gt1724Set)
    {
        qDebug() << "BertWorker: Initialise GT1724";
        result = gt1724->init();
        if (result != globals::OK)
        {
            qDebug() << "BertWorker: Error setting up GT1724 (" << result << ")";
            emit WorkerShowMessage("Error configuring system!");
            return result;
        }
    }

    // Hardware set up OK!
    return globals::OK;
}



/*!
 \brief Shut down and clean up all components initialised at connection time
*/
void BertWorker::shutdownComponents()
{
    qDebug() << "BertWorker: hardware clean up...";
    // ====== GT1724 ICs: =========================================================
    qDebug() << "BertWorker: REMOVE Core modules...";
    GT1724 *gt1724;
    while (gt1724Set.count() > 0)
    {
        gt1724 = gt1724Set.last();
        delete gt1724;
        gt1724Set.removeLast();
    }

    // ====== LMX Clock Modules: =================================================
    qDebug() << "BertWorker: REMOVE LMX clock modules...";
    LMX2594 *lmxClock;
    while (lmxClockSet.count() > 0)
    {
        lmxClock = lmxClockSet.last();
        delete lmxClock;
        lmxClockSet.removeLast();
    }

    // ====== PCA9557 IO Controllers: =================================================
    qDebug() << "BertWorker: REMOVE PCA9557 IO modules...";
    PCA9557B *pca9557b;
    while (pca9557bSet.count() > 0)
    {
        pca9557b = pca9557bSet.last();
        delete pca9557b;
        pca9557bSet.removeLast();
    }


    // ====== PCA9557A IO Controllers: =================================================
    qDebug() << "BertWorker: REMOVE PCA9557A IO modules...";
    PCA9557A *pca9557a;
    while (pca9557aSet.count() > 0)
    {
        pca9557a = pca9557aSet.last();
        delete pca9557a;
        pca9557aSet.removeLast();
    }

    // ===== M24M02 EEPROMs: =====================================================
    qDebug() << "BertWorker: REMOVE M24M02 modules...";
    M24M02 *m24m02;
    while (m24m02Set.count() > 0)
    {
        m24m02 = m24m02Set.last();
        delete m24m02;
        m24m02Set.removeLast();
    }

    // ====== SI5340 Low jitter reference clock IC: ==============================
    qDebug() << "BertWorker: REMOVE SI5340 modules...";
    SI5340 *si5340;
    while (si5340Set.count() > 0)
    {
        si5340 = si5340Set.last();
        delete si5340;
        si5340Set.removeLast();
    }

    qDebug() << "BertWorker: Hardware cleanup finished.";
}





void BertWorker::run()
{
    qDebug() << "=== Bert Worker START ===";
    qDebug() << "BertWorker running on thread " << QThread::currentThreadId();
    flagStop = false;

    // Periodic Update Timer:
    QTimer timer(this);
    timer.setInterval(250);
    connect(&timer, SIGNAL(timeout()), this, SLOT(slotTimerTick()));
    timer.start();

    // Comms Layer: I2C Comms class
    comms = new I2CComms();

    // Get a list of serial ports:
    RefreshSerialPorts();

    flagWorkerReady = true;

    // Enter an event loop; Wait for signals.
    forever
    {
        if (flagStop) break;
        qDebug() << "BertWorker: ***** Calling exec (Event Loop)...";
        exec();
        qDebug() << "BertWorker: ***** exec exited...";
    }

    timer.stop();
    qDebug() << "=== Bert Worker FINISHED ===";
    flagWorkerReady = false;

    shutdownComponents();
    if (comms->portIsOpen()) comms->close();
    delete comms;
}

