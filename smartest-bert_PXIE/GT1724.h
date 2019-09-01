/*!
 \file   GT1724.h
 \brief  Functional commands to control a Gennum GT1724 PRBS Generator / Checker
         Note: This class also covers later versions of the Gennum IC which use
         the same macros and register definitions.

 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#ifndef GT1724_H
#define GT1724_H

#include <QObject>
#include <QStringList>
#include <QTime>

#include "globals.h"
#include "BertComponent.h"
#include "I2CComms.h"

class EyeMonitor;

class GT1724 : public BertComponent
{
    Q_OBJECT

public:
    GT1724(I2CComms *comms, const uint8_t i2cAddress, const uint8_t laneOffset);
    ~GT1724();

    friend class EyeMonitor;

    // Scan types for Eye Monitor:
    static const int GT1724_EYE_SCAN = 1;
    static const int GT1724_BATHTUB_SCAN = 2;

    // NOTE: Lanes used by GT1724:
    //
    // Each GT1724 IC has 4 lanes (0-3).
    //
    // Each chip also has 2 x PRBS checkers (also referred to as "EDs"),
    // each of which is associated with TWO lanes (lanes 0/1 and lanes 2/3).
    // We number the ED "lanes" 0 to 1:
    //  0 = PRBS Checker 0/1
    //  1 = PRBS Checker 2/3
    //
    // Most slots below take a "lane" parameter. In the case of instruments with
    // multiple GT1724 ICs, the lane may be > 3 (e.g. first IC has lanes 0-3,
    // second has lanes 4-7, etc). We don't care which IC we are, so distill the
    // supplied lane down to 0-3 range using '%4' operation. However, the supplied
    // lane value is returned for reference in any response signal, where appropriate.
    // For commands where the lane is only used in the response (e.g. configPG, which
    // applies to ALL lanes on the chip), the parameter is labelled "metaLane".

    static bool ping(I2CComms *comms, const uint8_t i2cAddress);
    void getOptions();
    int init();

#define GT1724_SIGNALS \
    void EDLosLol(int lane, bool los, bool lol);                    \
    void EDCount(int lane,                                          \
                 bool locked,                                       \
                 double bits, double bitsTotal,                     \
                 double errors, double errorsTotal);                \
    void EyeScanProgressUpdate(int lane, int type, int percent);    \
    void EyeScanError(int lane, int type, int code);                \
    void EyeScanFinished(int lane, int type, QVector<double> data, int xRes, int yRes);

#define GT1724_SLOTS \
    void CommsCheck(int metaLane); \
    void GetTemperature(int metaLane); \
    void ConfigSetDefaults(int metaLane, double bitRate); \
    void ConfigPG(int metaLane, int pattern, double bitRate); \
    void ConfigCDR(int metaLane, int inputLane, int freqDivider); \
    void SetLaneOn(int lane, bool laneOn, bool powerDownOnMute); \
    void SetOutputSwing(int lane, int swingIndex); \
    void SetLaneInverted(int lane, bool inverted); \
    void SetDeEmphasis(int lane, int level, int prePost); \
    void SetCrossPoint(int lane, int crossPointIndex); \
    void SetEQBoost(int lane, int eqBoostIndex); \
    void SetForceCDRBypass(int lane, int forceCDRBypass, double bitRate); \
    void SetEDOptions(int metaLane, int pattern01, bool invert01, bool enable01, int pattern23, bool invert23, bool enable23); \
    void GetLosLol(int metaLane); \
    void GetEDCount(int lane, double bitRate); \
    void EDErrorInject(int lane); \
    void EyeScanStart(int lane, int type, int hStep, int vStep, int vOffset, int countRes); \
    void EyeScanRepeat(int lane); \
    void EyeScanCancel(int lane);

#define GT1724_CONNECT_SIGNALS(CLIENT, GT1724) \
    connect(GT1724, SIGNAL(EDLosLol(int, bool, bool)),         CLIENT, SLOT(EDLosLol(int, bool, bool)));                            \
    connect(GT1724, SIGNAL(EDCount(int, bool, double, double, double, double)),                                                     \
                                                               CLIENT, SLOT(EDCount(int, bool, double, double, double, double)));   \
    connect(GT1724, SIGNAL(EyeScanProgressUpdate(int, int, int)),                                                                   \
                                                               CLIENT, SLOT(EyeScanProgressUpdate(int, int, int)));                 \
    connect(GT1724, SIGNAL(EyeScanError(int, int, int)),       CLIENT, SLOT(EyeScanError(int, int, int)));                          \
    connect(GT1724, SIGNAL(EyeScanFinished(int, int, QVector<double>, int, int)),                                                   \
                                                               CLIENT, SLOT(EyeScanFinished(int, int, QVector<double>, int, int))); \
                                                                                                                                    \
    connect(CLIENT, SIGNAL(CommsCheck(int)),                   GT1724, SLOT(CommsCheck(int)));                                      \
    connect(CLIENT, SIGNAL(GetTemperature(int)),               GT1724, SLOT(GetTemperature(int)));                                  \
    connect(CLIENT, SIGNAL(ConfigSetDefaults(int, double)),    GT1724, SLOT(ConfigSetDefaults(int, double)));                       \
    connect(CLIENT, SIGNAL(ConfigPG(int, int, double)),        GT1724, SLOT(ConfigPG(int, int, double)));                           \
    connect(CLIENT, SIGNAL(ConfigCDR(int, int, int)),          GT1724, SLOT(ConfigCDR(int, int, int)));                             \
    connect(CLIENT, SIGNAL(SetLaneOn(int, bool, bool)),        GT1724, SLOT(SetLaneOn(int, bool, bool)));                           \
    connect(CLIENT, SIGNAL(SetOutputSwing(int, int)),          GT1724, SLOT(SetOutputSwing(int, int)));                             \
    connect(CLIENT, SIGNAL(SetLaneInverted(int, bool)),        GT1724, SLOT(SetLaneInverted(int, bool)));                           \
    connect(CLIENT, SIGNAL(SetDeEmphasis(int, int, int)),      GT1724, SLOT(SetDeEmphasis(int, int, int)));                         \
    connect(CLIENT, SIGNAL(SetCrossPoint(int, int)),           GT1724, SLOT(SetCrossPoint(int, int)));                              \
    connect(CLIENT, SIGNAL(SetEQBoost(int, int)),              GT1724, SLOT(SetEQBoost(int, int)));                                 \
    connect(CLIENT, SIGNAL(SetForceCDRBypass(int, int, double)),                                                                    \
                                                               GT1724, SLOT(SetForceCDRBypass(int, int, double)));                  \
    connect(CLIENT, SIGNAL(SetEDOptions(int, int, bool, bool, int, bool, bool)),                                                    \
                                                               GT1724, SLOT(SetEDOptions(int, int, bool, bool, int, bool, bool)));  \
    connect(CLIENT, SIGNAL(GetLosLol(int)),                    GT1724, SLOT(GetLosLol(int)));                                       \
    connect(CLIENT, SIGNAL(GetEDCount(int, double)),           GT1724, SLOT(GetEDCount(int, double)));                              \
    connect(CLIENT, SIGNAL(EDErrorInject(int)),                GT1724, SLOT(EDErrorInject(int)));                                   \
    connect(CLIENT, SIGNAL(EyeScanStart(int, int, int, int, int, int)),                                                             \
                                                               GT1724, SLOT(EyeScanStart(int, int, int, int, int, int)));           \
    connect(CLIENT, SIGNAL(EyeScanRepeat(int)),                GT1724, SLOT(EyeScanRepeat(int)));                                   \
    connect(CLIENT, SIGNAL(EyeScanCancel(int)),                GT1724, SLOT(EyeScanCancel(int)));                                   \
    BERT_COMPONENT_CONNECT_SIGNALS(CLIENT, GT1724)


signals:
    // Signals which are specific to a GT1724:
    GT1724_SIGNALS

    // Public Slots for BERT Commands:
public slots:
    GT1724_SLOTS


private:

    // GT1724 Instrument Functions:

    // configXXX methods: These set up the GT1724 for a certain mode of operation.
    // They may run several macros, including "Mission Low Power", "Configure Device Mode", etc.

    int  configSetDefaults (double bitRate);                 // Set up all default settings (cold boot / resync only)

    int  configPG          (int pattern, double bitRate);    // Set up GT1724 for Pattern Generator mode
    int  configCDR         (int inputLane, int freqDivider); // Set up GT1724 for Clock Data Recovery Mode

    // getXXX / setXXX methods: These methods get or set the state of a specific GT1724
    // option, and can generally be used while the PG or ED is running. Note however
    // some may have undesirable effects on certain modes of operation; e.g.
    // powering down the De-Emphasis path with setPDDeEmphasis will also stop the
    // PRBS checker from working.

    /* UNUSED: These methods are no longer used and the code hasn't been
     * updated for a while. Probably need some work to resurrect!
    int  setAutoBypassOnLOL (int lane, bool autoBypassOn);
    bool getAutoBypassOnLOL (int lane);
    int setPDLaneOutput (const uint8_t lane, const uint8_t powerDown);
    int getPDLaneOutput (const uint8_t lane, uint8_t *powerDown);
    */

    int  setLaneOn (int lane, bool laneOn, bool powerDownOnMute);
    bool getLaneOn (int lane);

    int  setOutputSwing              (int lane, int swing);
    int  getOutputSwings             ();
    int  configOutputDriverMainSwing (int swings[4]);
    int  queryOutputDriverMainSwing  (int swings[4]);

    int  setPRBSOptions (int  pattern, int  vcoFreq, int  source, int  enable);
    int  getPRBSPattern (int *pattern);
    int  getPRBSOptions (int *pattern, int *vcoFreq, int *source, int *enable);

    int  setLaneInverted (int lane, bool inverted);
    bool getLaneInverted (int lane, bool emitSignal = true);

    int  setPDDeEmphasis (int lane, bool powerDown);
    bool getPDDeEmphasis (int lane);

    int  setDeEmphasis (int lane, int level, int prePost);
    int  getDeEmphasis (int lane);

    int  setCrossPoint (int lane, int crossPointIndex);
    int  getCrossPoint (int lane);

    int  setEQBoost (int lane, int eqBoostIndex);
    int  getEQBoost (int lane);

    int  setPDLaneMainPath (int lane, bool powerDown);

    int  setEDOptions (int pattern01, int invert01, int enable01, int pattern23, int invert23, int enable23);
    int  getEDOptions ();
    int  getEDCount   (int edLane, double *bits, double *errors);

    int  setLosEnable(uint8_t state);
    int  getLosLol(uint8_t los[4], uint8_t lol[4]);

    int  setForceCDRBypass  (int lane, int forceCDRBypass, double bitRate);
    int  getForceCDRBypass  (int lane);

    int  debugPowerStatus(int lane);

    // Emit signals on behalf of EyeScan module:
    void emitEyeScanProgressUpdate(int lane, int type, int percent);
    void emitEyeScanError(int lane, int type, int code);
    void emitEyeScanFinished(int lane, int type, QVector<double> data, int xRes, int yRes);
    // Eye scanner - check for cancel signal:
    void eyeScanCheckForCancel();

    // *** Lists of settings with lookups: ***
    static const QList<int> PG_OUTPUT_SWING_LOOKUP;
    static const QStringList PG_OUTPUT_SWING_LIST;

    static const QStringList PG_PATTERN_LIST;

    static const QStringList PG_EQ_DEEMPH_LIST;

    static const QStringList PG_EQ_CURSOR_LIST;

    static const QList<int> PG_CROSS_POINT_LOOKUP;
    static const QStringList PG_CROSS_POINT_LIST;

    static const QStringList ED_PATTERN_LIST;
    static const int ED_PATTERN_DEFAULT;

    static const QList<int> ED_EQ_BOOST_LOOKUP;
    static const QStringList ED_EQ_BOOST_LIST;

    static const QList<int> EYESCAN_VHSTEP_LOOKUP;
    static const QStringList EYESCAN_VHSTEP_LIST;
    static const int EYESCAN_VHSTEP_DEFAULT;

    static const QList<int>  EYESCAN_VOFF_LOOKUP;
    static const QStringList EYESCAN_VOFF_LIST;
    static const int EYESCAN_VOFF_DEFAULT;

    static const QStringList EYESCAN_COUNTRES_LIST;
    static const int EYESCAN_COUNTRES_DEFAULT;

    static const QStringList CDR_BYPASS_OPTIONS_LIST;
    static const int CDR_BYPASS_OPTIONS_DEFAULT;

    static const QStringList CDR_FREQDIV_OPTIONS_LIST;
    static const int CDR_FREQDIV_OPTIONS_DEFAULT;

    // *** GT1724 Register Addresses: ***
    static const uint8_t GTREG_CDR_REG_0      = 0x00;
    static const uint8_t GTREG_OFF_COR_REG_2  = 0x08;
    static const uint8_t GTREG_EQ_REG_0       = 0x09;
    static const uint8_t GTREG_EQ_REG_2       = 0x0B;
    static const uint8_t GTREG_DRV_REG_0      = 0x32;
    static const uint8_t GTREG_DRV_REG_2      = 0x34;
    static const uint8_t GTREG_DRV_REG_5      = 0x37;
    static const uint8_t GTREG_PD_REG_1       = 0x3D;
    static const uint8_t GTREG_PD_REG_6       = 0x42;

    static const uint16_t GTREG_LOS_REG_0           = 0x0404;  // = 1028d (16 bit address)
    static const uint16_t GTREG_LOSL_OUTPUT         = 0x0411;  // = 1041d (16 bit address)
    static const uint16_t GTREG_LOSL_OUTPUT_LATCHED = 0x0412;  // = 1042d (16 bit address)

    static const double EXT_CLOCK_FREQ_MIN;
    static const double EXT_CLOCK_FREQ_MAX;

    const globals::MacroFileInfo *macroVersion = &(globals::MACRO_FILES[0]);
       // Pointer to information about the macro version for the chip (initially unknown)

    I2CComms *comms;              // Comms port for I2C connecton to chip (set up elsewhere!)

    EyeMonitor *eyeMonitor01;     // Eye Monitor modules used for carrying out eye scans on this device's ED lanes
    EyeMonitor *eyeMonitor23;     //  (one instance for each ED lane!)

    const uint8_t   i2cAddress;   // I2C Address of this chip (7 bit, i.e. not including R/W bit), supplied by creator

    const uint8_t   laneOffset;   // Lane number of 1st lane this chip will implement (e.g. 0 for 1st chip, 4 for 2nd, etc).
                                  // Nb: We don't know or care what chip we are; we just return this value
                                  // as a reference for others when needed.

    const uint8_t   coreNumber;   // Derived from laneOffset and used to ID the GT1724 chip
                                  // (Purely for display purposes for users).
                                  // lane 0-3 = Core 1, Lane 4-7 = Core 2, etc.

    int forceCDRBypass0 = CDR_BYPASS_OPTIONS_DEFAULT;  // CDR Bypass setting for Lane 0
    int forceCDRBypass1 = CDR_BYPASS_OPTIONS_DEFAULT;  // CDR Bypass setting for Lane 1 (only used for 4 channel PG mode)
    int forceCDRBypass2 = CDR_BYPASS_OPTIONS_DEFAULT;  // CDR Bypass setting for Lane 2
    int forceCDRBypass3 = CDR_BYPASS_OPTIONS_DEFAULT;  // CDR Bypass setting for Lane 3 (only used for 4 channel PG mode)


    typedef struct edParameters_t
    {
        bool edRunning = false;      //
        double bitsTotal = 0.0;      //
        double errorsTotal = 0.0;    // Used for ED measurements
        QTime *edRunTime = NULL;     //
        int lastMeasureTimeMs = 0;   //
        bool los = false;            //
        bool lol = false;            //
    } edParameters_t;

    edParameters_t ed01;
    edParameters_t ed23;

    // *** Private methods to drive the BERT, load macros, etc: ******
    int     macroCheck(int metaLane);
    int     downloadHexFile();
    uint8_t hexCharToInt(uint8_t byte);
    bool    hexCharsToInt(uint8_t charHi, uint8_t charLo, uint8_t *result);
    double  edBytesToDouble(const uint8_t bytes[2]);
    int     commsCheckOneRegister(int lane, uint8_t value, int &countGood, int &countError);
    int     getCurrentSettings(int *pattern);
    bool    checkForceCDRBypass(int forceCDRBypass, double bitRate);

    // Methods to access GT1724 via I2C, and run macros, set registers, etc.
    static int runMacroStatic(I2CComms      *comms,
                              const uint8_t  i2cAddress,
                              const uint8_t  code,
                              const uint8_t *dataIn,
                              const uint8_t  dataInSize,
                              uint8_t       *dataOut,
                              const uint8_t  dataOutSize,
                              const uint16_t timeoutMs);

    int runLongMacro(const uint8_t  code,
                     const uint8_t *dataIn,
                     const uint8_t  dataInSize,
                     uint8_t       *dataOut,
                     const uint8_t  dataOutSize,
                     const uint16_t timeoutMs);

    int runMacro(const uint8_t  code,
                 const uint8_t *dataIn,
                 const uint8_t  dataInSize,
                 uint8_t       *dataOut,
                 const uint8_t  dataOutSize);

    int runSimpleMacro(const uint8_t code, const uint8_t dataIn);

    int runVerySimpleMacro(const uint8_t code);

    int setRegister(int lane, const uint8_t address, const uint8_t  data);
    int getRegister(int lane, const uint8_t address,       uint8_t *data);

    int setRegister16(const uint16_t address, const uint8_t data);
    int getRegister16(const uint16_t address, uint8_t *data);

    int rawWrite24(const uint8_t  addressHi,
                   const uint8_t  addressMid,
                   const uint8_t  addressLow,
                   const uint8_t *data,
                   const uint8_t  nBytes);

    int rawRead24(const uint8_t  addressHi,
                  const uint8_t  addressMid,
                  const uint8_t  addressLow,
                  uint8_t       *data,
                  const size_t   nBytes);

};

#endif // GT1724_H
