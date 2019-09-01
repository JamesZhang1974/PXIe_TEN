/*!
 \file   BertWorker.h
 \brief  Class to create a worker thread to operate the Bert instrument functions.
         This is used to separate the UI thread from the back end 'heavy lifting'.

         The Worker thread handles connecting to the instrument, which is a "meta"
         connect (open the serial port and also check that the I2C adaptor is
         working, etc). This is handled here because it's not a specific hardware
         component of the system. Signals and slots for "connect" are provided here.
         Other signals and slots are provided through BertComponent and its subclasses.
         Subclasses of Bert Component implement control functions for each hardware
         component, using signals and slots.

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#ifndef BERTWORKER_H
#define BERTWORKER_H

#include <memory>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QList>

#include "I2CComms.h"
#include "TLC59108.h"
#include "GT1724.h"
#include "LMX2594.h"
#include "PCA9557B.h"
#include "PCA9557A.h"
#include "M24M02.h"
#include "SI5340.h"


class BertWorker : public QThread
{
    Q_OBJECT

public:
    BertWorker();
    ~BertWorker();


/*
  Worker 'Connect' process signalling order:

  Client (UI)                               Worker

  Connect Clicked  ---(CommsConnect)---->  *Open serial port
                                           *Search for hardware
  Hook up signals <--(ComponentXAdded)--
  for X
  Hook uo signals <--(ComponentYAdded)--
  for Y

                  <--(StatusConnect)----

  Ready for        ---(GetOptions)------>  *Get options and lists for
  options!                                  each component

  Populate        <--(ListPopulate A)---
  lists, etc      <--(ListPopulate B)---
                                           All options sent...
                  <--(OptionsSent)------

  Ready to        --(InitComponents)--->  *Init called for each
  Init!                                    component
                  <--(UI Update sigs)--   *Components may emit signals
                                           to update UI display during init
  Activate UI     <---(Result)---------   *Init finished!

*/

#define BERT_WORKER_SIGNALS \
    void WorkerResult(int result);                                 \
    void WorkerShowMessage(QString message, bool append = false);  \
    void ListSerialPorts(QStringList ports);                       \
    void TLC59108Added(TLC59108 *tlc59108, int deviceID );         \
    void GT1724Added(GT1724 *gt1724, int laneOffset);              \
    void LMX2594Added(LMX2594 *lmx2594, int deviceID);             \
    void PCA9557B_Added(PCA9557B *pca9557b, int deviceID);         \
    void PCA9557A_Added(PCA9557A *pca9557a, int deviceID);         \
    void M24M02Added(M24M02 *m24m02, int deviceID);                \
    void SI5340Added(SI5340 *si5340, int deviceID );         \
    void StatusConnect(bool connected);                            \
    void OptionsSent();                                            \


#define BERT_WORKER_SLOTS \
    void RefreshSerialPorts();       \
    void CommsConnect(QString port); \
    void CommsDisconnect();          \
    void GetOptions();               \
    void InitComponents();           \
    void WorkerStop();

#define BERT_WORKER_CONNECT_SIGNALS(CLIENT, WORKER) \
    connect(WORKER, SIGNAL(WorkerResult(int)),                CLIENT, SLOT(WorkerResult(int)));                \
    connect(WORKER, SIGNAL(WorkerShowMessage(QString, bool)), CLIENT, SLOT(WorkerShowMessage(QString, bool))); \
    connect(WORKER, SIGNAL(ListSerialPorts(QStringList)),     CLIENT, SLOT(ListSerialPorts(QStringList)));     \
    connect(WORKER, SIGNAL(TLC59108Added(TLC59108 *, int)),   CLIENT, SLOT(TLC59108Added(TLC59108 *, int)));   \
    connect(WORKER, SIGNAL(GT1724Added(GT1724 *, int)),       CLIENT, SLOT(GT1724Added(GT1724 *, int)));       \
    connect(WORKER, SIGNAL(LMX2594Added(LMX2594 *, int)),     CLIENT, SLOT(LMX2594Added(LMX2594 *, int)));     \
    connect(WORKER, SIGNAL(PCA9557B_Added(PCA9557B *, int)),  CLIENT, SLOT(PCA9557B_Added(PCA9557B *, int)));   \
    connect(WORKER, SIGNAL(PCA9557A_Added(PCA9557A *, int)),  CLIENT, SLOT(PCA9557A_Added(PCA9557A *, int)));   \
    connect(WORKER, SIGNAL(M24M02Added(M24M02 *, int)),       CLIENT, SLOT(M24M02Added(M24M02 *, int)));       \
    connect(WORKER, SIGNAL(SI5340Added(SI5340 *, int)),       CLIENT, SLOT(SI5340Added(SI5340 *, int)));       \
    connect(WORKER, SIGNAL(StatusConnect(bool)),              CLIENT, SLOT(StatusConnect(bool)));              \
    connect(WORKER, SIGNAL(OptionsSent()),                    CLIENT, SLOT(OptionsSent()));                    \
    connect(CLIENT, SIGNAL(RefreshSerialPorts()),             WORKER, SLOT(RefreshSerialPorts()));             \
    connect(CLIENT, SIGNAL(CommsConnect(QString)),            WORKER, SLOT(CommsConnect(QString)));            \
    connect(CLIENT, SIGNAL(CommsDisconnect()),                WORKER, SLOT(CommsDisconnect()));                \
    connect(CLIENT, SIGNAL(GetOptions()),                     WORKER, SLOT(GetOptions()));                     \
    connect(CLIENT, SIGNAL(InitComponents()),                 WORKER, SLOT(InitComponents()));                 \
    connect(CLIENT, SIGNAL(WorkerStop()),                     WORKER, SLOT(WorkerStop()));

signals:
    BERT_WORKER_SIGNALS

public slots:
    BERT_WORKER_SLOTS

private slots:
    void slotTimerTick();

private:
    void run();
    int  findAndInitEEPROM();
    int  findComponents();
    void getComponentOptions();
    int  initComponents();
    void shutdownComponents();

    bool flagStop;
    bool flagWorkerReady;

    // Comms Layer: I2C Comms class
    I2CComms *comms = NULL;

    // Components of the BERT System:
    QList<M24M02 *>  m24m02Set;    // There will be 1 x M24M02 IC per board (Serial EEPROM; defines model details)
    QList<GT1724 *>  gt1724Set;    // There will be 2 x GT chips per board
    QList<LMX2594 *> lmxClockSet;  // There will be 1 x LMX clock gen per board
    QList<PCA9557B *> pca9557bSet;   // There will be 1 x PCA9557B IC per board
    QList<PCA9557A *> pca9557aSet;   // There will be 1 x PCA9557A IC per board
    QList<SI5340 *>  si5340Set;    // There may be 1 x SI5340 IC per board (selected models only)
    QList<TLC59108 *>  tlc59108Set;    // There may be 1 x SI5340 IC per board (selected models only)
};

#endif // BERTWORKER_H
