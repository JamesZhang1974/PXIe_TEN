#include "mainwindow.h"


// -- Repeat Count Lookup: ------------
// This look up table maps the index of items in the eye scan and bathtub plot
// "repeats" combo box to a number of repeats. -1 = Repeat forever.
const QList<int> BertWindow::EYESCAN_REPEATS_LOOKUP =
   { 1, 5, 10, 50, 100, 500, 1000, -1 };

// List of items for use in the eye scan and bathtub plot 'repeats' combo
const QStringList BertWindow::EYESCAN_REPEATS_LIST =
   { "1", "5", "10", "50", "100", "500", "1000", "∞" };


// Convert index in Channel Select list (on CDR Mode tab) to device lane
#define CDR_CH_SELECT_TO_LANE(i) (i * 2) + 1

BertWindow::BertWindow(QWidget *parent) :
    QMainWindow(parent),
    nextEDMaster(edEnabledMasterChannels),
    nextEDSlave(edEnabledSlaveChannels)
{
    globals::setAppPath(QCoreApplication::applicationDirPath());  // qApp->applicationDirPath());

    // Create UI Layout:
    qDebug() << "Setting up UI...";

    // Window Size:
    resize(1302, 662);

    // Window title:
#ifdef BERT_DEMO_CHANNELS
    QString windowTitle = QString("%1 DEMO").arg(BertBranding::APP_TITLE);
#else
    QString windowTitle = BertBranding::APP_TITLE;
#endif
    setWindowTitle(windowTitle);

    // Create widgets:
    setCentralWidget(makeUI(parent));

#ifdef BRAND_CEYEAR
#else
    if (checkFactoryKey())
    {
        makeUIFactoryOptions(this);
        factoryOptionsEnabled = true;
    }
    else
    {
        makeUIDummyFactoryOptions(this);
        factoryOptionsEnabled = false;
    }
#endif

    QMetaObject::connectSlotsByName(this);

    commsConnected = false;
    eventsEnabled = true;
    uiChangeOnConnect(false);  // Set UI to "not connected" state.

    // Set up the Error Detector timer: When the error detector is enabled,
    // this timer is used to read the bit and error counters at regular intervals.
    uiUpdateTimer = new QTimer(this);
    uiUpdateTimer->setInterval(250);
    connect(uiUpdateTimer, SIGNAL(timeout()), this, SLOT(uiUpdateTimerTick()));

    tickCountStatusTextReset = 0;
    tickCountTempUpdate = 0;
    tickCountTemperatureTextReset = 0;
    tickCountClockLockUpdate = 0;

    // Tracks how long the ED measurement has been running:
    edRunTime = new QTime();

    // Make sure "flasher" labels are invisible and other ED controls are ready:
    edControlInit();

    uiUpdateTimer->start();  // Nb: Timer runs all the time, but only does work when needed.

    // Set up worker thread:
    qDebug() << "Creating worker FROM thread " << QThread::currentThreadId();
    bertWorker = new BertWorker();
    bertWorker->moveToThread(bertWorker);
      // We need to do this before connecting up signals and slots, so that we
      // ensure that the slots get called in the correct thread (i.e. the worker).

    // Connect up worker signals: Nb: Macro! See BertWorker.h
    BERT_WORKER_CONNECT_SIGNALS(this, bertWorker)

    bertWorker->start();

//#define BERT_DEMO_CHANNELS 4
//#define BERT_DEMO_CHANNELS 8
#ifdef BERT_DEMO_CHANNELS
    for (int ch = 1; ch <= BERT_DEMO_CHANNELS; ch++) makeUIChannel(ch, this);
    unlockUI();
#endif

}



BertWindow::~BertWindow()
{
    uiUpdateTimer->stop();
    emit WorkerStop();
    globals::sleep(2000); // TODO: Implement properly!
    delete bertWorker;
    delete uiUpdateTimer;
}



#define BERT_SIGNALS_DEBUG

// ========== SLOTS - Worker thread signals ================================================
void BertWindow::WorkerResult(int result)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig Result from worker thread (" << result << ")";
#endif
    unlockUI();
}

void BertWindow::WorkerShowMessage(QString message, bool append)
{
    ShowMessage(message, append);
}

void BertWindow::ListSerialPorts(QStringList ports)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig ListSerialPorts on thread " << QThread::currentThreadId();
#endif
    // Copy the existing list of serial ports. If there are already some ports, and a
    // new one has just appeared, we assume the user has turned on the instrument then
    // clicked "Refresh"; thus the new item in the list is probably the port for
    // the instrument and we should select it automatically.
    QStringList oldList = listSerialPorts->getItems();
    listPopulate("listSerialPorts", -1, ports, 0);
    QStringList newList = listSerialPorts->getItems();
    for (auto i = 0; i < newList.count(); i++)
    {
        if (!oldList.contains(newList.at(i)))
        {
            // NEW item found!
            listSerialPorts->setCurrentIndex(i);
            break;
        }
    }
}

void BertWindow::StatusConnect(bool connected)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig StatusConnect (Connected = " << connected << ") on thread " << QThread::currentThreadId();
#endif
    uiChangeOnConnect(connected);
    commsConnected = connected;
    if (connected)
    {
        // At this point, the comms are connected, components have been added by the worker
        // (GT1724Added signal, etc, received).
        // Tell the worker we are ready to get options for the hardware components.
        emit GetOptions();
    }
    else
    {
        // If disconnected: Delete all channel-specific controls, and clear the channel list:
        deleteUIChannels();
        if (groupRefClock) deleteUIRefClock();  // If Reference Clock controls exist, remove them.
    }
}

void BertWindow::GT1724Added(GT1724 *gt1724, int laneOffset)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig GT1724Added with lane offset " << laneOffset << " on thread " << QThread::currentThreadId();
#endif
    // Create UI channels for this GT1724:
    int channel = BertChannel::laneToChannel(laneOffset);
    makeUIChannel(channel,     this);
    makeUIChannel(channel + 1, this);
    if (BertModel::UseFourChanPGMode())
    {
        makeUIChannel(channel + 2, this);
        makeUIChannel(channel + 3, this);
    }

    // Connect up slots and signals for this GT1724 chip:
    GT1724_CONNECT_SIGNALS(this, gt1724)
}

void BertWindow::LMX2594Added(LMX2594 *lmx2594, int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig LMX2594Added with ID " << deviceID << " on thread " << QThread::currentThreadId();
#endif
    // Connect up slots and signals for an LMX2594 clock chip:
    LMX_CONNECT_SIGNALS(this, lmx2594)
}

void BertWindow::PCA9557B_Added(PCA9557B *pca9557b, int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig PCA9557B_Added with ID " << deviceID << " on thread " << QThread::currentThreadId();
#endif
    // Connect up slots and signals for a PCA9557B IO chip:
    PCA9557B_CONNECT_SIGNALS(this, pca9557b)
}

void BertWindow::PCA9557A_Added(PCA9557A *pca9557a, int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig PCA9557A_Added with ID " << deviceID << " on thread " << QThread::currentThreadId();
#endif
    //Connect up slots and signals for a PCA9557A IO chip:
    PCA9557A_CONNECT_SIGNALS(this, pca9557a)
}


void BertWindow::M24M02Added(M24M02 *m24m02, int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig M24M02Added with ID " << deviceID << " on thread " << QThread::currentThreadId();
#endif
    // Connect up slots and signals for a M24M02Added EEPROM chip:
    M24M02_CONNECT_SIGNALS(this, m24m02)
}

void BertWindow::TLC59108Added(TLC59108 *tlc59108, int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig TLC59108 with ID " << deviceID << " on thread " << QThread::currentThreadId();
#endif
    // Connect up slots and signals for a M24M02Added EEPROM chip:
    TLC59108_CONNECT_SIGNALS(this, tlc59108)
}

void BertWindow::SI5340Added(SI5340 *si5340, int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig SI5340Added with ID " << deviceID << " on thread " << QThread::currentThreadId();
#endif
    // Connect up slots and signals for an SI5340Added low jitter clock chip:
    SI5340_CONNECT_SIGNALS(this, si5340)

    // Add UI contols for ref clock:
    if (deviceID == 0) makeUIRefClock();  // Only add UI once (for 1st ref clock we find!).
}


void BertWindow::OptionsSent()
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig OptionsSent";
#endif
    // Options for hardware components have all been sent (so our signals should all be connected):
    // We can now start hardware initialisation.
    emit InitComponents();
    // Request EEPROM data (fills in instrument information):
    emit EEPROMReadStrings(0);

    // FACTORY MODE ONLY: Request reading of clock defs from TCS files
    if (factoryOptionsEnabled)
    {
        /*
        QString FilesFirmwarePath = globals::getAppPath() + QString("\\firmwares\\");
        QStringList FirmwareVersion;
        int FWcheck = BertFile::readDirectory(FilesFirmwarePath, FirmwareVersion);
        if (!FWcheck)
        {
            qDebug() <<"Firmware Name is "<<FirmwareVersion;
            const QStringList firmwaveList = FirmwareVersion;

        }
        else {
            qDebug() << "$$$$$$$$$$$$$$NOT Read the firmware!$$$$$$$$$$$$$$$"<<FWcheck;
        }
        */
        QString clockRegFilePath = globals::getAppPath() + QString("\\clockdefs\\");
        emit ReadTcsFrequencyProfiles(clockRegFilePath);
    }
}


// ========== SLOTS - General Purpose Component signals ========================================

void BertWindow::Result(int result, int lane)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig Result from component (Result: " << result << "; Lane: " << lane << ") ------------------------------------------------------------------";
#endif
    unlockUI();
}

void BertWindow::ListPopulate(QString name, int lane, QStringList items, int defaultIndex)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig ListPopulate from component (Name: '" << name << "''; Lane: " << lane << ")";
#endif
    eventsEnabled = false;
    listPopulate(name, lane, items, defaultIndex);
    // SPECIAL CASES:
    if (name == "listLMXFreq") listPopulate("listEEPROMLMXFreq", lane, items, 0);
    eventsEnabled = true;
}


void BertWindow::ListSelect(QString name, int lane, int index)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig ListSelect from component (Name: '" << name << "''; Lane: " << lane << "; Index: " << index << ")";
#endif
    QComboBox *target = findItem<QComboBox *>(name, lane);
    if (!target) return;
    Q_ASSERT(index >= 0 && index < target->count());
    eventsEnabled = false;
    target->setCurrentIndex(index);
    eventsEnabled = true;
}

void BertWindow::UpdateBoolean(QString name, int lane, bool value)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig UpdateBoolean for '" << name << "', lane " << lane << ", value " << value;
#endif
    QCheckBox *target = findItem<QCheckBox *>(name, lane);
    if (!target) return;
    eventsEnabled = false;
    target->setChecked(value);
    eventsEnabled = true;
}


void BertWindow::SetPGLedStatus(int lane, bool laneOn)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig SetPGLedStatus for lane " << lane << ", laneon " << laneOn;
#endif
    emit (ChangPGLedStatus(lane, laneOn));
}



void BertWindow::UpdateString(QString name, int lane, QString value)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig UpdateString for '" << name << "', lane " << lane << ", value " << value;
#endif
    QLabel *target = findItem<QLabel *>(name, lane);
    if (!target) return;
    eventsEnabled = false;
    target->setText(QString("%1").arg(value));
    // Special cases:
    if (name.left(15) == "CoreTemperature") tickCountTemperatureTextReset = 0;
    eventsEnabled = true;
}

void BertWindow::ShowMessage(QString message, bool append)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig ShowMessage ('" << message << "'') on thread " << QThread::currentThreadId();
#endif
    if (append) appendStatus(message);
    else        updateStatus(message);
}


// ========== SLOTS - GT1724 IC  ============================================

void BertWindow::EDLosLol(int lane, bool los, bool lol)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig EDLosLol: Lane = " << lane << "; LOS = " << los << "; LOL = " << lol;
#endif
    int channel = BertChannel::laneToChannel(lane);
    getChannel(channel)->getED()->setEDSignalLock(!los);
    getChannel(channel)->getED()->setEDCDRLock(!los && !lol);
    cdrModeLosLolUpdate(lane, los, lol);
}



void BertWindow::EDCount(int lane,
                         bool locked,
                         double bits, double bitsTotal,
                         double errors, double errorsTotal)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig EDCount: Lane = "
             << lane << ";  locked = "
             << locked << "; bits = "
             << bits << "; errors = "
             << errors << "; bitsTotal = "
             << bitsTotal << "; errorsTotal = "
             << errorsTotal;
#endif
    // Note the "BER" we calculate here is best described as the
    // "bit error RATIO" as it is errors per bit, not errors per second.

    // Update the ED requests pending count: Choose the master or slave count based on the board for this lane.
    if (BertChannel::laneToBoard(lane) == 0) edMasterRequestsPending--;
    else                                     edSlaveRequestsPending--;

    // If this count is for a lane which wasn't locked, IGNORE the count:
    // The values will be all 0! Note we DO have to decrement the "pending" count
    // (done above) so that we know that our requests are still being handled.
    if (!locked) return;

    double errorRatio;
    int channel = BertChannel::laneToChannel(lane);
    BertChannel *bertChannel = getChannel(channel);

    // Turn on error indicator if this error count greater than last one:
    if (errors > 0) bertChannel->edErrorflasherOn = true;
    else            bertChannel->edErrorflasherOn = false;
    emit EDLedFlash(lane, edRunning, bertChannel ->edErrorflasherOn);
    // Error ratio calculations:
    errorRatio = 0.0;
    // Check whether to show Instantaneous or Cumulative results:
    int resultDisplay = listEDResultDisplay->currentIndex();
    // 0 = Cumulative, 1 = Instantaneous:
    if (resultDisplay == 0)
    {
        // --- Show Accumulated values: ---
        if (bitsTotal > 0) errorRatio = (errorsTotal / bitsTotal);
        // Update the Bit and Error counters in the UI:
        bertChannel->getED()->setEDValueBits(bitsTotal);
        bertChannel->getED()->setEDValueErrors(errorsTotal);
    }
    else
    {
        // --- Show Instantaneous values: ---
        if (bits > 0) errorRatio = (errors / bits);
        // Update the Bit and Error counters in the UI:
        bertChannel->getED()->setEDValueBits(bits);
        bertChannel->getED()->setEDValueErrors(errors);
    }
    bertChannel->getED()->setEDValueBER(errorRatio);
    bertChannel->getED()->plotAddPoint(errorRatio);
}


// ========== SLOTS - LMX Clock Source  ====================================

void BertWindow::LMXInfo(int deviceID, int indexProfile, int indexTrigOutputPower, int indexFOutOutputPower, int indexTriggerDivide, bool outputsOn, float frequency)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig LMXInfo for clock " << deviceID;
#endif
    // We only care about updates from the master clock (hopefully slave clock should be exactly the same!)
    if (deviceID > 0) return;
    Q_UNUSED(outputsOn)
    Q_UNUSED(indexFOutOutputPower)
    Q_UNUSED(indexTriggerDivide)
    eventsEnabled = false;
    listLMXFreq->setCurrentIndex(indexProfile);
    listLMXTrigOutPower->setCurrentIndex(indexTrigOutputPower);
    // Update the system-wide bit rate:
    bitRate = static_cast<double>(frequency) * 2.0 * 1e6; // Convert to GBits and double (clock is 1/2 rate)
    valueBitRate_PG->setText(      QString("%1").arg( (bitRate/1e9), 0, 'f', 5)  );  // Gbps
    valueBitRate_ED->setText(      QString("%1").arg( (bitRate/1e9), 0, 'f', 5)  );
    valueBitRate_EyeScan->setText( QString("%1").arg( (bitRate/1e9), 0, 'f', 5)  );
    valueBitRate_Bathtub->setText( QString("%1").arg( (bitRate/1e9), 0, 'f', 5)  );
    qDebug() << "Bitrate Updated to " << bitRate;
    tickCountClockLockUpdate = 0;
    eventsEnabled = true;
}


/* NB: NOT USED: */
void BertWindow::LMXVTuneLock(int deviceID, bool isLocked)
{
    // LMX Clock: VCO Lock status update
    // We only care about updates from the master clock (hopefully slave clock should be exactly the same!)
    if (deviceID > 0) return;
    qDebug() << "****** LMXVTuneLock: " << isLocked << "********";
}




/*!
 \brief LMX Clock settings have changed
        Need to resync the PG.
*/
void BertWindow::LMXSettingsChanged(int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig LMXSettingsChanged for clock " << deviceID << ". Resync PG!!!";
#endif
    lockUI(4000);
    // Nb: Need to reconfigure correct lanes, depending on clock number.
    // Clock 0 drives lanes 0 - 7 (master board); Clock 1 drives lanes 8 - 15 (slave board), etc.
    int laneA = deviceID * 8;
    int laneB = laneA + 4;
    int pattern = getChannel(1)->getPG()->getPGPatternIndex();
    qDebug() << "-Clock " << deviceID << " Changed: Resync lanes " << laneA << " - " << laneB + 3;
    emit ConfigPG(laneA, pattern, bitRate);
    emit ConfigPG(laneB, pattern, bitRate);
}


// ========== SLOTS - PCA9557 IO Controller  =======================================
void BertWindow::LMXLockDetect(int deviceID, bool isLocked)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig LMXLockDetect: " << isLocked << " for clock " << deviceID;
#endif
    BertUILamp *lamp = nullptr;
    if (deviceID == 0) lamp = lampLMXLockMaster;
    if (deviceID == 1) lamp = lampLMXLockSlave;
    if (!lamp) return;  // ??? Invalid board.
    if (isLocked) lamp->setState(BertUILamp::OK);
    else          lamp->setState(BertUILamp::ERR);
}




// ========== SLOTS - M24M02 EEPROM IC  ============================================
// Received EEPROM data
// NOTE: This message includes the "model" code for the device,
// which is used to set up the GT1724 cores (PG vs ED channels)
// Therefore, it's important that this signal be received before the
// GT1724 cores are configured.
// We hope that the Bert Worker class manages that; the only reason
// that we select the Bert Model here (BertModel::SelectModel) is because
// the back end doesn't really want to know what model it is supposed to be;
// we want to delegate that choice to the UI.
// POSSIBLE IMPROVEMENT: Move model selection to the back end, and just
// inform the UI of what channel controls it ought to display?

void BertWindow::EEPROMStringData(int deviceID,
                                  QString model,
                                  QString serial,
                                  QString productionDate,
                                  QString calibrationDate,
                                  QString warrantyStart,
                                  QString warrantyEnd,
                                  QString synthConfigVersion)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig EEPROMStringData for EEPROM " << deviceID << ".";
#endif
    if (deviceID != 0) return;   // IGNORE any eeproms other than master board!
    qDebug() << "EEPROM Data Received: ";
    qDebug() << "  Model:          " << model;
    qDebug() << "  Serial:         " << serial;
    qDebug() << "  Prod Date:      " << productionDate;
    qDebug() << "  Cal Date:       " << calibrationDate;
    qDebug() << "  Warranty Start: " << warrantyStart;
    qDebug() << "  Warranty End:   " << warrantyEnd;
    qDebug() << "  Synth Config:   " << synthConfigVersion;

    UpdateString("InstrumentModel", 0, model);
    UpdateString("InstrumentSerial", 0, serial);
    UpdateString("InstrumentProductionDate", 0, productionDate);
    UpdateString("InstrumentCalibrationDate", 0, calibrationDate);
    UpdateString("InstrumentWarrantyStartDate", 0, warrantyStart);
    UpdateString("InstrumentWarrantyEndDate", 0, warrantyEnd);
    UpdateString("InstrumentSynthConfigVersion", 0, synthConfigVersion);

    // Also update contents of the "Write EEPROM Data" panel if it exists (Factory only):
    if (inputModel) inputModel->setText(model);
    if (inputSerial) inputSerial->setText(serial);
    if (inputProdDate) inputProdDate->setText(productionDate);
    if (inputCalDate) inputCalDate->setText(calibrationDate);
    if (inputWarrantyStart) inputWarrantyStart->setText(warrantyStart);
    if (inputWarrantyEnd) inputWarrantyEnd->setText(warrantyEnd);
    if (inputSynthConfigVersion) inputSynthConfigVersion->setText(synthConfigVersion);

    /* // DEPRECATED
    // Decide whether to display the ED controls:
    // These will only be shown if the model is "SB-XXXXXX" and BertModel::UseFourChanPGMode() is FALSE
    bool showED = false;
    if (!BertModel::UseFourChanPGMode()
      && model.length() > 2
      && model.left(2) == QString("SB")) showED = true;

    if (showED) showEDControls(true);
    else        showEDControls(false);

    */

    // Select Model features:
    if (BertModel::SelectModel(model))
    {
        showControls(SMARTEST_PG, true);  // All models have PG controls.

        if (BertModel::UseFourChanPGMode()) showControls(SMARTEST_ED, false);
        else                                showControls(SMARTEST_ED, true);
    }
    else
    {
        // Couldn't determine model... hide all PG and ED controls.
        showControls(SMARTEST_PG, false);
        showControls(SMARTEST_ED, false);
    }
}


// ========== SLOTS - SI5340 reference clock IC ===========================================
void BertWindow::RefClockInfo(int deviceID,
                              int indexProfile,
                              float frequencyIn,
                              float frequencyOut,
                              QString descriptionIn,
                              QString descriptionOut)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig RefClockInfo for Ref Clock " << deviceID << ".";
#endif
    if (deviceID != 0) return;   // IGNORE any ref clocks other than master board!

    Q_UNUSED(frequencyIn)
    Q_UNUSED(frequencyOut)

    eventsEnabled = false;
    if (listRefClockProfiles) listRefClockProfiles->setCurrentIndex(indexProfile);
    if (valueRefClockFreqIn) valueRefClockFreqIn->setText(descriptionIn);
    if (valueRefClockFreqOut) valueRefClockFreqOut->setText(descriptionOut);
    eventsEnabled = true;
}

void BertWindow::RefClockSettingsChanged(int deviceID)
{
#ifdef BERT_SIGNALS_DEBUG
    qDebug() << "Received Sig RefClockSettingsChanged for Ref Clock " << deviceID << ".";
#endif
    if (deviceID != 0) return;   // IGNORE any ref clocks other than master board!
    // TODO: Trigger resync of other components??
}



// ------ PRIVATE helper methods: ---------------------------
void BertWindow::listPopulate(QString name, int lane, QStringList items, int defaultIndex)
{
    QComboBox *comboBox = findItem<QComboBox *>(name, lane);
    if (!comboBox) return;
    comboBox->clear();
    comboBox->addItems(items);
    if (defaultIndex < 0 || defaultIndex >= items.count())
    {
        qDebug() << "ERROR - sigPopulateList - '" << name
                 << "': Default index out of range (Default: " << defaultIndex << "; Items: " << items.count() << ")";
        comboBox->setCurrentIndex(0);
        return;
    }
    comboBox->setCurrentIndex(defaultIndex);
}


/*!
 \brief Find child widget by name
 This template method searches for a specified type of widget by name,
 in the child widgets of the main window.
 \param suffix  Optional suffix. If >= 0, "_n" is appended to the supplied name
                (where n is the supplied value). Optional; defaults to -1 (no suffix)
 \return [widget] Pointer to the widget if found
 \return NULL     No widget with that name was found.
*/
template<class T> T BertWindow::findItem(QString name, int suffix)
{
    QString nameFull;
    if (suffix >= 0) nameFull = QString("%1_%2").arg(name).arg(suffix);
    else             nameFull = name;
    //qDebug() << "Searching for item " << nameFull << "(suffix: " << suffix << ")";
    T target = this->findChild<T>(nameFull);
    if (!target)
    {
        qDebug() << "Error: Item " << nameFull << " not found.";
        return nullptr;
    }
    return target;
}


BertChannel *BertWindow::getChannel(int channel)
{
    BertChannel *thisChannel = bertChannels.value(channel, nullptr);
    Q_ASSERT(thisChannel);
    return thisChannel;
}


/* ============ Lock / Unlock The UI: ===================================================== */
/*!
 \brief Lock UI
        This method disables all UI controls, so that the user can't
        start any new operations (used while an operation is in progress).
        The UI is unlocked again when a "Result" signal is received, or
        when the timeout expires. Where an operation may produce several
        'Result' messages, 'depth' can be used to ignore the intermediate
        results (see below).

 \param timeoutMs  If the operation isn't finished within this time, UNLOCK
                   the UI anyway (NB: this is probably due to an error in
                   the worker thread! It's not normal operation).
 \param depth      If an operation produces several "Result" signals as
                   part of normal progress, they can be ignored by using
                   depth > 1 (e.g. depth = 2 will ignore the first Result
                   signal).
                   OPTIONAL; Defaults to 1.
*/
void BertWindow::lockUI(int timeoutMs, int depth)
{
    Q_ASSERT(depth > 0);
    uiUnlockInMs = timeoutMs;
    uiLockLevel += depth;
    tabWidget->setEnabled(false);
}


void BertWindow::unlockUI()
{
    uiLockLevel--;
    if (uiLockLevel < 1)
    {
        // All nested locks have been unlocked. Unlock UI and clear auto unlock timer:
        uiUnlockInMs = 0;
        uiLockLevel = 0;
        tabWidget->setEnabled(true);
    }
    else
    {
        // At least one outstanding lock remains:
        // If the auto unlock timer has expired, extend it to unlock remaining nested locks:
        if (uiUnlockInMs <= 0) uiUnlockInMs = 250;
    }
}

void BertWindow::enablePageChanges()
{
    eventsEnabled = true;
}


void BertWindow::uiChangeOnConnect(bool connectedStatus)
{
    tabConnect->setEnabled(true);
    listSerialPorts->setEnabled(!connectedStatus);
    buttonPortListRefresh->setEnabled(!connectedStatus);
    buttonConnect->setText((connectedStatus) ? "Disconnect" : "Connect");
    buttonResync->setEnabled(connectedStatus);

    if (listEEPROMModel) listEEPROMModel->setEnabled(connectedStatus);
    if (buttonEEPROMDefaults) buttonEEPROMDefaults->setEnabled(connectedStatus);
    if (buttonWriteEEPROM) buttonWriteEEPROM->setEnabled(connectedStatus);
    if (listTCSLMXProfiles) listTCSLMXProfiles->setEnabled(connectedStatus);
    if (listEEPROMLMXProfiles) listEEPROMLMXProfiles->setEnabled(connectedStatus);
    if (buttonWriteProfilesToEEPROM) buttonWriteProfilesToEEPROM->setEnabled(connectedStatus);
    if (buttonVerifyLMXProfiles) buttonVerifyLMXProfiles->setEnabled(connectedStatus);

    if (listFirmwareVersion) listFirmwareVersion->setEnabled(connectedStatus);
 // if (listEEPROMFirmware) listEEPROMFirmware->setEnabled(connectedStatus);
 // if (buttonWriteFirmwareToEEPROM) buttonWriteFirmwareToEEPROM->setEnabled(connectedStatus);
 // if (buttonVerifyFirmware) buttonVerifyFirmware->setEnabled(connectedStatus);

    groupTemps->setEnabled(connectedStatus);
    groupClock->setEnabled(connectedStatus);

    tabPatternGenerator->setEnabled(connectedStatus);
    tabErrorDetector->setEnabled(connectedStatus);
    tabAnalysisEyeScan->setEnabled(connectedStatus);
    tabAnalysisBathtub->setEnabled(connectedStatus);
    tabAbout->setEnabled(true);

    if (!connectedStatus)
    {
        showControls(SMARTEST_PG, false);
        showControls(SMARTEST_ED, false);
    }

}




/*!
 \brief Update Status Message
 \param text  New text for status message
*/
void BertWindow::updateStatus(QString text)
  {
  statusMessage->setText(text);
  statusMessage->repaint();
  tickCountStatusTextReset = 0;
  }
/*!
 \brief Append text to Status Message
 \param text  Text to append to the current status message
*/
void BertWindow::appendStatus(QString text)
  {
  statusMessage->setText(statusMessage->text().append(text));
  statusMessage->repaint();
  tickCountStatusTextReset = 0;
  }







/**************************************************************************
*    Tab Events: May cancel click in some circumstances
**************************************************************************/

// Click occurred on tab bar: Record current tab in case we want to cancel.
void BertWindow::on_tabWidget_tabBarClicked(int index)
{
    currentTabIndex = tabWidget->currentIndex();
    Q_UNUSED(index);
}
// Tab bar clicked. Check whether changing tabs is allowed right now.
void BertWindow::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index)
    // Can't change tabs while connecting, or ED, eye scan or bathtub analysis, or CDR mode, is operating:
    if ( connectInProgress ||
         edPending         ||
         edRunning         ||
         eyeScanRunning    ||
         bathtubRunning    ||
         cdrModeRunning )
    {
        tabWidget->setCurrentIndex(currentTabIndex);
        return;
        // ...Cancel the click.
    }

    // When changing to the ED page, update the Start / Stop button status
    // (this depends on info on other pages...)
    TabID tabID = static_cast<TabID>(tabWidget->currentWidget()->property("TabID").toInt());
    if (tabID == TAB_ED) edStartStopReflect();
    currentTabIndex = tabWidget->currentIndex();
}








// *******************************************************************************
// **** Connect Page Buttons: ****************************************************
// *******************************************************************************


/*!
 \brief Refresh button - Refresh list of serial ports
*/
void BertWindow::on_buttonPortListRefresh_clicked()
{
    emit RefreshSerialPorts();
}


/*!
 \brief Connect Button
  - Opens connection to BERT board (uses BertHardware; this is probably a
    virtual serial port representing a USB-I2C adaptor).
  - Connect function in BertHardware will check whether the extension macros
    have been downloaded to the board yet, and download them if they haven't.
*/
void BertWindow::on_buttonConnect_clicked()
{
    qDebug() << "Connect button clicked on thread " << QThread::currentThreadId();
    lockUI(70000);

    if (!commsConnected)
    {
        // Connect:
        QString port;
        if (listSerialPorts->count() == 0)
        {
            port = QString("");
        }
        else
        {
            if (listSerialPorts->currentIndex() < 0) listSerialPorts->setCurrentIndex(0);
            port = listSerialPorts->itemText(listSerialPorts->currentIndex());
        }
        emit CommsConnect(port);
    }
    else
    {
        // Disconnect:
        emit CommsDisconnect();
        if (listTCSLMXProfiles) listTCSLMXProfiles->clear();
        if (listEEPROMLMXProfiles) listEEPROMLMXProfiles->clear();
        if (listFirmwareVersion) listFirmwareVersion->clear();
//      if (listEEPROMFirmware) listEEPROMFirmware->clear();
    }
}


/*!
 \brief Resync Button Clicked
 Force resync by calling the "Control PRBS Generator" macro
*/
void BertWindow::on_buttonResync_clicked()
{
    updateStatus("Re-Sync...");

    // ---- Resync the clock: ----
    updateStatus("Re-Sync Frequency Synthesizer...");
    lockUI(5000, 2);
    emit SelectProfile(listLMXFreq->currentIndex(), false);

    // ---- Resync Gennum ICs: ----
    globals::sleep(1000);  // Allow for the OK message to be received.
    updateStatus("Re-Sync Pattern Generator...");
    lockUI(5000, 1);
    emit ConfigSetDefaults(globals::ALL_LANES, bitRate);
}




/*!
 * \brief List of model options in EEPROM setup has chaned to a new item. Set 'model' text box.
 * \param index New list index
 */
void BertWindow::on_listEEPROMModel_currentIndexChanged(int index)
{
    Q_ASSERT(inputModel && listEEPROMModel);
    if (index > 0) inputModel->setText(listEEPROMModel->currentText());
}


/*!
 * \brief EEPROM Set Defaults for String Data
 * Set default values for EEPROM string data (EEPROM Setup - Factory Only)
 */
void BertWindow::on_buttonEEPROMDefaults_clicked()
{
    Q_ASSERT(inputModel
          && inputSerial
          && inputProdDate
          && inputCalDate
          && inputWarrantyStart
          && inputWarrantyEnd);

    QDate dateNow = QDate::currentDate();
    inputSerial->setText("00000001");
    inputProdDate->setText(dateNow.toString("dd-MMM-yyyy"));
    inputCalDate->setText(dateNow.toString("dd-MMM-yyyy"));
    inputWarrantyStart->setText(dateNow.toString("dd-MMM-yyyy"));
    inputWarrantyEnd->setText(dateNow.addYears(1).toString("dd-MMM-yyyy"));
}


/*!
 * \brief Write EEPROM String Data
 * Write string values to EEPROM (EEPROM Setup - Factory Only)
 */
void BertWindow::on_buttonWriteEEPROM_clicked()
{
    Q_ASSERT(inputModel
          && inputSerial
          && inputProdDate
          && inputCalDate
          && inputWarrantyStart
          && inputWarrantyEnd
          && inputSynthConfigVersion);

    updateStatus("Writing Data to EEPROM...");
    lockUI(5000, 3);

    qDebug() << "UNLOCK EEPROM...";
    emit SetEEPROMWriteEnable(true);
    qDebug() << "UPDATE EEPROM...";
    emit EEPROMWriteStrings(0,
                            inputModel->text(),
                            inputSerial->text(),
                            inputProdDate->text(),
                            inputCalDate->text(),
                            inputWarrantyStart->text(),
                            inputWarrantyEnd->text(),
                            inputSynthConfigVersion->text());
    qDebug() << "RELOCK EEPROM...";
    emit SetEEPROMWriteEnable(false);
    emit EEPROMReadStrings(0);

}


/*!
 \brief Write Frequency Profiles to EEPROM
 EEPROM Setup - Factory Only
*/
void BertWindow::on_buttonWriteProfilesToEEPROM_clicked()
{
    lockUI(10000, 3);
    qDebug() << "UNLOCK EEPROM...";
    emit SetEEPROMWriteEnable(true);
    qDebug() << "UPDATE EEPROM...";
    emit LMXEEPROMWriteFrequencyProfiles();
    qDebug() << "RELOCK EEPROM...";
    emit SetEEPROMWriteEnable(false);

    listTCSLMXProfiles->clear();
    listEEPROMLMXProfiles->clear();
    listEEPROMLMXProfiles->addItems(QStringList({"[Disconnect / Reconnect Required]"}));
    buttonWriteProfilesToEEPROM->setEnabled(false);
    buttonVerifyLMXProfiles->setEnabled(false);  // Must disconnect / reconnect to verify.
}

/*!
 \brief Write Frequency Firmware to EEPROM
 EEPROM Setup - Factory Only
*/
/*
void BertWindow::on_buttonWriteFirmwareToEEPROM_clicked()
{
    lockUI(10000, 3);
    qDebug() << "UNLOCK EEPROM...";
    emit SetEEPROMWriteEnable(true);
    qDebug() << "UPDATE EEPROM...";
      emit WriteFirmware(0);
    qDebug() << "RELOCK EEPROM...";
    emit SetEEPROMWriteEnable(false);

//  listFirmwareVersion->clear();
//  listEEPROMFirmware->clear();
//  listEEPROMFirmware->addItems(QStringList({"[Disconnect / Reconnect Required]"}));
    buttonWriteFirmwareToEEPROM->setEnabled(false);
//  buttonVerifyFirmware->setEnabled(false);  // Must disconnect / reconnect to verify.
}

*/

/*!
 \brief Verify Frequency Profiles
 Compares profiles read from files to profiles read from EEPROM
*/
void BertWindow::on_buttonVerifyLMXProfiles_clicked()
{
    emit LMXVerifyFrequencyProfiles();
}

/*!
 \brief Verify Firmware
 Compares Firmware read from files to Firmware read from EEPROM
*/
/*
void BertWindow::on_buttonVerifyFirmware_clicked()
{
    emit EEPROMVerifyFirmware();
}
*/
// *******************************************************************************
// ***** Clock Synth Page ********************************************************
// *******************************************************************************

void BertWindow::frequencyProfileChanged(int index)
{
    lampLMXLockMaster->setState(BertUILamp::ERR);

    if (maxChannel > 4) lampLMXLockSlave->setState(BertUILamp::ERR);
    else                lampLMXLockSlave->setState(BertUILamp::OFF);

    lockUI(5000, 1);
    emit SelectProfile(index);
}




// *******************************************************************************
// ***** Pattern Generator Page **************************************************
// *******************************************************************************


/*!
 \brief PG Channel De-Emphasis setting changed (level or cursor)
 \param lane PG Lane number (0, 2, 4, ...etc)
*/
void BertWindow::pgDemphChanged(int lane)
{
    if (eventsEnabled)
    {
        int channel = BertChannel::laneToChannel(lane);
qDebug() << "BertWindow::pgDemphChanged; lane: " << lane << "; channel: " << channel;

        int levelIndex = getChannel(channel)->getPG()->getDeemphLevelIndex();
        int cursorIndex = getChannel(channel)->getPG()->getDeemphCursorIndex();
        lockUI(2000, 1);
        emit SetDeEmphasis(lane, levelIndex, cursorIndex);
    }
}




// *******************************************************************************
// ***** Error Detector Page *****************************************************
// *******************************************************************************


/*!
 \brief Show / Hide ED or PG Controls
 \param edControlsVisible  true = Show the ED related pages; false = Hide
*/
void BertWindow::showControls(TabType tabType, bool visible)
{
    // Show / Hide Tabs:
    qDebug() << (visible ? "Show" : "Hide") << " tabs for "
             << ((tabType == SMARTEST_PG) ? "PG" : "ED");

    foreach (QWidget *thisTab, tabs)
    {
        TabType thisTabType = static_cast<TabType>(thisTab->property("TabType").toInt());
        if (thisTabType == tabType)
        {
            if (visible && thisTab->parent() == dynamic_cast<QObject *>(hiddenTabs))
            {
                // This tab is currently hidden, and should be displayed:
                QString title = thisTab->property("TabTitle").toString();   // Get the title string for this tab
                int newIndex = tabWidget->count()-1;                        // New tab index: Insert before the "About" tab
                newIndex = tabWidget->insertTab(newIndex, thisTab, title);  // Insert the tab
            }
            else if (!visible && thisTab->parent() != dynamic_cast<QObject *>(hiddenTabs))
            {
                // This tab is currently visible, and should be hidden:
                tabWidget->removeTab(tabWidget->indexOf(thisTab));  // Remove the tab
                thisTab->setParent(hiddenTabs);                     // Move it to the hidden widget on About page (hiddenTabs).
                                                                    // This means that widget searches will still locate it (e.g. List Update).
            }
        }
    }
}



/*!
 \brief Initialise ED Controls to a suitable initial state
*/
void BertWindow::edControlInit()
{
    foreach (BertChannel *bertChannel, bertChannels)
    {
        bertChannel->getED()->setEDErrorFlasher(false);
        bertChannel->edErrorflasherOn = false;
        bertChannel->edOptionsChanged = true;
    }
    buttonEDStart->setEnabled(false);
    buttonEDStop->setEnabled(false);
    edPending = false;
    edRunning = false;
    flagEDStart = false;
    flagEDStop = false;
    flagEDEQChange = false;
    flagEDErrorInject = false;
}

/*!
 \brief Enable or Disable ED Start / Stop buttons as appropriate:
*/
void BertWindow::edStartStopReflect()
{
    if (bertChannels.count() == 0) return;  // Nothing to do (no channels created yet!)
    // Check whether any ED channels are enabled:
    int edEnabledCount = 0;
    foreach (BertChannel *bertChannel, bertChannels)
    {
        if (bertChannel->getED()->getEDEnabled()) edEnabledCount++;
    }
    int pattern = getChannel(1)->getPG()->getPGPatternIndex();
    qDebug() << "ED Pending: " << edPending << "; "
             << "ED Running: " << edRunning << "; "
             << "ED channels enabled: " << edEnabledCount << "; "
             << "Pattern: " << pattern;

    if (!edPending && !edRunning && pattern < 3 && edEnabledCount > 0)
    {
        updateStatus("");
        buttonEDStart->setEnabled(true);
    }
    else
    {
        updateStatus("Note: To use the Error Detector, first select a PRBS pattern (Pattern Generator page) and enable one or more channels.");
        buttonEDStart->setEnabled(false);
    }

    if (edRunning) buttonEDStop->setEnabled(true);
    else           buttonEDStop->setEnabled(false);
}


// ----- Error Detector Data Reset: ---------
/*!
 \brief Reset UI for a specific ED channel
 Clears the UI for a specified ED channel
 \param channel ED channel number
*/
void BertWindow::edResetUI(const int8_t channel)
{
    qDebug() << "ED Reset - Channel: " << channel;
    getChannel(channel)->edErrorflasherOn = false;
    getChannel(channel)->getED()->plotClear();
    getChannel(channel)->getED()->setEDValueBits(0.0);
    getChannel(channel)->getED()->setEDValueErrors(0.0);
    getChannel(channel)->getED()->setEDValueBER(0.0);
}


// ------ Select ALL Channels: ----------------
void BertWindow::on_checkEDEnableAll_clicked(bool checked)
{
    // Note: Only change the ED enabled state if page changes enabled
    // AND the ED is not running:
    if ( (!eventsEnabled) || (edPending) || (edRunning) )
    {
        // Cancel the click:
        checkEDEnableAll->setChecked(!checked);
        return;
    }
    // Enable ALL channels:
    foreach (BertChannel *bertChannel, bertChannels)
    {
        bertChannel->getEDCheckbox()->setChecked(checked);
        bertChannel->edErrorflasherOn = false;
        if (checked) bertChannel->getED()->setState(BertUIEDChannel::STOPPED);
        else         bertChannel->getED()->setState(BertUIEDChannel::DISABLED);
    }
    edStartStopReflect();
}


// ------ Start / Stop / Resync ---------------
void BertWindow::on_buttonEDStart_clicked()
{
    edPending = true;
    flagEDStart = true;
    emit EDStartLed();
}

void BertWindow::on_buttonEDStop_clicked()
{
    flagEDStop = true;
    edStartStopReflect();
    emit EDStopLed();
}


/*!
 \brief UI Update Timer Tick
 This timer fires every 250 ms, and is used to carry out ALL
 time-based updates. This is a better approach than using several
 different timers, because they all run in the same thread anyway,
 and one timer event can interrupt the call from a previous timer
 event, which is confusing.
*/
void BertWindow::uiUpdateTimerTick()
{
    // Check UI unlock timer; Unlock UI if it has expired.
    if (uiUnlockInMs > 0)
    {
        uiUnlockInMs -= 250;
        if (uiUnlockInMs <= 0)
        {
            unlockUI();
        }
    }

    // Find out which tab is currently displayed:
    TabID tabID = static_cast<TabID>(tabWidget->currentWidget()->property("TabID").toInt());

    // qDebug() << "UI Update Timer Tick; Tab = " << tabID;

    // Clear the status text every 5 seconds:
    tickCountStatusTextReset++;
    if (tickCountStatusTextReset >= 20)
    {
        statusMessage->clear();
        tickCountStatusTextReset = 0;
    }

    // Clear the core temperature text every 6 seconds if not updated:
    tickCountTemperatureTextReset++;
    if (tickCountTemperatureTextReset >= 24)
    {
        foreach (BertChannel *bertChannel, bertChannels) bertChannel->resetCoreTemp();
        tickCountTemperatureTextReset = 0;
    }

    // Read the chip temperature every 5 seconds - if we're on the
    // 'Connect' page, and connected.
    if (tabID == TAB_CONNECT)
    {
        tickCountTempUpdate++;
        if (tickCountTempUpdate >= 20)
        {
            // qDebug() << "MainWindow: Chip temperature read Tick...";
            if (uiLockLevel == 0) emit GetTemperature(globals::ALL_LANES);  // Only update temperatures when UI is active (e.g. don't try to update during init process)
            tickCountTempUpdate = 0;
        }
    }

    // Read the LMX clock lock status every second if we are on the Clock Synth page:
    if (tabID == TAB_CLOCKSYNTH && eventsEnabled)
    {
        tickCountClockLockUpdate++;
        if (tickCountClockLockUpdate >= 4)
        {
            // Read from LMX register: emit GetLMXVTuneLock();
            emit ReadLMXLockDetect();
            tickCountClockLockUpdate = 0;
        }
    }

    // ===========================================================================================================================
    // If on CDR Mode Page: Update LOL indicator every second:
    if (commsConnected
     && tabID == TAB_CDR
     && checkCDRModeEnable->isChecked() )
    {
        // qDebug() << "CDR Update timer " << cdrUpdateCounter;

        // Update the CDR Mode Lock Lamps for each channel:
        // If not locked, make the lamp flash.

        foreach (BertChannel *bertChannel, bertChannels)
        {
            if (!bertChannel->hasCDRModeChannel()) continue;  // Not a CDR mode channel
            BertUICDRChannel *cdr = bertChannel->getCDRMode();

            // If Data locked, show data "Lock" lamp:
            if (cdr->getDataLocked())
            {
                cdr->setDataLockLampState(BertUILamp::OK);
            }
            else
            {
                // Flash mode: If CDR mode is enabled but data not locked, flash the "No Lock" lamp:
                if (cdrUpdateCounter == 0)
                {
                    cdr->setDataLockLampState(BertUILamp::OFF);
                }

                if (cdrUpdateCounter == 2)
                {
                    cdr->setDataLockLampState(BertUILamp::ERR);
                }
            }

            // If CDR locked, show CDR "Lock" lamp:
            if (cdr->getCDRLocked())
            {
                cdr->setCDRLockLampState(BertUILamp::OK);
            }
            else
            {
                // Flash mode: If CDR not locked, flash the "No Lock" lamp:
                if (cdrUpdateCounter == 0)
                {
                    cdr->setCDRLockLampState(BertUILamp::OFF);
                }

                if (cdrUpdateCounter == 2)
                {
                    cdr->setCDRLockLampState(BertUILamp::ERR);
                }
            }
        }

        if (cdrUpdateCounter == 3)
        {
            // ----- Read and Updade the LOS / LOL State: -------
            // LOS / LOL lights are only updated if CDR mode enabled.
            // We update the LOS / LOL for any GT1724 core which has a CDR Mode UI section.
            foreach (BertChannel *bertChannel, bertChannels)
            {
                if (bertChannel->hasCDRModeChannel())
                {
                    // qDebug() << "-Emit Get LOS / LOL for meta lane " << bertChannel->getMetaLane();
                    emit GetLosLol(bertChannel->getMetaLane());
                }
            }
        }
    }
    cdrUpdateCounter++;
    if (cdrUpdateCounter >= 4) cdrUpdateCounter = 0;



    // ===========================================================================================================================
    // Code below here updates the ED page, so skip if we're not on that page.
    if (tabID != TAB_ED) return;

    // Read all the flags, and carry out action for the most important one.
    // Flags are set in order of precedence, in case two are set at once.
    bool flagStart = false;
    bool flagStop = false;
    bool flagErrorInject = false;
    bool flagEQChange = false;

    if (flagEDStart)
    {
        flagStart = true;
        flagEDStart = false;
    }
    else if (flagEDStop)
    {
        flagStop = true;
        flagEDStop = false;
    }
    else if (flagEDErrorInject)
    {
        flagErrorInject = true;
        flagEDErrorInject = false;
    }
    else if (flagEDEQChange)
    {
        flagEQChange = true;
        flagEDEQChange = false;
    }
    else if (flagEDDisplayChange)
    {
        foreach (BertChannel *bertChannel, bertChannels)
        {
            if (bertChannel->getED()->getEDEnabled()) bertChannel->getED()->plotClear();
        }
        flagEDDisplayChange = false;
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (flagStart)
    {
        // ED Start: Set the UI to 'running' state:
        edRunning = true;
        edPending = false;
        buttonEDStart->setEnabled(false);
        buttonEDStop->setEnabled(true);
        checkEDEnableAll->setEnabled(false);
        // ED Count Update State Management: Reset. ///////////////
        edEnabledMasterChannels.clear();
        edMasterRequestsPending = 0;
        edEnabledSlaveChannels.clear();
        edSlaveRequestsPending = 0;
        // ////////////////////////////////////////////////////////
        foreach (BertChannel *bertChannel, bertChannels)
        {
            bertChannel->edOptionsChanged = false;
            edResetUI(bertChannel->getChannel());
            bertChannel->getED()->setState(BertUIEDChannel::RUNNING);
            // ED Count Update State Management: Add active master and slave channels: //////
            if (bertChannel->getED()->getEDEnabled())
            {
                if (bertChannel->getBoard() == 0) edEnabledMasterChannels.append(bertChannel->getED());  // Enabled MASTER channel found
                else                              edEnabledSlaveChannels.append(bertChannel->getED());   // Enabled SLAVE channel found
            }
        }
        nextEDMaster = QListIterator<BertUIEDChannel *>(edEnabledMasterChannels);
        nextEDMaster.toBack();
        nextEDSlave = QListIterator<BertUIEDChannel *>(edEnabledSlaveChannels);
        nextEDSlave.toBack();

        // Set up the PRBS checkers IF the settings have been changed and the channel is enabled:
        updateStatus( QString("Synchronizing Pattern...") );
        edSetUpAndStart(true);

        valueMeasurementTime->setText( QString("00:00:00") );
        edRunTime->start();

        updateStatus( QString("ED Started.") );
    }
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (flagStop)
    {
        // ED Stop: DISABLE the PRBS checkers:
        edSetUpAndStart(false);
        // Set the UI to 'Stopped' state:
        buttonEDStart->setEnabled(true);
        buttonEDStop->setEnabled(false);
        checkEDEnableAll->setEnabled(true);
        foreach (BertChannel *bertChannel, bertChannels)
        {
            bertChannel->getED()->setState(BertUIEDChannel::STOPPED);
        }
        edRunning = false;
        updateStatus( QString("ED Stopped.") );
    }
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (flagErrorInject)
    {
        updateStatus(QString("Inject Errors: ED Channel %1").arg(edErrorInjectChannel));
        emit EDErrorInject((edErrorInjectChannel*2)-1);
    }
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    if (flagEQChange)
    {
        qDebug() << "Set EQ Boost for channel " << eqBoostChannel << ": Index = " << eqBoostNewIndex;
        emit SetEQBoost((eqBoostChannel*2)-1, eqBoostNewIndex);
    }
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    if (edUpdateCounter >= 4) edUpdateCounter = 0;
    edUpdateCounter++;

    // --- Update Error Flashers every 0.25s: ----
    if ( (edUpdateCounter == 1) || (edUpdateCounter == 3) )
    {
        if (edRunning)
        {
            foreach (BertChannel *bertChannel, bertChannels) bertChannel->getED()->setEDErrorFlasher(false);
        }
    }
    else
    {
        // Don't check whether the ED is running... I.e. if the ED is stopped
        // and the channel had errors, error light stays on.
        foreach (BertChannel *bertChannel, bertChannels)
        {
            if (bertChannel->edErrorflasherOn) bertChannel->getED()->setEDErrorFlasher(true);
        }
    }

    // --- Update LOS / LOL indicators every second (on off-beat): ---
    if ( (edUpdateCounter == 2) && (commsConnected))
    {
        // ----- Read and Updade the LOS / LOL State: -------
        // LOS / LOL lights are always updated regardless of
        // whether the ED is running. GetLosLol is issued for each
        // GT1724 IC, i.e. lanes 0, 4, 8, ...
        foreach (BertChannel *bertChannel, bertChannels)
        {
            if ((bertChannel->getPGLane() % 4) == 0) emit GetLosLol(bertChannel->getPGLane());
        }
    }  // [if ( (edUpdateCounter == 2) && (commsConnected))]

    // ---- Update TWO CHANNELS of the error counter every 1/4 second:---
    // One from the master board, and one from the slave board (if present)
    if (commsConnected && edRunning)
    {
        // MASTER Channels: ////////////////////////////////////////////////////////
        if (edEnabledMasterChannels.count() > 0 && edMasterRequestsPending < 2)
        {
            BertUIEDChannel *thisEDChannel;
            // There are enabled Master channels, and we don't have more than one outstanding request:
            // We can request error counts for the next master channel.
            if (!nextEDMaster.hasNext()) nextEDMaster.toFront();  // End of channel list reached. Go back to the start.
            thisEDChannel = nextEDMaster.next();
            // Signal the back end to get ED counts for this channel (by Lane):
            qDebug() << "Get ED Counts for MASTER lane " << thisEDChannel->getLane();
            emit GetEDCount(thisEDChannel->getLane(), bitRate);
            edMasterRequestsPending++;
        }
        // SLAVE Channels: ////////////////////////////////////////////////////////
        if (edEnabledSlaveChannels.count() > 0 && edSlaveRequestsPending < 2)
        {
            BertUIEDChannel *thisEDChannel;
            // There are enabled Master channels, and we don't have more than one outstanding request:
            // We can request error counts for the next master channel.
            if (!nextEDSlave.hasNext()) nextEDSlave.toFront();  // End of channel list reached. Go back to the start.
            thisEDChannel = nextEDSlave.next();
            // Signal the back end to get ED counts for this channel (by Lane):
            qDebug() << "Get ED Counts for SLAVE lane " << thisEDChannel->getLane();
            emit GetEDCount(thisEDChannel->getLane(), bitRate);
            edSlaveRequestsPending++;
        }
    }

    // ---- Update run timer, etc, at the end of each second:---
    if ( (edUpdateCounter >= 4) && (commsConnected) )
    {
        if (edRunning)
        {
            // Update run time:
            const int runTimeMs = edRunTime->elapsed();
            int calcRemainder = runTimeMs;
            int runTimeHrs  = calcRemainder / (1000*60*60);
            calcRemainder -= (runTimeHrs * (1000*60*60));
            int runTimeMins = calcRemainder / (1000*60);
            calcRemainder -= (runTimeMins * (1000*60));
            int runTimeSecs = calcRemainder / 1000;
            valueMeasurementTime->setText(QString("%1:%2:%3")
                                              .arg(runTimeHrs,2,10,QLatin1Char('0'))
                                              .arg(runTimeMins,2,10,QLatin1Char('0'))
                                              .arg(runTimeSecs,2,10,QLatin1Char('0')));

        }  // If ED is running and comms connected...
    } // 1 second interval...
}



// ------ ED Channels ---------------

/*!
 * \brief ED Channel has been enabled or disabled
 * \param channel Channel number
 */
void BertWindow::edChannelEnableChanged(const uint8_t channel)
{
    // Note: Only change the ED enabled state if page changes enabled
    // AND the ED is not running:
    if ( (!eventsEnabled) || (edPending) || (edRunning) )
    {
        // Cancel the click:
        getChannel(channel)->getED()->setEDEnabled(!getChannel(channel)->getED()->getEDEnabled());
        return;
    }
    getChannel(channel)->edErrorflasherOn = false;
    foreach (BertChannel *bertChannel, bertChannels)
    {
        if (bertChannel->getED()->getEDEnabled()) bertChannel->getED()->setState(BertUIEDChannel::STOPPED);
        else                                      bertChannel->getED()->setState(BertUIEDChannel::DISABLED);
    }
    edStartStopReflect();
}


/*!
 \brief Set ED channel options and enable (start) or disable (stop) channels
*/
void BertWindow::edSetUpAndStart(bool start)
{
    int channelNoA;
    int channelNoB;
    BertChannel *channelA = nullptr;
    BertChannel *channelB = nullptr;
    int metaLane;
    // We need a pair of channels on the same GT1724 chip to set up the ED.
    // Loop through channel list using a channel number, assuming that odd
    // channel numbers are the first ED channel on each GT1724 chip.
    // Check that there's another channel at the next (even) channel nuber,
    // and start the ED with settings for this pair of channels.
    channelNoA = 1;
    metaLane = 0;
    while (true)
    {
        channelNoB = channelNoA + 1;
        channelA = bertChannels.value(channelNoA);
        channelB = bertChannels.value(channelNoB);
        if (channelA && channelB)
        {
            BertUIEDChannel *edChA = channelA->getED();
            BertUIEDChannel *edChB = channelB->getED();
            bool enableA;
            bool enableB;
            if (start)
            {
                enableA = edChA->getEDEnabled();
                enableB = edChB->getEDEnabled();
            }
            else
            {
                enableA = false;
                enableB = false;
            }
            emit SetEDOptions(metaLane, // Meta Lane (0 = 1st Chip; 4 = 2nd Chip, etc)
                              edChA->getEDPatternIndex(),   // ED Pattern Lanes 0/1
                              edChA->getEDPatternInvert(),  // ED pattern inverted Lanes 0/1
                              enableA,                      // ED Enabled Lanes 0/1
                              edChB->getEDPatternIndex(),   // ED Pattern Lanes 2/3
                              edChB->getEDPatternInvert(),  // ED pattern inverted Lanes 2/3
                              enableB);                     // ED Enabled Lanes 2/3
        }
        else
        {
            // Pair of channels not found. End reached; Give up.
            break;
        }
        channelNoA += 2;
        metaLane += 4;
    }
}



/*!
 \brief ED Page EQ Boost setting changed
 \param channel       ED channel which changed (1 - 4)
 \param eqBoostIndex  New setting
*/
void BertWindow::eqBoostSet(const uint8_t channel, const uint8_t eqBoostIndex)
{
    flagEDEQChange = true;
    eqBoostChannel = channel;
    eqBoostNewIndex = eqBoostIndex;
}

/*!
 \brief Error Injection
 Generate an error pulse by maximising the EQ boost briefly
 \param channel   Channel to apply error inject to (1 - 4)
*/
void BertWindow::errorInject(const uint8_t channel)
{
    flagEDErrorInject = true;
    edErrorInjectChannel = channel;
}








/*************************************************************************
 ******  Eye Scan and Bathtub Plots  *************************************
 *************************************************************************/



/******** Slots used by Eye and Bathtub scans: **********************/

/*!
 \brief Eye Scan Progress Update slot
 \param lane   Lane which
 \param type
 \param progressPercent
*/
void BertWindow::EyeScanProgressUpdate(int lane, int type, int progressPercent)
{
    uint8_t eyeScanChannel = (lane + 1) / 2;

    // Calculate the TOTAL percentage completed, which is the percentage through
    // the current repeat, PLUS the repeats already done:
    uint16_t totalRepeats;
    if (eyeScanRepeatsTotal > 0)  totalRepeats = (uint16_t)eyeScanRepeatsTotal;
    else                          totalRepeats = 0;
    uint16_t doneRepeats = (uint16_t)eyeScanRepeatsDone;

    int totalPercent = progressPercent;
    if (totalRepeats > 0)
    {
        // We are doing several repeats for one or more channels... so we need to calculate percent finished out of TOTAL scans.
        totalPercent = (int)(100.0f * ((float)eyeScansDone / (float)eyeScansTotal)) + ((float)progressPercent / (float)eyeScansTotal);

        updateStatus( QString("%1 Channel %2: %3 % (Scan %4 of %5; %6 % complete overall)")
                      .arg( (type == GT1724::GT1724_EYE_SCAN) ? QString("Eye Scan") : QString("Bathtub Plot") )
                      .arg(eyeScanChannel)
                      .arg(progressPercent,3,10,QChar(' '))
                      .arg(doneRepeats+1,3,10,QChar(' '))
                      .arg(totalRepeats,3,10,QChar(' '))
                      .arg(totalPercent,3,10,QChar(' ')) );
    }
    else
    {
        // Repeat FOREVER:
        updateStatus( QString("%1 Channel %2: %3 % (Scan %4)")
                      .arg( (type == GT1724::GT1724_EYE_SCAN) ? QString("Eye Scan") : QString("Bathtub Plot") )
                      .arg(eyeScanChannel)
                      .arg(progressPercent,3,10,QChar(' '))
                      .arg(doneRepeats+1,3,10,QChar(' ')) );
    }
}


void BertWindow::EyeScanError(int lane, int type, int code)
{
    // Make sure ALL pending scans are cancelled:
    emit EyeScanCancel(globals::ALL_LANES);
    if (code == globals::CANCELLED)
    {
        // Scan cancelled!
        qDebug() << "Eye Scan CANCELLED: Scan type: " << type << "; Lane :" << lane << "; Code: " << code;
        updateStatus("Eye Scan Cancelled.");
    }
    else
    {
        qDebug() << "Eye scan ERROR: Scan type: " << type << "; Lane :" << lane << "; Code: " << code;
        updateStatus(QString("Error running Eye Scan: %1").arg(code));
    }
    eyeScanUIUpdate(false);
    bathtubUIUpdate(false);
}


void BertWindow::EyeScanFinished(int lane, int type, QVector<double> data, int xRes, int yRes)
{
    if (!eyeScanRunning && !bathtubRunning) return;  // Late arrival of update AFTER scan cancel?

    // Plot the results of this scan: Depends whether it is an eye diagram or a bathtub plot.
    int eyeScanChannel = BertChannel::laneToChannel(lane);
    eyeScansDone++;  // Finished one CHANNEL scan (may not be full repeat as there may be other channels to do yet).

    if (type == GT1724::GT1724_EYE_SCAN)
    {
        ///////// EYE DIAGRAM: //////////////////////////////////////////
        getChannel(eyeScanChannel)->getEyescan()->plotShowData(data, xRes, yRes);
    }
    else
    {
        //////// BATHTUB PLOT: //////////////////////////////////////
        getChannel(eyeScanChannel)->getBathtub()->plotShowData(data);
    }

    // Are there any more lanes to scan during THIS repeat cycle?
    bool scanStarted = false;
    if (eyeScanChannel < maxChannel) scanStarted = eyeScanStart(type, eyeScanChannel+1);

    if (!scanStarted)
    {
        // No more channels to scan in THIS repeat.
        eyeScanRepeatsDone++;
        qDebug() << "Finished all scans for this repeat. Repeats Done: " << eyeScanRepeatsDone;
        if (eyeScanRepeatsTotal < 0 ||                    // -1 means repeat forever.
            eyeScanRepeatsDone < eyeScanRepeatsTotal)     // Repeat until we have done the requested number
        {
            scanStarted = eyeScanStart(type, 1);  // Start again from first enabled channel
        }
    }

    if (!scanStarted)
    {
        qDebug() << "Finished all scans.";
        updateStatus("Eye Scan Finished.");
        if (type == GT1724::GT1724_EYE_SCAN) eyeScanUIUpdate(false);
        else                                 bathtubUIUpdate(false);
    }
}




/*!
 \brief Eye Scan UI Config
 Sets up the UI to match the supplied Eye Scan status
 (enables / disables buttons, etc)
 \param isRunning
*/
void BertWindow::eyeScanUIUpdate(bool isRunning)
{
    eyeScanRunning = isRunning;
    buttonEyeScanStart->setEnabled(!isRunning);
    buttonEyeScanStop->setEnabled(isRunning);
    listEyeScanVStep->setEnabled(!isRunning);
    listEyeScanHStep->setEnabled(!isRunning);
    listEyeScanCountRes->setEnabled(!isRunning);
    listEyeScanRepeats->setEnabled(!isRunning);
    checkESEnableAll->setEnabled(!isRunning);
    paneESCheckBoxes->setEnabled(!isRunning);
}



/*!
 \brief Eye Scan Start
 Reads scan settings, then launches a scan for the first
 channel where eyeScanRepeatsDone is less than eyeScanRepeatsTotal
 (i.e. scans remain to be done).
 If all channels have been done, returns false.
 \param channel  Channel number (1-4)
 \param reset    Reset parameters and start a new scan.
                 Used on first call to make sure scan is reset.
 \return true  A scan was started
 \return false No scan started - ALL scans have now been finished.
*/
bool BertWindow::eyeScanStart(int type, int firstChannel)
{
    // Find first enabled channel >= specified first channel:
    bool channelChecked;
    foreach (BertChannel *bertChannel, bertChannels)
    {
        if (bertChannel->getChannel() < firstChannel) continue;
        if (type == GT1724::GT1724_EYE_SCAN) channelChecked = bertChannel->getEyeScanChannelEnabled();
        else                                 channelChecked = bertChannel->getBathtubChannelEnabled();
        if (channelChecked)
        {
            int lane = bertChannel->getEDLane();
            if (bertChannel->eyeScanStartedFlag)
            {
                // Already started eye scans on this channel. Request a repeat:
                emit EyeScanRepeat(lane);
            }
            else
            {
                // Haven't started scanning on this channel yet:
                if (type == GT1724::GT1724_EYE_SCAN)
                {
                    emit EyeScanStart(lane,
                                      GT1724::GT1724_EYE_SCAN,
                                      listEyeScanHStep->currentIndex(),      //  hStep
                                      listEyeScanVStep->currentIndex(),      //  vStep
                                      0,                                     //  vOffset: Unused for Eye Plot
                                      listEyeScanCountRes->currentIndex());  // countRes
                }
                else
                {
                    emit EyeScanStart(lane,
                                      GT1724::GT1724_BATHTUB_SCAN,
                                      0,                                     //  hStep: We always use 1 for Bathtub Plot (no point doing low res scan!)
                                      0,                                     //  vStep: Unused for Bathtub Plot
                                      listBathtubVOffset->currentIndex(),    //  vOffset
                                      listBathtubCountRes->currentIndex());  // countRes

                }
                bertChannel->eyeScanStartedFlag = true;  // First scan started on this channel!
            }
            return true;   // Scan started!
        }
    }
    return false;   // No active channels >= firstChannel!
}


void BertWindow::on_checkESEnableAll_clicked(bool checked)
{
    foreach (BertChannel *bertChannel, bertChannels)
    {
        bertChannel->getEyescanCheckbox()->setChecked(checked);
    }
}


void BertWindow::on_buttonEyeScanStart_clicked()
{
    eyeScanUIUpdate(true);
    bool scanStarted = false;
    eyeScanRepeatsTotal = EYESCAN_REPEATS_LOOKUP[listEyeScanRepeats->currentIndex()];
    eyeScanRepeatsDone = 0;
    eyeScanChannelCount = 0;

    qDebug() << "Eye Scan Start Clicked: Running eye scan with " << eyeScanRepeatsTotal << " repeats.";
    // Count the number of enabled eyescan channels:
    foreach (BertChannel *bertChannel, bertChannels)
    {
        bertChannel->eyeScanStartedFlag = false;
        if  (bertChannel->getEyeScanChannelEnabled())
        {
            bertChannel->getEyescan()->plotClear();  // eyePlt[channel]->clear();
            eyeScanChannelCount++;
        }
    }
    eyeScansTotal = eyeScanChannelCount * eyeScanRepeatsTotal;     // Number of eye scans to do in this run (=[active channels] * [repeats])
    eyeScansDone = 0;                                              // Number of eye scans finished in this run
    if (eyeScanChannelCount == 0)
    {
        // No scan started: No channels selected.
        updateStatus(QString("No channels selected for eye scan."));
    }
    else
    {
        // At least one channel selected! Start scans.
        scanStarted = eyeScanStart(GT1724::GT1724_EYE_SCAN, 1);  // Start from first enabled channel
    }
    if (!scanStarted) eyeScanUIUpdate(false);  // No channels to scan...
}


void BertWindow::on_buttonEyeScanStop_clicked()
{
    emit EyeScanCancel(globals::ALL_LANES);
    updateStatus("Stopping Eye Scanner...");
}





/*******************************************************************
 ******  Bathtub Plot  *********************************************
 *******************************************************************/

/*!
 \brief Bathtub UI Config
 Sets up the UI to match the supplied status
 (enables / disables buttons, etc)
 \param isRunning
*/
void BertWindow::bathtubUIUpdate(bool isRunning)
{
    bathtubRunning = isRunning;
    buttonBathtubStart->setEnabled(!isRunning);
    buttonBathtubStop->setEnabled(isRunning);
    listBathtubVOffset->setEnabled(!isRunning);
    listBathtubCountRes->setEnabled(!isRunning);
    listBathtubRepeats->setEnabled(!isRunning);
    checkBPEnableAll->setEnabled(!isRunning);
    paneBPCheckBoxes->setEnabled(!isRunning);
}


void BertWindow::on_checkBPEnableAll_clicked(bool checked)
{
    foreach (BertChannel *bertChannel, bertChannels)
    {
        bertChannel->getBathtubCheckbox()->setChecked(checked);
    }
}


void BertWindow::on_buttonBathtubStart_clicked()
{
    bathtubUIUpdate(true);
    bool scanStarted = false;
    eyeScanRepeatsTotal = EYESCAN_REPEATS_LOOKUP[listBathtubRepeats->currentIndex()];
    eyeScanRepeatsDone = 0;
    eyeScanChannelCount = 0;
    qDebug() << "Bathtub Scan Start Clicked: Running eye scan with " << eyeScanRepeatsTotal << " repeats.";
    // Count the number of enabled eyescan channels:
    foreach (BertChannel *bertChannel, bertChannels)
    {
        bertChannel->eyeScanStartedFlag = false;
        if  (bertChannel->getBathtubChannelEnabled())
        {
            bertChannel->getBathtub()->plotClear();
            eyeScanChannelCount++;
        }
    }
    eyeScansTotal = eyeScanChannelCount * eyeScanRepeatsTotal;     // Number of eye scans to do in this run (=[active channels] * [repeats])
    eyeScansDone = 0;                                              // Number of eye scans finished in this run
    if (eyeScanChannelCount == 0)
    {
        // No scan started: No channels selected.
        updateStatus( QString("No channels selected for bathtub plot scan.") );
    }
    else
    {
        // At least one channel selected! Start scans.
        scanStarted = eyeScanStart(GT1724::GT1724_BATHTUB_SCAN, 1);  // Start from first enabled channel
    }
    if (!scanStarted) bathtubUIUpdate(false);  // No channels to scan...
}

void BertWindow::on_buttonBathtubStop_clicked()
{
    emit EyeScanCancel(globals::ALL_LANES);
    updateStatus("Stopping Eye Scanner...");
}



// ***********************************************************
//  CDR Mode Page
// ***********************************************************

int BertWindow::cdrModeChanSelIndexToLane(int index)
{
    return (index * 2) + 1;
}

/*!
 \brief Update LOS / LOL lamps on CDR Mode Tab
 \param lane  Source lane (should be 1 or 3)
 \param los   True = Loss Of Signal
 \param lol   True = Loss of Lock (CDR)
*/
void BertWindow::cdrModeLosLolUpdate(int lane, bool los, bool lol)
{
    // We get LOS / LOL data for odd numbered lanes, where
    //  lanes 1, 3 are Core 0
    //  lanes 5, 7 are Core 1, etc.
    // However, each core has one "CDR Mode" channel for which we need
    // to update the Lock lamp, and which lane it maps to depends on
    // which input lane is selected in the controls for the channel.
    // Also, when considering whether we have any CDR mode controls
    // for the lane given in the LOS LOL data, we need to melt the
    // lane number down to the first lane for each core,
    //  i.e. lanes 1, 3 -> 1; lanes 5, 7 -> 5, etc...
    // Because there is only one CDR control for each core.
    int baseLane = (lane - (lane % 4)) + 1;

    int channel = BertChannel::laneToChannel(baseLane);
    BertChannel *bertChannel = getChannel(channel);

    if (!bertChannel->hasCDRModeChannel()) return;  // Not a CDR mode channel

    BertUICDRChannel *cdr = bertChannel->getCDRMode();

    // We need to filter LOS / LOL updated from the Gennum core according to whether
    // they are for the currently selected input lane in the CDR mode controls.
    // Get the index of the selected input lane for this CDR mode channel,
    // and use it to calculate the equivalent lane as returned by the LOS LOL update.
    // We mod lane by 4 because LOS / LOL updates are device-wide lanes, i.e. 1,3,5,7, ...
    // but our CDR mode channel controls only care about lane relative to their core
    // (i.e. 1 or 3).
    int virtualSelectedLane = cdrModeChanSelIndexToLane(cdr->getCDRChannelSelectIndex());

    /* DEBUG: */
    qDebug() << "cdrModeLosLolUpdate: "
             << "lane = " << lane
             << "; los = " << los
             << "; lol = " << lol
             << "; baseLane: " << baseLane
             << "; virtualSelectedLane: " << virtualSelectedLane;
    /* */

    if (cdrModeRunning && (lane % 4) == virtualSelectedLane)
    {
        // Nb: We just set flags for Data and CDR locked.
        // UI updates (if needed) are done in the uiUpdateTimerTick method
        cdr->setDataLocked(!los);
        cdr->setCDRLocked(!los && !lol);
        // qDebug() << "-- Signal lock for lane is now " << cdr->getCDRLocked();
    }
}

/*!
 \brief CDR Mode Enable / Disable
  Global; i.e. not specific to one channel or core.
  CDR Mode is either ON or OFF for all cores and lanes because
  engaging CDR mode requires stopping the clock generator.
*/
void BertWindow::cdrModeEnableChanged(bool isEnabled)
{
    qDebug() << "CDR Mode Enable Changed; Now: " << isEnabled;
    if (isEnabled)
    {
        cdrModeRunning = true;
        qDebug() << "--Disbale LMX Clock...";
        emit SetLMXEnable(false);
        cdrModeSettingsChanged(-1);   // Set up all available CDR mode channels
        // Enable all CDR mode controls
        foreach (BertChannel *bertChannel, bertChannels)
        {
            if (bertChannel->hasCDRModeChannel()) bertChannel->getCDRMode()->setVisualState(true);
        }
        qDebug() << "--OK!";
    }
    else
    {
        // Disable all CDR mode controls
        foreach (BertChannel *bertChannel, bertChannels)
        {
            if (bertChannel->hasCDRModeChannel())
            {
                bertChannel->getCDRMode()->setVisualState(false);
                // Clear the data and CDR locked state for the channel:
                bertChannel->getCDRMode()->setDataLockLampState(BertUILamp::OFF);
                bertChannel->getCDRMode()->setDataLocked(false);
                bertChannel->getCDRMode()->setCDRLockLampState(BertUILamp::OFF);
                bertChannel->getCDRMode()->setCDRLocked(false);
            }
        }
        qDebug() << "--Enbale LMX Clock...";
        emit SetLMXEnable(true);
          // Nb: Enabling the LMX will trigger a full resync event which should restart the PG - see LMX class
        cdrModeRunning = false;
        qDebug() << "--OK!";
    }
}


/*!
 \brief CDR Mode Settings Changed
 Triggered by a change to a setting on the CDR mode page. Resets the
 CDR mode settings accordingly.
 \param core The Gennum core to apply change to (0 or 1 supported,
             depending on how many cores the board has).
             Specifying core = -1 will search for all CDR mode
             channels in the UI and set up each one. This is used
             when the global CDR Mode Enable checkbox is checked.
 */
void BertWindow::cdrModeSettingsChanged(int core)
{
    if (!cdrModeRunning) qDebug() << "CDR Mode Settings Changed but CDR mode not running!";

    if (!cdrModeRunning) return;   // CDR mode not enabled; No point changing settings.

    qDebug() << "CDR Mode Settings Changed; Core: " << core << " (-1 = all)";

    // Loop through all channels; If a channel has CDR Mode controls defined,
    // and that channel matches the core we want to set up, read the CDR mode
    // settings for the channel and set up the gennum core...
    foreach (BertChannel *bertChannel, bertChannels)
    {
        if (!bertChannel->hasCDRModeChannel()) continue;
        int thisCore = bertChannel->getCore();
        if (core == -1 || thisCore == core)
        {
            const int metaLane = bertChannel->getMetaLane();
            qDebug() << "CDR Mode Settings Change for Core " << thisCore << "; metaLane: " << metaLane;

            // Clear the data / cdr locked state for the channel when settings change:
            bertChannel->getCDRMode()->setDataLockLampState(BertUILamp::OFF);
            bertChannel->getCDRMode()->setDataLocked(false);
            bertChannel->getCDRMode()->setCDRLockLampState(BertUILamp::OFF);
            bertChannel->getCDRMode()->setCDRLocked(false);

            const int channelSelIndex = bertChannel->getCDRMode()->getCDRChannelSelectIndex();
            const int divideIndex = bertChannel->getCDRMode()->getCDRDivideRatioIndex();
            qDebug() << "--Set up GT1724 for CDR Mode: channelSelIndex: " << channelSelIndex << "; divideIndex: " << divideIndex;
            emit ConfigCDR(metaLane, cdrModeChanSelIndexToLane(channelSelIndex), divideIndex);
        }
    }
}





/*!
 \brief Main window Close Event
 Window is closing: Make sure the eye monitor is shut down!!
 \param event
*/
void BertWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
}











// ====== Generate UI Layout for Main Window: =====================================================================


QWidget *BertWindow::makeUI(QWidget *parent)
{
    int x, y, vGrid;

    // ======== Connect Tab: =========================================
    x = 25; y = 28;
    BertUIGroup *groupConnButtons = new BertUIGroup("", parent, "Communication", -1, 0, 0, 600, 70);
    new                         BertUILabel  ("", groupConnButtons, "Port:", -1,                         x,       y, 33 );
    listSerialPorts       = new BertUIList   ("listSerialPorts", groupConnButtons, QStringList(), -1,    x+=38,   y, 80 );
    buttonPortListRefresh = new BertUIButton ("buttonPortListRefresh", groupConnButtons, "Refresh", -1,  x+=100,  y, 100);
    buttonConnect         = new BertUIButton ("buttonConnect", groupConnButtons, "Connect", -1,          x+=120,  y, 100);
    buttonResync          = new BertUIButton ("buttonResync", groupConnButtons, "Resync", -1,            x+=120,  y, 100);
    new                         BertUILabel  ("", groupConnButtons, "", -1,                              x+=120,  y, 100);
    buttonResync->setEnabled(false);

    x = 16; y = 20;
    groupTemps = new BertUIGroup("groupTemps", parent, "Device Core Temperatures", -1, 0, 0, 600, 150);
    //groupTemps->setMinimumHeight(50);
    layoutTemps = new QGridLayout(parent);
    groupTemps->setLayout(layoutTemps);

    groupFactoryOptions = nullptr; // Added later if fatory key file found.

    // DEPRECATED layoutConnect = new QVBoxLayout(parent);
    layoutConnect = new QGridLayout(parent);
    /* // DEPRECATED
    layoutConnect->addWidget(groupConnButtons);
    layoutConnect->addWidget(groupTemps);
    layoutConnect->addWidget(new QWidget(parent));  // Filler
    */
    layoutConnect->addWidget(groupConnButtons, 0, 0, 1, 1);
    layoutConnect->addWidget(groupTemps, 1, 0, 1, 1);
    layoutConnect->addWidget(new QWidget(parent), 2, 0, 1, 1);  // Filler


    tabConnect = new QWidget(parent);
    tabConnect->setLayout(layoutConnect);


    // ======== Clock Synth Tab: =========================================
    vGrid = 35;
    x = 16; y = 30;
    groupClock = new BertUIGroup("groupClock", parent, "Frequency Synthesizer Configuration",                 -1,  0, 0, 600, 0);
    groupClock->setMinimumHeight(150);
    new                          BertUILabel  ("", groupClock, "Synthesizer Frequency:",                      -1,  x, y,        135 );
    new                          BertUILabel  ("", groupClock, "Synthesizer Lock:",                             -1,  x, y+=vGrid, 135 );
    new                          BertUILabel  ("", groupClock, "Trigger Out Divide Ratio:",                   -1,  x, y+=vGrid, 135 );
    new                          BertUILabel  ("", groupClock, "Trigger RF Output Power:",                    -1,  x, y+=vGrid, 135 );

    x = 166; y = 30;
    listLMXFreq            = new BertUIList   ("listLMXFreq",            groupClock, QStringList(),           -1,  x, y,        201 );
    lampLMXLockMaster      = new BertUILamp   ("", groupClock, "Lock", "No Lock", BertUILamp::OFF,            -1,  x, y+=vGrid, 100 );
    lampLMXLockSlave       = new BertUILamp   ("", groupClock, "Lock", "No Lock", BertUILamp::OFF,            -1,  x+110, y,    100 );

    listLMXTrigOutDivRatio = new BertUIList   ("listLMXTrigOutDivRatio", groupClock, QStringList(),           -1,  x, y+=vGrid, 91  );
    listLMXTrigOutPower    = new BertUIList   ("listLMXTrigOutPower",    groupClock, QStringList(),           -1,  x, y+=vGrid, 91  );


    layoutClockSynth = new QVBoxLayout(parent);
    layoutClockSynth->addWidget(groupClock);

    tabClockSynth = new QWidget(parent);
    tabClockSynth->setLayout(layoutClockSynth);


    // ======== Pattern Generator Tab: =========================================
    x = 10; y = 6;
    BertUIPane *panePGBitRate = new BertUIPane("", parent, -1, 0, 0, 0, 0);
    panePGBitRate->setMinimumHeight(30);
    panePGBitRate->setMaximumHeight(30);
    new                   BertUILabel    ("",                panePGBitRate, "Bit Rate:", -1, x,      y, 52 );
    valueBitRate_PG = new BertUITextInfo ("valueBitRate_PG", panePGBitRate, "0",         -1, x+=55,  y, 81 );
    new                   BertUILabel    ("",                panePGBitRate, "Gbps",      -1, x+=90,  y, 52 );
    // Channels:
    BertUIPane *panePGChannels = new BertUIPane("", parent, -1, 0, 0, 0, 0);
    layoutPGChannels = new QGridLayout(parent);
    layoutPGChannels->setContentsMargins(0, 0, 0, 0);
    panePGChannels->setLayout(layoutPGChannels);
    QVBoxLayout *layoutPG = new QVBoxLayout(parent);
    layoutPG->addWidget(panePGBitRate);
    layoutPG->addWidget(panePGChannels);
    layoutPG->addWidget(new QWidget(parent));  // Filler to make sure channels stay at their minimum height
    tabPatternGenerator = new QWidget(parent);
    tabPatternGenerator->setLayout(layoutPG);


    // ======== Error Detector Tab: ==========================================
    x = 10; y = 20; vGrid = 30;
    const QStringList resultDisplayItems = { "Accumulated", "Instantaneous" };
    BertUIGroup *groupEDControls = new BertUIGroup("", parent, "Error Detector", -1, 0, 0, 0, 0);
    groupEDControls->setMinimumWidth(131);
    groupEDControls->setMaximumWidth(131);

    new                        BertUILabel    ("",                     groupEDControls, "Rate (Gbps):",     -1,  x+3,  y,        111 );
    valueBitRate_ED      = new BertUITextInfo ("valueBitRate_ED",      groupEDControls, "0",                -1,  x,    y+=25,    111 );
    buttonEDStart        = new BertUIButton   ("buttonEDStart",        groupEDControls, "Start",            -1,  x,    y+=vGrid, 111 );
    buttonEDStop         = new BertUIButton   ("buttonEDStop",         groupEDControls, "Stop",             -1,  x,    y+=vGrid, 111 );
    new                        BertUILabel    ("",                     groupEDControls, "Time:",            -1,  x+3,  y+=vGrid, 35  );
    valueMeasurementTime = new BertUITextInfo ("valueMeasurementTime", groupEDControls, "00:00:00",         -1,  x+39, y,        70  );
    new                        BertUILabel    ("",                     groupEDControls, "Result Display:",  -1,  x,    y+=vGrid, 111 );
    listEDResultDisplay  = new BertUIList     ("listEDResultDisplay",  groupEDControls, resultDisplayItems, -1,  x,    y+=25,    111 );

    // Channel enable checkboxes:
    checkEDEnableAll = new BertUICheckBox ("checkEDEnableAll", groupEDControls, "Enable ALL", -1, 12, y+=vGrid+10, 101    );
    paneEDCheckBoxes = new BertUIPane     ("",                 groupEDControls,               -1, 17, y+=vGrid-4,  110, 0 );
    paneEDCheckBoxes->setMinimumHeight(1);
    paneEDCheckBoxes->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    layoutEDCheckboxes = new QGridLayout(parent);
    layoutEDCheckboxes->setContentsMargins(0, 0, 0, 0);
    paneEDCheckBoxes->setLayout(layoutEDCheckboxes);

    // Channels:
    BertUIPane *paneEDChannels = new BertUIPane("", parent, -1, 0, 0, 0, 0);
    layoutEDChannels = new QGridLayout(parent);
    layoutEDChannels->setContentsMargins(0, 0, 0, 0);
    // Layout:
    paneEDChannels->setLayout(layoutEDChannels);
    QHBoxLayout *layoutED = new QHBoxLayout(parent);
    layoutED->addWidget(groupEDControls);
    layoutED->addWidget(paneEDChannels);
    tabErrorDetector = new QWidget(parent);
    tabErrorDetector->setLayout(layoutED);

    // ======== Eye Scan Tab: ==============================================
    // Controls:
    x = 10; y = 20; vGrid = 30;
    groupEyeScanOpts = new BertUIGroup("", parent, "Eye Scan Options", -1, 0, 0, 0, 0);
    groupEyeScanOpts->setMinimumWidth(131);
    groupEyeScanOpts->setMaximumWidth(131);
    new                        BertUILabel    ("",                      groupEyeScanOpts, "Rate (Gbps):",   -1, x+3,  y,        111);
    valueBitRate_EyeScan = new BertUITextInfo ("valueBitRate_EyeScan",  groupEyeScanOpts, "0",              -1, x,    y+=25,    111);
    buttonEyeScanStart   = new BertUIButton   ("buttonEyeScanStart",    groupEyeScanOpts, "Start",          -1, x,    y+=vGrid, 111);
    buttonEyeScanStop    = new BertUIButton   ("buttonEyeScanStop",     groupEyeScanOpts, "Stop",           -1, x,    y+=vGrid, 111);
    buttonEyeScanStop->setEnabled(false);
    x = 10; y = 45+(vGrid*3);
    new                        BertUILabel    ("",                      groupEyeScanOpts, "Vert. Step:",    -1, x, y,        62 );
    new                        BertUILabel    ("",                      groupEyeScanOpts, "Horiz. Step:",   -1, x, y+=vGrid, 62 );
    new                        BertUILabel    ("",                      groupEyeScanOpts, "Resolution:",    -1, x, y+=vGrid, 62 );
    new                        BertUILabel    ("",                      groupEyeScanOpts, "Repeats:",       -1, x, y+=vGrid, 62 );
    x = 73; y = 45+(vGrid*3);
    listEyeScanVStep     = new BertUIList     ("listEyeScanVStep",      groupEyeScanOpts, QStringList(),    -1, x, y,        51 );
    listEyeScanHStep     = new BertUIList     ("listEyeScanHStep",      groupEyeScanOpts, QStringList(),    -1, x, y+=vGrid, 51 );
    listEyeScanCountRes  = new BertUIList     ("listEyeScanCountRes",   groupEyeScanOpts, QStringList(),    -1, x, y+=vGrid, 51 );
    listEyeScanRepeats   = new BertUIList     ("listEyeScanRepeats",    groupEyeScanOpts, EYESCAN_REPEATS_LIST, -1, x, y+=vGrid, 51 );
    // Channel enable checkboxes:
    checkESEnableAll = new BertUICheckBox ("checkESEnableAll", groupEyeScanOpts, "Scan ALL", -1, 12, y+=vGrid+10, 101    );
    paneESCheckBoxes = new BertUIPane     ("",                 groupEyeScanOpts,             -1, 17, y+=vGrid-4,  110, 0 );
    paneESCheckBoxes->setMinimumHeight(1);
    paneESCheckBoxes->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    layoutESCheckboxes = new QGridLayout(parent);
    layoutESCheckboxes->setContentsMargins(0, 0, 0, 0);
    paneESCheckBoxes->setLayout(layoutESCheckboxes);
    // Channels:
    BertUIPane *paneESChannels = new BertUIPane("", parent, -1, 0, 0, 0, 0);
    layoutESChannels = new QGridLayout(parent);
    layoutESChannels->setContentsMargins(0, 0, 0, 0);
    // Layout:
    paneESChannels->setLayout(layoutESChannels);
    QHBoxLayout *layoutES = new QHBoxLayout(parent);
    layoutES->addWidget(groupEyeScanOpts);
    layoutES->addWidget(paneESChannels);
    //layoutES->setAlignment(groupEyeScanOpts, Qt::AlignTop);
    tabAnalysisEyeScan = new QWidget(parent);
    tabAnalysisEyeScan->setLayout(layoutES);

    // ======== Bathtub Plot Tab: ==============================================
    // Controls:
    x = 10; y = 20;
    vGrid = 30;
    groupBathtubOpts = new BertUIGroup("", parent, "Bathtub Plot Options", -1, 0, 0, 0, 0);
    groupBathtubOpts->setMinimumWidth(131);
    groupBathtubOpts->setMaximumWidth(131);
    new                        BertUILabel    ("",                      groupBathtubOpts, "Rate (Gbps):",   -1, x+3,  y,        111);
    valueBitRate_Bathtub = new BertUITextInfo ("valueBitRate_Bathtub",  groupBathtubOpts, "0",              -1, x,    y+=25,    111);
    buttonBathtubStart   = new BertUIButton   ("buttonBathtubStart",    groupBathtubOpts, "Start",          -1, x,    y+=vGrid, 111);
    buttonBathtubStop    = new BertUIButton   ("buttonBathtubStop",     groupBathtubOpts, "Stop",           -1, x,    y+=vGrid, 111);
    buttonBathtubStop->setEnabled(false);
     x = 10; y = 45+(vGrid*3);
    new                        BertUILabel    ("",                      groupBathtubOpts, "Offset:",        -1, x, y,        62 );
    new                        BertUILabel    ("",                      groupBathtubOpts, "Resolution:",    -1, x, y+=vGrid, 62 );
    new                        BertUILabel    ("",                      groupBathtubOpts, "Repeats:",       -1, x, y+=vGrid, 62 );
    x = 73; y = 45+(vGrid*3);
    listBathtubVOffset   = new BertUIList     ("listBathtubVOffset",    groupBathtubOpts, QStringList(),    -1, x, y,        51 );
    listBathtubCountRes  = new BertUIList     ("listBathtubCountRes",   groupBathtubOpts, QStringList(),    -1, x, y+=vGrid, 51 );
    listBathtubRepeats   = new BertUIList     ("listBathtubRepeats",    groupBathtubOpts, EYESCAN_REPEATS_LIST, -1, x, y+=vGrid, 51 );
    // Channel enable checkboxes:
    y+=vGrid;
    checkBPEnableAll = new BertUICheckBox ("checkBPEnableAll", groupBathtubOpts, "Scan ALL", -1, 12, y+=vGrid+10, 101    );
    paneBPCheckBoxes = new BertUIPane     ("",                 groupBathtubOpts,             -1, 17, y+=vGrid-4,  110, 0 );
    paneBPCheckBoxes->setMinimumHeight(1);
    layoutBPCheckboxes = new QGridLayout(parent);
    layoutBPCheckboxes->setContentsMargins(0, 0, 0, 0);
    paneBPCheckBoxes->setLayout(layoutBPCheckboxes);
    // Channels:
    BertUIPane *panePBChannels = new BertUIPane("", parent, -1, 0, 0, 0, 0);
    layoutBPChannels = new QGridLayout(parent);
    layoutBPChannels->setContentsMargins(0, 0, 0, 0);
    // Layout:
    panePBChannels->setLayout(layoutBPChannels);
    QHBoxLayout *layoutBP = new QHBoxLayout(parent);
    layoutBP->addWidget(groupBathtubOpts);
    layoutBP->addWidget(panePBChannels);
    //layoutBP->setAlignment(groupBathtubOpts, Qt::AlignTop);
    tabAnalysisBathtub = new QWidget(parent);
    tabAnalysisBathtub->setLayout(layoutBP);

    // === CDR Mode Tab: =====================================================
    layoutCDR = new QVBoxLayout(parent);

    paneCDRCheckBoxes  = new BertUIPane     ("", this,  -1, 0, 0,  200, 80 );
    new                      BertUILabel    ("",                   paneCDRCheckBoxes, "Enable CDR Mode",  -1, 16,  30, 135     );
    checkCDRModeEnable = new BertUICheckBox ("checkCDRModeEnable", paneCDRCheckBoxes, "",                 -1, 155, 30, 50      );

    layoutCDR->addWidget(paneCDRCheckBoxes);
    tabCDRControls = new QWidget(parent);
    tabCDRControls->setLayout(layoutCDR);


    // === About Tab: ========================================================
#ifdef BERT_DEMO_CHANNELS
    QString versionNumberExtra = QString(" DEMO");
#else
    QString versionNumberExtra = QString("");
#endif
    tabAbout = new QWidget(parent);
    tabAbout->setMinimumSize(QSize(BertBranding::TAB_WIDTH_MIN, BertBranding::TAB_HEIGHT_MIN));
    x = 20; y = 20; vGrid = 26;
    new BertUIImage    ("", tabAbout, BertBranding::LOGO_FILE_LARGE, x, y, BertBranding::LOGO_SIZE_LARGE.width(), BertBranding::LOGO_SIZE_LARGE.height());
    x = 30; y += BertBranding::LOGO_SIZE_LARGE.height();

    // Information from Instrument:
    new BertUITextArea ("",                              tabAbout, "Model:",              0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("InstrumentModel_0",             tabAbout, "Unknown",             0, x+136, y,        314 , 30);
    new BertUITextArea ("",                              tabAbout, "Serial Number:",      0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("InstrumentSerial_0",            tabAbout, "Unknown",             0, x+136, y,        314 , 30);
    new BertUITextArea ("",                              tabAbout, "Production Date:",    0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("InstrumentProductionDate_0",    tabAbout, "Unknown",             0, x+136, y,        314 , 30);
    new BertUITextArea ("",                              tabAbout, "Calibration Date:",   0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("InstrumentCalibrationDate_0",   tabAbout, "Unknown",             0, x+136, y,        314 , 30);
    new BertUITextArea ("",                              tabAbout, "Warranty Start:",     0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("InstrumentWarrantyStartDate_0", tabAbout, "Unknown",             0, x+136, y,        314 , 30);
    new BertUITextArea ("",                              tabAbout, "Warranty End:",       0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("InstrumentWarrantyEndDate_0",   tabAbout, "Unknown",             0, x+136, y,        314 , 30);
    new BertUITextArea ("",                              tabAbout, "Macro File Version:", 0, x,     y+=vGrid, 135 , 30);
    new BertUITextArea ("MacroVersion_0",                tabAbout, "Unknown",             0, x+136, y,        314 , 30);

    // Application Information:
    new BertUITextArea ("",               tabAbout, "Application Version:",     0, x,     y+=vGrid, 135, 30);
    new BertUITextArea ("",               tabAbout, QString("%1 %2, %3")
                                                    .arg(BertModel::BUILD_VERSION)
                                                    .arg(versionNumberExtra)
                                                    .arg(BertModel::BUILD_DATE),  0, x+136, y,        314, 30);
    // Clock defs version:
    new BertUITextArea ("",                               tabAbout, "Synthesizer Config:",  0, x,     y+=vGrid, 135, 30);
    new BertUITextArea ("InstrumentSynthConfigVersion_0", tabAbout, "Unknown",              0, x+136, y,        314, 30);

    // Copyright Text:
    y += 10;
    new BertUITextArea ("",               tabAbout, BertBranding::ABOUT_BLURB,       -1, x,     y+=vGrid, 550, 160);

    // Invisible widget to temporarily hold hidden tabs:
    hiddenTabs = new QWidget(tabAbout);
    hiddenTabs->setVisible(false);

    // Titles for Tabs:
    const QString TAB_CONNECT_TITLE("Setup Communication");
    const QString TAB_CLOCKSYNTH_TITLE("Clock Synthesizer");
    const QString TAB_PG_TITLE("Pattern Generator Control");
    const QString TAB_ED_TITLE("Error Detector Control");
    const QString TAB_EYESCAN_TITLE("Eye Scan && BER Contour");
    const QString TAB_BATHTUB_TITLE("Bathtub Plot");
    const QString TAB_CDR_TITLE("CDR Mode");
    const QString TAB_ABOUT_TITLE("About");

    // ======== Main Content Tabs: ==========================
    tabWidget = new BertUITabs("tabWidget", parent, 0, 0, 100, 100);
    tabWidget->addTab(tabConnect,          TAB_CONNECT_TITLE);
    tabWidget->addTab(tabClockSynth,       TAB_CLOCKSYNTH_TITLE);
    tabWidget->addTab(tabAbout,            TAB_ABOUT_TITLE);

    // The following tabs are NOT added now; They are hidden until AFTER connect,
    // and only displayed for some instrument types (determined by EEPROM data):
    // * tabPatternGenerator
    // * tabErrorDetector
    // * tabAnalysisEyeScan
    // * tabAnalysisBathtub
    // Instead, these tab widgets are added to a hidden widget on the
    // 'About' tab; This ensures that all signals are connected, etc:
    tabPatternGenerator->setParent(hiddenTabs);
    tabErrorDetector->setParent(hiddenTabs);
    tabAnalysisEyeScan->setParent(hiddenTabs);
    tabAnalysisBathtub->setParent(hiddenTabs);
    tabCDRControls->setParent(hiddenTabs);

    // Set up Tab IDs for easy identification of which tab is selected:
    tabConnect->setProperty          ("TabID", TAB_CONNECT);
    tabClockSynth->setProperty       ("TabID", TAB_CLOCKSYNTH);
    tabPatternGenerator->setProperty ("TabID", TAB_PG);
    tabErrorDetector->setProperty    ("TabID", TAB_ED);
    tabAnalysisEyeScan->setProperty  ("TabID", TAB_EYESCAN);
    tabAnalysisBathtub->setProperty  ("TabID", TAB_BATHTUB);
    tabCDRControls->setProperty      ("TabID", TAB_CDR);
    tabAbout->setProperty            ("TabID", TAB_ABOUT);

    // Give each tab a title property for use when adding hidden tabs:
    tabConnect->setProperty          ("TabTitle", TAB_CONNECT_TITLE);
    tabClockSynth->setProperty       ("TabTitle", TAB_CLOCKSYNTH_TITLE);
    tabPatternGenerator->setProperty ("TabTitle", TAB_PG_TITLE);
    tabErrorDetector->setProperty    ("TabTitle", TAB_ED_TITLE);
    tabAnalysisEyeScan->setProperty  ("TabTitle", TAB_EYESCAN_TITLE);
    tabAnalysisBathtub->setProperty  ("TabTitle", TAB_BATHTUB_TITLE);
    tabCDRControls->setProperty      ("TabTitle", TAB_CDR_TITLE);
    tabAbout->setProperty            ("TabTitle", TAB_ABOUT_TITLE);

    // Give each tab a property to identify what type of instrument function it applies to:
    tabConnect->setProperty          ("TabType", SMARTEST_ALL);
    tabClockSynth->setProperty       ("TabType", SMARTEST_ALL);
    tabPatternGenerator->setProperty ("TabType", SMARTEST_PG);
    tabErrorDetector->setProperty    ("TabType", SMARTEST_ED);
    tabAnalysisEyeScan->setProperty  ("TabType", SMARTEST_ED);
    tabAnalysisBathtub->setProperty  ("TabType", SMARTEST_ED);
    tabCDRControls->setProperty      ("TabType", SMARTEST_ED);  // TODO: SMARTEST_CDR);
    tabAbout->setProperty            ("TabType", SMARTEST_ALL);

    // List of refs to our tab contents. Used when turning tabs on / off later:
    tabs.append(tabConnect);
    tabs.append(tabClockSynth);
    tabs.append(tabPatternGenerator);
    tabs.append(tabErrorDetector);
    tabs.append(tabAnalysisEyeScan);
    tabs.append(tabAnalysisBathtub);
    tabs.append(tabCDRControls);
    tabs.append(tabAbout);


    // ======== Status Area: Status text and Logo ==========
    QWidget     *widgetStatusArea = new QWidget(parent);
    QHBoxLayout *layoutStatusArea = new QHBoxLayout(parent);
    widgetStatusArea->setLayout(layoutStatusArea);
    statusMessage = new BertUIStatusMessage("textStatusMessage", parent, "", 0, 0, 460);
    BertUIImage *logoSmall = new BertUIImage("", parent, BertBranding::LOGO_FILE_SMALL, 0, 0, BertBranding::LOGO_SIZE_SMALL.width(), BertBranding::LOGO_SIZE_SMALL.height());
    layoutStatusArea->setContentsMargins(0, 0, 0, 0);
    layoutStatusArea->addWidget(statusMessage);
    layoutStatusArea->addWidget(logoSmall);

    // ======== Central Widget: Contains everything ========
    //QWidget *widgetMain = new QWidget(parent);
    BertUIBGWidget *widgetMain = new BertUIBGWidget(parent);

    QVBoxLayout *layoutMain = new QVBoxLayout;
    widgetMain->setLayout(layoutMain);
    layoutMain->setContentsMargins(10, 10, 10, 10);
    layoutMain->addWidget(widgetStatusArea);
    layoutMain->addWidget(tabWidget);

    QScrollArea *scrollArea = new QScrollArea(parent);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(widgetMain);
    scrollArea->setWidgetResizable(true);
    return scrollArea;

}



void BertWindow::makeUIChannel(int channel, QWidget *parent)
{
    if (channel > maxChannel) maxChannel = channel;
    if (bertChannels.value(channel)) return;  // Channel already added.

    BertChannel *newChannel = new BertChannel(channel, parent, BertModel::ShowCDRBypassSelect());
    bertChannels.insert(channel, newChannel);
    if (newChannel->getTemperature())
    {
        int temperatureRow = newChannel->getRow() / 2;
        int temperatureCol = newChannel->getRow() % 2;
        layoutTemps->addWidget(newChannel->getTemperature(), temperatureRow, temperatureCol, 1, 1);
        /* Not used:
        if (temperatureCol == 0)
        {
            groupTemps->setMinimumHeight(groupTemps->minimumHeight() + HEIGHT_ADD_TEMPS);
            groupTemps->setMaximumHeight(groupTemps->maximumHeight() + HEIGHT_ADD_TEMPS);
        }
        */
    }

    layoutPGChannels->addWidget(newChannel->getPG(), newChannel->getRow(), newChannel->getCol(), 1, 1);

    layoutEDCheckboxes->addWidget(newChannel->getEDCheckbox(), newChannel->getRow1Col(), 0, 1, 1);
    uiWidgetAddHeight(paneEDCheckBoxes, HEIGHT_ADD_CHECKBOX);
    layoutEDChannels->addWidget(newChannel->getED(), newChannel->getRow(), newChannel->getCol(), 1, 1);
    newChannel->getED()->setState(BertUIEDChannel::DISABLED);

    layoutESCheckboxes->addWidget(newChannel->getEyescanCheckbox(), newChannel->getRow1Col(), 0, 1, 1);
    uiWidgetAddHeight(paneESCheckBoxes, HEIGHT_ADD_CHECKBOX);
    layoutESChannels->addWidget(newChannel->getEyescan(), newChannel->getRow(), newChannel->getCol(), 1, 1);

    layoutBPCheckboxes->addWidget(newChannel->getBathtubCheckbox(), newChannel->getRow1Col(), 0, 1, 1);
    uiWidgetAddHeight(paneBPCheckBoxes, HEIGHT_ADD_CHECKBOX);
    layoutBPChannels->addWidget(newChannel->getBathtub(), newChannel->getRow(), newChannel->getCol(), 1, 1);

    if (newChannel->hasCDRModeChannel())
    {
        layoutCDR->addWidget(newChannel->getCDRMode());
        newChannel->getCDRMode()->setVisualState(false);
    }
}



/*!
 \brief Delete ALL UI Channels
*/
void BertWindow::deleteUIChannels()
{
    foreach (BertChannel *bertChannel, bertChannels) deleteUIChannel(bertChannel);
    bertChannels.clear();
    maxChannel = 0;

    // Hide the VCO lock indicators:
    lampLMXLockMaster->setState(BertUILamp::OFF);
    lampLMXLockSlave->setState(BertUILamp::OFF);
}


/*!
 \brief Delete a channel from the UI
        Deletes all UI elements and frees memory.
        Leaves the supplied BertChannel pointer INVALID (hanging).
 \param bertChannel  Pointer to a BertChannel. This method will DELETE the target
                     of the pointer so it will no longer be valid!
*/
void BertWindow::deleteUIChannel(BertChannel *bertChannel)
{
    // DEPRECATED bool needTempWidgetResize = (bertChannel->hasTemperatureWidget() && (bertChannel->getRow() % 2) == 0);

    // Clean up the channel object (will also remove all widgets):
    delete bertChannel;

    /* // DEPRECATED
    // Reverse any UI layout changes made to accomodate the channel (e.g. size changes):
    if (needTempWidgetResize)
    {
        groupTemps->setMinimumHeight(groupTemps->minimumHeight() - HEIGHT_ADD_TEMPS);
        groupTemps->setMaximumHeight(groupTemps->maximumHeight() - HEIGHT_ADD_TEMPS);
    }
    */

    uiWidgetAddHeight(paneEDCheckBoxes, -1 * HEIGHT_ADD_CHECKBOX);
    uiWidgetAddHeight(paneESCheckBoxes, -1 * HEIGHT_ADD_CHECKBOX);
    uiWidgetAddHeight(paneBPCheckBoxes, -1 * HEIGHT_ADD_CHECKBOX);
}


/*!
 \brief Increase the height of a widget
 \param target     Widget to modify
 \param addHeight  Extra height to add
*/
void BertWindow::uiWidgetAddHeight(QWidget *target, int addHeight)
{
    target->setGeometry(target->x(), target->y(), target->width(), target->height() + addHeight);
    target->updateGeometry();
}


/*!
 \brief Show Factory Options - EEPROM set up, etc.
 \param parent  Widget to add new controls to
*/
void BertWindow::makeUIFactoryOptions(QWidget *parent)
{
    Q_ASSERT(groupFactoryOptions == nullptr);
    if (groupFactoryOptions != nullptr) return;  // Already added!

    int x = 16;
    int y = 20;
    int vGrid = 30;
    groupFactoryOptions = new BertUIGroup("", parent, "Write EEPROM Data", -1, 0, 0, 600, 560);
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Model:",               0, x,     y,        135 );
    inputModel           = new BertUITextInput ("SetInstrumentModel_0",             groupFactoryOptions, "",                     0, x+136, y,        155 );
    listEEPROMModel      = new BertUIList      ("listEEPROMModel",                  groupFactoryOptions, BertModel::BERT_MODELS, 0, x+296, y,        140 );
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Max 20 chrs.",         0, x+460, y,        100 );

    new                        BertUILabel     ("",                                 groupFactoryOptions, "Serial Number:",      0, x,     y+=vGrid, 135 );
    inputSerial          = new BertUITextInput ("SetInstrumentSerial_0",            groupFactoryOptions, "",                    0, x+136, y,        250 );
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Max 50 chrs.",        0, x+460, y,        100 );

    new                        BertUILabel     ("",                                 groupFactoryOptions, "Production Date:",    0, x,     y+=vGrid, 135 );
    inputProdDate        = new BertUITextInput ("SetInstrumentProductionDate_0",    groupFactoryOptions, "",                    0, x+136, y,        155 );
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Max 20 chrs.",        0, x+460, y,        100 );

    new                        BertUILabel     ("",                                 groupFactoryOptions, "Calibration Date:",   0, x,     y+=vGrid, 135 );
    inputCalDate         = new BertUITextInput ("SetInstrumentCalibrationDate_0",   groupFactoryOptions, "",                    0, x+136, y,        155 );
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Max 20 chrs.",        0, x+460, y,        100 );

    new                        BertUILabel     ("",                                 groupFactoryOptions, "Warranty Start:",     0, x,     y+=vGrid, 135 );
    inputWarrantyStart   = new BertUITextInput ("SetInstrumentWarrantyStartDate_0", groupFactoryOptions, "",                    0, x+136, y,        155 );
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Max 20 chrs.",        0, x+460, y,        100 );

    new                        BertUILabel     ("",                                 groupFactoryOptions, "Warranty End:",       0, x,     y+=vGrid, 135 );
    inputWarrantyEnd     = new BertUITextInput ("SetInstrumentWarrantyEndDate_0",   groupFactoryOptions, "",                    0, x+136, y,        155 );
    new                        BertUILabel     ("",                                 groupFactoryOptions, "Max 20 chrs.",        0, x+460, y,        100 );

    new                           BertUILabel     ("",                                   groupFactoryOptions, "Synth Config Vers:",  0, x,     y+=vGrid, 135 );
    inputSynthConfigVersion = new BertUITextInput ("SetInstrumentSynthConfigVersion_0",  groupFactoryOptions, "",                    0, x+136, y,        250 );
    new                           BertUILabel     ("",                                   groupFactoryOptions, "Max 50 chrs.",        0, x+460, y,        100 );

    buttonEEPROMDefaults = new BertUIButton    ("buttonEEPROMDefaults",             groupFactoryOptions, "Use Defaults",        0, x+136, y+=vGrid, 120 );
    buttonWriteEEPROM    = new BertUIButton    ("buttonWriteEEPROM",                groupFactoryOptions, "Write to EEPROM",     0, x+276, y,        120 );


    new                               BertUILabel  ("",                            groupFactoryOptions, "LMX Frequency Profiles:",      0, x,     y+=vGrid, 135 );
    new                               BertUILabel  ("",                            groupFactoryOptions, "Defs from FILES:",             0, x,     y+=vGrid, 135 );
    listTCSLMXProfiles          = new BertUIList   ("listTCSFreq",                 groupFactoryOptions, QStringList(),                  0, x+136, y,        250 );
    new                               BertUILabel  ("",                            groupFactoryOptions, "Defs from EEPROM:",            0, x,     y+=vGrid, 135 );
    listEEPROMLMXProfiles       = new BertUIList   ("listEEPROMLMXFreq",           groupFactoryOptions, QStringList(),                  0, x+136, y,        250 );
    buttonWriteProfilesToEEPROM = new BertUIButton ("buttonWriteProfilesToEEPROM", groupFactoryOptions, "Write ALL TCS Defs to EEPROM", 0, x+136, y+=vGrid, 250 );
    buttonVerifyLMXProfiles     = new BertUIButton ("buttonVerifyLMXProfiles",     groupFactoryOptions, "Verify Clock Defs",            0, x+136, y+=vGrid, 250 );

//  new                               BertUILabel  ("",                            groupFactoryOptions, "Firmware Download:",           0, x,     y+=vGrid, 135 );
//  new                               BertUILabel  ("",                            groupFactoryOptions, "Firmware from FILES:",         0, x,     y+=vGrid, 135 );
//  listFirmwareVersion         = new BertUIList   ("FirmwareVersion",             groupFactoryOptions, BertModel::BERT_FIRMWARES,      0, x+136, y,        250 );
 // new                               BertUILabel  ("",                            groupFactoryOptions, "Fireware from EEPROM:",        0, x,     y+=vGrid, 135 );
 // listEEPROMFirmware          = new BertUIList   ("EEPROMFirmware",              groupFactoryOptions, QStringList(),                  0, x+136, y,        250 );
 // buttonWriteFirmwareToEEPROM = new BertUIButton ("buttonWriteFirmwareToEEPROM", groupFactoryOptions, "Write Fireware to EEPROM",     0, x+136, y+=vGrid, 250 );
 // buttonVerifyFirmware        = new BertUIButton ("buttonVerifyFirmware",        groupFactoryOptions, "Verify Firmware",              0, x+136, y+=vGrid, 250 );

    layoutConnect->addWidget(groupFactoryOptions, 0, 1, 3, 1);
}


/*!
 \brief Add place holder for Factory Options
 This is called when factory options are NOT enabled, to make sure
 layout stays the same for other elements.
 \param parent  Widget to add new controls to
*/
void BertWindow::makeUIDummyFactoryOptions(QWidget *parent)
{
    Q_UNUSED(parent)
    layoutConnect->addWidget(new QWidget(parent), 0, 1, 3, 1);  // Filler to make sure connect controls stay to the left side.
}


/*!
 \brief Check for Factory Mode key file
 \return true  - Factory mode key file found. Enable factory mode.
 \return false - Factory key not found. Normal mode.
*/
bool BertWindow::checkFactoryKey()
{
    return true;

    int result;
    QString keyFile = globals::getAppPath() + QString("\\factory.key");
    qDebug() << "Checking for factory key file...";
    QStringList keyFileLines;
    result = BertFile::readFile(keyFile, 100, keyFileLines);
    if (result != globals::OK)
    {
        qDebug() << "Key not found (" << result << ")";
        return false;  // Key file not found.
    }
    if (keyFileLines.length() < 1)
    {
        qDebug() << "Empty key file!";
        return false;  // Key error

    }

    QCryptographicHash keyHash(QCryptographicHash::Sha3_256);
    keyHash.addData(keyFileLines[0].toUtf8());

    // qDebug() << "Hash Calculated:";
    // qDebug() << keyHash.result().toHex();

    if (keyHash.result().toHex() == globals::FACTORY_KEY_HASH.toUtf8())
    {
        qDebug() << "Key Match!";
        return true;
    }
    else
    {
        qDebug() << "Key Mismatch.";
        return false;
    }

}


/*!
 \brief Make UI contols for Reference Clock generator (SI5340) (selected models only)
*/
void BertWindow::makeUIRefClock()
{
    Q_ASSERT(groupRefClock == nullptr);
    if (groupRefClock != nullptr) return;  // Already added!

qDebug() << "*** makeUIRefClock...";

    int x = 16;
    int y = 20;
    int vGrid = 35;
    groupRefClock = new BertUIGroup("", this, "Synthesizer Reference", -1, 0, 0, 600, 550);
    groupRefClock->setMinimumHeight(150);
    new                          BertUILabel  ("",                     groupRefClock, "Reference Clock:",  -1,  x, y,        135 );
    new                          BertUILabel  ("",                     groupRefClock, "Ref In:",           -1,  x, y+=vGrid, 135 );
    new                          BertUILabel  ("",                     groupRefClock, "Ref Out:",          -1,  x, y+=vGrid, 135 );

    x = 166; y = 20;
    listRefClockProfiles   = new BertUIList     ("listRefClockProfiles",  groupRefClock, QStringList(),    -1,  x, y,        201 );
    valueRefClockFreqIn    = new BertUITextInfo ("valueRefClockFreqIn",   groupRefClock, "",               -1,  x, y+=vGrid, 201 );
    valueRefClockFreqOut   = new BertUITextInfo ("valueRefClockFreqOut",  groupRefClock, "",               -1,  x, y+=vGrid, 201 );

 //   layoutClockSynth = new QVBoxLayout(parent);
    layoutClockSynth->addWidget(groupRefClock);

//    tabClockSynth = new QWidget();
//    tabClockSynth->setLayout(layoutClockSynth);

    // Manually connect "currentIndexChanged" signal from Profiles list:
    // This won't be connected automatically since the UI is only created later
    // (after SI5340 hardware is discovered).
    connect(listRefClockProfiles, SIGNAL(currentIndexChanged(int)), this, SLOT(listRefClockProfiles_currentIndexChanged(int)));                        \

}


/*!
 \brief Delete UI controls for Reference Clock generator (SI5340) (selected models only)
*/
void BertWindow::deleteUIRefClock()
{
    Q_ASSERT(groupRefClock != nullptr);
    if (groupRefClock == nullptr) return;  // Nothing to delete!

qDebug() << "*** deleteUIRefClock...";

    layoutClockSynth->removeWidget(groupRefClock);

    delete valueRefClockFreqOut;
    valueRefClockFreqOut = nullptr;

    delete valueRefClockFreqIn;
    valueRefClockFreqIn = nullptr;

    delete listRefClockProfiles;
    listRefClockProfiles = nullptr;

    delete groupRefClock;
    groupRefClock = nullptr;
}
