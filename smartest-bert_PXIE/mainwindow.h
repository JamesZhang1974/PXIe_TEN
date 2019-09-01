#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCoreApplication>
#include <QMainWindow>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QSize>
#include <QDebug>
#include <QList>
#include <QListIterator>
#include <QMap>
#include <QDate>
#include <QCryptographicHash>

#include <math.h>

#include "widgets/BertUIPGChannel.h"
#include "widgets/BertUIEDChannel.h"
#include "widgets/BertUIEyescanChannel.h"
#include "widgets/BertUIBathtubChannel.h"

#include "widgets/BertUIButton.h"
#include "widgets/BertUIBGWidget.h"
#include "widgets/BertUICDRChannel.h"
#include "widgets/BertUICheckBox.h"
#include "widgets/BertUIEDChannel.h"
#include "widgets/BertUIGroup.h"
#include "widgets/BertUIImage.h"
#include "widgets/BertUILabel.h"
#include "widgets/BertUILamp.h"
#include "widgets/BertUIList.h"
#include "widgets/BertUIPane.h"
#include "widgets/BertUIPGChannel.h"
#include "widgets/BertUIStatusMessage.h"
#include "widgets/BertUITabs.h"
#include "widgets/BertUITextArea.h"
#include "widgets/BertUITextInfo.h"
#include "widgets/BertUITextInput.h"
#include "widgets/BertUIPlotEye.h"

#include "globals.h"
#include "BertBranding.h"
#include "BertModel.h"

#include "BertChannel.h"
#include "BertWorker.h"
#include "BertFile.h"
#include "LMXFrequencyProfile.h"




class BertWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BertWindow(QWidget *parent = 0);
    ~BertWindow();

    void enablePageChanges();

signals:
    // Signals for worker thread:
    BERT_WORKER_SLOTS

    // Signals for GT1724:
    GT1724_SLOTS

    // Signals for LMX Clock IC:
    LMX2594_SLOTS

    // Signals for the PCA9557A IC:
    PCA9557A_SLOTS

    // Signals for the PCA9557B IC:
    PCA9557B_SLOTS

    // Signals for the M24M02 EEPROM IC:
    M24M02_SLOTS

    // Signals for the SI5340 Reference Clock IC:
    SI5340_SLOTS

    // Signals for the TLC59108 LED IC:
    TLC59108_SLOTS

private slots:

    // Signals from worker:
    BERT_WORKER_SIGNALS

    // General signals from device components:
    BERT_COMPONENT_SIGNALS

    // Signals from GT1724:
    GT1724_SIGNALS

    // Signals from LMX clock IC:
    LMX2594_SIGNALS

    // Signals from PCA9557A IC:
    PCA9557A_SIGNALS


    // Signals from M24M02 EEPROM IC:
    M24M02_SIGNALS

    // Signals for the SI5340 Reference Clock IC:
    SI5340_SIGNALS


    //*************************************************************
    // MACROS to tidy up boiler-plate event handling code
    // Generic UI event (e.g. combo box change):
    //   * Check that UI events are enabled
    //   * Lock / disable the UI while action is carried out
    //   * Emit the specified signal to start the action
    #define UI_EVENT(TIMEOUT, DEPTH, SIG) \
    {                                     \
        if (eventsEnabled)                \
        {                                 \
            lockUI(TIMEOUT, DEPTH);       \
            emit SIG;                     \
        }                                 \
    }

    // Run a function only if UI events are enabled:
    #define IF_UI_ENABLED(ACTION) \
    {                             \
        if (eventsEnabled)        \
        {                         \
            ACTION;               \
        }                         \
    }
    //*************************************************************


    // ==== Slots for UI events: ==============================
    void uiUpdateTimerTick();

    // --- Tab Widget: -----------------
    void on_tabWidget_tabBarClicked(int index);
    void on_tabWidget_currentChanged(int index);

    // --- Connect Page: ---------------
    void on_buttonPortListRefresh_clicked();
    void on_buttonConnect_clicked();
    void on_buttonResync_clicked();
    // DEBUG ONLY void on_buttonCommsCheck_clicked();
    void on_buttonEEPROMDefaults_clicked();
    void on_buttonWriteEEPROM_clicked();
    void on_listEEPROMModel_currentIndexChanged(int index);
    void on_buttonWriteProfilesToEEPROM_clicked();
//    void on_buttonWriteFirmwareToEEPROM_clicked();
    void on_buttonVerifyLMXProfiles_clicked();

    // --- Frequency Synth Page: ---------
    void on_listLMXFreq_currentIndexChanged(int index)             IF_UI_ENABLED(frequencyProfileChanged(index))
    void on_listLMXTrigOutDivRatio_currentIndexChanged(int index)  UI_EVENT(2000, 1, SelectTriggerDivide(index))
    void on_listLMXTrigOutPower_currentIndexChanged(int index)     UI_EVENT(2000, 1, ConfigureOutputs(-1, index, true))

    void listRefClockProfiles_currentIndexChanged(int index)       UI_EVENT(5000, 1, RefClockSelectProfile(index, true))


    // --- PG Page: ---------------
    void boolPGLaneOn_clicked                   (int lane, bool checked)  UI_EVENT(2000, 1, SetLaneOn(lane, checked, false))
    void boolPGLaneInverted_clicked             (int lane, bool checked)  UI_EVENT(2000, 1, SetLaneInverted(lane, checked))
    void listPGAmplitude_currentIndexChanged    (int lane, int index)     UI_EVENT(2000, 1, SetOutputSwing(lane, index))
    void listPGPattern_currentIndexChanged      (int lane, int index)     UI_EVENT(5000, 1, ConfigPG(lane, index, bitRate))
    void listPGDeemphLevel_currentIndexChanged  (int lane, int index)     IF_UI_ENABLED(pgDemphChanged(lane); Q_UNUSED(index))
    void listPGDeemphCursor_currentIndexChanged (int lane, int index)     IF_UI_ENABLED(pgDemphChanged(lane); Q_UNUSED(index))
    void listPGCrossPoint_currentIndexChanged   (int lane, int index)     UI_EVENT(2000, 1, SetCrossPoint(lane, index))
    void listPGCDRBypass_currentIndexChanged    (int lane, int index)     UI_EVENT(2000, 1, SetForceCDRBypass(lane, index, bitRate))

    // --- ED Page: ---------------
    void on_buttonEDStart_clicked();
    void on_buttonEDStop_clicked();
    void on_listEDResultDisplay_currentIndexChanged(int index)         IF_UI_ENABLED(flagEDDisplayChange = true; Q_UNUSED(index))
    void on_checkEDEnableAll_clicked(bool checked);

    void boolEDEnable_clicked              (int channel, bool checked) IF_UI_ENABLED(edChannelEnableChanged(channel); Q_UNUSED(checked))
    void listEDPattern_currentIndexChanged (int channel, int index)    IF_UI_ENABLED(getChannel(channel)->edOptionsChanged = true; Q_UNUSED(index))
    void boolEDPatternInvert_clicked       (int channel, bool checked) IF_UI_ENABLED(getChannel(channel)->edOptionsChanged = true; Q_UNUSED(checked))
    void listEDEQBoost_currentIndexChanged (int channel, int index)    IF_UI_ENABLED(eqBoostSet(channel, index))
    void buttonEDInjectError_clicked       (int channel) { errorInject(channel); }

    // --- Eyescan Page: ---------------
    void on_buttonEyeScanStart_clicked();
    void on_buttonEyeScanStop_clicked();
    void on_checkESEnableAll_clicked(bool checked);

    // --- Bathtub Page: ---------------
    void on_buttonBathtubStart_clicked();
    void on_buttonBathtubStop_clicked();
    void on_checkBPEnableAll_clicked(bool checked);

    // --- CDR Mode Page: --------------
    void listCDRChannelSelect_currentIndexChanged(int core, int index) IF_UI_ENABLED(cdrModeSettingsChanged(core); Q_UNUSED(index))
    void listCDRDivideRatio_currentIndexChanged(int core, int index)   IF_UI_ENABLED(cdrModeSettingsChanged(core); Q_UNUSED(index))
    // JCB OLD void checkCDRModeEnable_clicked(int core, bool checked)            IF_UI_ENABLED(cdrModeSettingsChanged(core); Q_UNUSED(checked))
    void on_checkCDRModeEnable_clicked(bool checked)                      IF_UI_ENABLED(cdrModeEnableChanged(checked))

 private:
    // Tab Identifiers: Stored with each tab widget as a dynamic property
    // called "TabID" so that we can tell later which tab we're looking at.
    enum TabID
    {
        TAB_CONNECT,
        TAB_CLOCKSYNTH,
        TAB_PG,
        TAB_ED,
        TAB_EYESCAN,
        TAB_BATHTUB,
        TAB_CDR,
        TAB_ABOUT
    };

    // Type identifiers for tabs:
    // What type of instrument function does this tab apply to?
    enum TabType
    {
        SMARTEST_ALL,  // All types of instrument
        SMARTEST_PG,   // Pattern generator functions
        SMARTEST_ED,   // Error detector functions
        SMARTEST_CDR   // Error detector functions
    };

    QWidget *makeUI(QWidget *parent);
    void makeUIChannel(int channel, QWidget *parent);
    void deleteUIChannel(BertChannel *bertChannel);
    void deleteUIChannels();
    void uiWidgetAddHeight(QWidget *target, int addHeight);
    bool checkFactoryKey();
    void makeUIFactoryOptions(QWidget *parent);
    void makeUIDummyFactoryOptions(QWidget *parent);
    void makeUIRefClock();
    void deleteUIRefClock();

    void updateStatus(QString text);
    void appendStatus(QString text);

    void listPopulate(QString name, int lane, QStringList items, int defaultIndex);

    template<class T> T findItem(QString name, int suffix = -1);

    BertChannel *getChannel(int channel);

    void lockUI(int timeoutMs, int depth = 1);
    void unlockUI();
    void uiChangeOnConnect(bool connectedStatus);

    void frequencyProfileChanged(int index);

    void pgDemphChanged(int level);

    void showControls(TabType tabType, bool isVisible);

    void edControlInit();
    void edStartStopReflect();
    void edResetUI(const int8_t channel);
    void edChannelEnableChanged(const uint8_t channel);
    void edSetUpAndStart(bool start);
    void eqBoostSet(const uint8_t channel, const uint8_t eqBoostIndex);
    void errorInject(const uint8_t channel);

    void eyeScanUIUpdate(bool isRunning);
    bool eyeScanStart(int type, int firstChannel);

    void bathtubUIUpdate(bool isRunning);

    void cdrModeLosLolUpdate(int lane, bool los, bool lol);
    void cdrModeEnableChanged(bool isEnabled);
    void cdrModeSettingsChanged(int core);
    static int cdrModeChanSelIndexToLane(int index);

    void closeEvent(QCloseEvent *event);

    static const QStringList EYESCAN_REPEATS_LIST;     // List of options for "Repeats" list (Eyescan and Bathtub plot)
    static const QList<int>  EYESCAN_REPEATS_LOOKUP;   // Lookup table of actual values associated with "repeats" list

    static const int HEIGHT_ADD_TEMPS = 30;      // "Temperature" group on connect page: Add this much extra height to box after first row of temperatures
    static const int HEIGHT_ADD_CHECKBOX = 30;   // "Scan Channel" checkboxes on Eyescan / Bathtub pages: Add this much extra height per row


    // Bert Worker Class: Handles the work of communicating with the hardware
    BertWorker *bertWorker;

    // UI Update Timer: Fires every 250 mS and updates things like the
    // status message, LOS/LOL lights, core temperature, etc.
    QTimer *uiUpdateTimer = NULL;

    // Flags for UI elements which might change while the ED is running.
    // We set flags when the user changes things while the ED is running;
    // The flags are checked from inside the inside the timer update slot
    // to prevent conflicts.
    bool flagEDStart = false;
    bool flagEDStop = false;
    bool flagEDErrorInject = false;
    bool flagEDEQChange = false;
    bool flagEDDisplayChange = false;

    uint8_t edErrorInjectChannel = 1;
    uint8_t eqBoostChannel = 1;
    uint8_t eqBoostNewIndex = 0;

    int tickCountStatusTextReset;
    int tickCountTempUpdate;
    int tickCountTemperatureTextReset;
    int tickCountClockLockUpdate;

    QTime *edRunTime = NULL;

    int uiLockLevel = 0;
    int uiUnlockInMs = 0;  // mS until the UI Lock is automatically released

    QMap<int, BertChannel *> bertChannels;
    int maxChannel = 0;  // Highest channel number overall (so far...)

    // Error Detector Update State Containers
    // When the ED is running, these are used to track which Master and Slave channel is due
    // for the next counter update, and how many requests are outstanding (waiting for the back end).
    QList<BertUIEDChannel *> edEnabledMasterChannels;  // When the ED is runing, this holds a list of enabled channels on the MASTER board
    QListIterator<BertUIEDChannel *> nextEDMaster;
    int edMasterRequestsPending;
    QList<BertUIEDChannel *> edEnabledSlaveChannels;   // When the ED is runing, this holds a list of enabled channels on the SLAVE board
    QListIterator<BertUIEDChannel *> nextEDSlave;
    int edSlaveRequestsPending;


    int  eyeScanRepeatsTotal = 1;
    int  eyeScanRepeatsDone  = 0;
    int  eyeScanChannelCount = 0;

    int  eyeScansTotal = 0;     // Number of eye scans to do in this run (=[active channels] * [repeats])
    int  eyeScansDone = 0;      // Number of eye scans finished in this run

    bool connectInProgress = false;
    bool commsConnected;
    bool eventsEnabled = false;

    int  edUpdateCounter = 0;
    int  cdrUpdateCounter = 0;

    bool edPending = false;
    bool edRunning = false;
    bool eyeScanRunning = false;
    bool bathtubRunning = false;
    bool cdrModeRunning = false;

    int currentTabIndex = 0;

    double bitRate = 0;

    bool factoryOptionsEnabled = false;

    // References to 'tab' widgets (each contains the content of a tab).
    // Used when turning tabs on or off (e.g. enabling ED controls)
    QList<QWidget *> tabs;

    // UI Elements of Note, Assigned during UI creation:
    BertUIStatusMessage *statusMessage;
    BertUITabs          *tabWidget;
    QWidget             *tabConnect;
    QWidget             *tabClockSynth;
    QWidget             *tabPatternGenerator;
    QWidget             *tabErrorDetector;
    QWidget             *tabAnalysisEyeScan;
    QWidget             *tabAnalysisBathtub;
    QWidget             *tabCDRControls;
    QWidget             *tabAbout;

    BertUIButton        *buttonPortListRefresh;
    BertUIButton        *buttonConnect;
    BertUIButton        *buttonResync;
    BertUIList          *listSerialPorts;

    QGridLayout         *layoutConnect;
    BertUIGroup         *groupTemps;
    QGridLayout         *layoutTemps;
    BertUIGroup         *groupFactoryOptions;

    BertUIGroup         *groupClock;
    QVBoxLayout         *layoutClockSynth;
    BertUIList          *listLMXFreq;
    BertUILamp          *lampLMXLockMaster;
    BertUILamp          *lampLMXLockSlave;
    BertUIList          *listLMXTrigOutDivRatio;
    BertUIList          *listLMXTrigOutPower;

    BertUIGroup         *groupRefClock = nullptr;
    BertUIList          *listRefClockProfiles;
    BertUITextInfo      *valueRefClockFreqIn;
    BertUITextInfo      *valueRefClockFreqOut;

    BertUITextInfo      *valueBitRate_PG;
    QGridLayout         *layoutPGChannels;

    BertUITextInfo      *valueBitRate_ED;
    BertUIButton        *buttonEDStart;
    BertUIButton        *buttonEDStop;
    BertUIList          *listEDResultDisplay;
    BertUITextInfo      *valueMeasurementTime;
    BertUICheckBox      *checkEDEnableAll;
    BertUIPane          *paneEDCheckBoxes;
    QGridLayout         *layoutEDCheckboxes;
    QGridLayout         *layoutEDChannels;

    BertUIGroup         *groupEyeScanOpts;
    BertUITextInfo      *valueBitRate_EyeScan;
    BertUIButton        *buttonEyeScanStart;
    BertUIButton        *buttonEyeScanStop;
    BertUIList          *listEyeScanVStep;
    BertUIList          *listEyeScanHStep;
    BertUIList          *listEyeScanCountRes;
    BertUIList          *listEyeScanRepeats;
    BertUICheckBox      *checkESEnableAll;
    BertUIPane          *paneESCheckBoxes;
    QGridLayout         *layoutESCheckboxes;
    QGridLayout         *layoutESChannels;

    BertUIGroup         *groupBathtubOpts;
    BertUITextInfo      *valueBitRate_Bathtub;
    BertUIButton        *buttonBathtubStart;
    BertUIButton        *buttonBathtubStop;
    BertUIList          *listBathtubVOffset;
    BertUIList          *listBathtubCountRes;
    BertUIList          *listBathtubRepeats;
    BertUICheckBox      *checkBPEnableAll;
    BertUIPane          *paneBPCheckBoxes;
    QGridLayout         *layoutBPCheckboxes;
    QGridLayout         *layoutBPChannels;

    QVBoxLayout         *layoutCDR;
    BertUIPane          *paneCDRCheckBoxes;
    BertUICheckBox      *checkCDRModeEnable;

    QWidget             *hiddenTabs;

    // UI Widgets for setting EEPROM data: Factory Use Only!
    // May not exist at all in production UI.
    BertUIList          *listEEPROMModel = nullptr;
    BertUITextInput     *inputModel = nullptr;
    BertUITextInput     *inputSerial = nullptr;
    BertUITextInput     *inputProdDate = nullptr;
    BertUITextInput     *inputCalDate = nullptr;
    BertUITextInput     *inputWarrantyStart = nullptr;
    BertUITextInput     *inputWarrantyEnd = nullptr;
    BertUITextInput     *inputSynthConfigVersion = nullptr;
    BertUIButton        *buttonEEPROMDefaults = nullptr;
    BertUIButton        *buttonWriteEEPROM = nullptr;

    BertUIList          *listTCSLMXProfiles = nullptr;
    BertUIList          *listEEPROMLMXProfiles = nullptr;
    BertUIList          *listFirmwareVersion = nullptr;
//  BertUIList          *listEEPROMFirmware = nullptr;
    BertUIButton        *buttonWriteProfilesToEEPROM = nullptr;
    BertUIButton        *buttonVerifyLMXProfiles = nullptr;
//    BertUIButton        *buttonWriteFirmwareToEEPROM = nullptr;
//  BertUIButton        *buttonVerifyFirmware = nullptr;

};


#endif // MAINWINDOW_H
