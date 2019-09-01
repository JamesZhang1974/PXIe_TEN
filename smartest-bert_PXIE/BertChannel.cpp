#include "mainwindow.h"  // Headers For slots

#include "BertChannel.h"

#define CONNECT(SRC, SIG) \
    QObject::connect(SRC, SIGNAL(SIG), parent, SLOT(SIG))


BertChannel::BertChannel(int channel, QWidget *parent, bool showCDRBypassOptions)
 : channel(channel),          // Displayed Channel: 1, 2, 3, 4, 5, ...
   core((channel-1)/2),       // GT1724 IC number:  0, 0, 1, 1, 2, ...
   board((channel-1)/4),      // Board number:      0, 0, 0, 0, 1, ...
   pgLane((channel-1)*2),     // Pattern Gen Lane:  0, 2, 4, 6, 8, ...
   pg4Lane(channel-1),        // Pattern Gen Lane for PG4 mode:  0, 1, 2, 3, ...
   edLane((channel*2)-1),     // Error Detect Lane: 1, 3, 5, 7, 9, ...
   esLane((channel*2)-1),     // Eye scanner lane:  1, 3, 5, 7, 9, ...
   ctLane((channel-1)*2),     // Temperature lane:  0, x, 4, x, 8, ... (One core temperature per GT1724, i.e. even numbered lanes)
   cdrLane((channel-1)*2)     // Clock data recovery mode (1 per GT1724 IC), same as ctLane
{
    eyeScanStartedFlag = false;
    edErrorflasherOn = false;
    edOptionsChanged = false;

    // Add a core temperature reading for every 2nd channel (1 per GT1724 chip):
    if ( (!BertModel::UseFourChanPGMode() && (ctLane % 4) == 0)
      || ( BertModel::UseFourChanPGMode() && channel == 1) )
    {
        QString labelText = QString("%1 %2:").arg((ctLane < 5) ? "Master" : "Slave").arg((core % 2) + 1);
        groupTemp = new BertUIPane("", parent, ctLane, 0, 0, 120, 30);
        labelTemp = new BertUILabel ("",                                        groupTemp, labelText, -1,      0,  0, 60 );
        valueTemp = new BertUILabel (QString("CoreTemperature_%1").arg(ctLane), groupTemp, "-- °C",   ctLane,  65, 0, 50 );
    }
    else
    {
        groupTemp = NULL;
        labelTemp = NULL;
        valueTemp = NULL;
    }

    int usePGLane = pgLane;
    if (BertModel::UseFourChanPGMode()) usePGLane = pg4Lane;
    pg = new BertUIPGChannel(QString("PGChannel_%1").arg(channel), parent, channel, usePGLane, 0, 0, 501, 100, showCDRBypassOptions); // 531, 100
    CONNECT(pg, boolPGLaneOn_clicked(int, bool));
    CONNECT(pg, boolPGLaneInverted_clicked(int, bool));
    CONNECT(pg, listPGAmplitude_currentIndexChanged(int, int));
    CONNECT(pg, listPGPattern_currentIndexChanged(int, int));
    CONNECT(pg, listPGDeemphLevel_currentIndexChanged(int, int));
    CONNECT(pg, listPGDeemphCursor_currentIndexChanged(int, int));
    CONNECT(pg, listPGCrossPoint_currentIndexChanged(int, int));
    if (showCDRBypassOptions) CONNECT(pg, listPGCDRBypass_currentIndexChanged(int, int));

    ed = new BertUIEDChannel(QString("EDChannel_%1").arg(channel), parent, channel, edLane, 0, 0, 501, 100); // 531, 100
    CONNECT(ed, boolEDEnable_clicked(int, bool));
    CONNECT(ed, listEDPattern_currentIndexChanged(int, int));
    CONNECT(ed, boolEDPatternInvert_clicked(int, bool));
    CONNECT(ed, listEDEQBoost_currentIndexChanged(int, int));
    CONNECT(ed, buttonEDInjectError_clicked(int));

    eyescan = new BertUIEyescanChannel(QString("ESChannel_%1").arg(channel), parent, channel, esLane, 0, 0, 100, 100); // 461, 251

    checkEyeScanChannel = new BertUICheckBox(QString("checkEyeScanChannel_%1").arg(channel), parent, QString("Scan Channel %1").arg(channel), esLane, 0, 0, 101);

    bathtub = new BertUIBathtubChannel(QString("BPChannel_%1").arg(channel), parent, channel, esLane, 0, 0, 100, 100);  // 461, 251

    checkBathtubChannel = new BertUICheckBox(QString("checkBathtubChannel_%1").arg(channel), parent, QString("Scan Channel %1").arg(channel), esLane, 0, 0, 101);

    if (!BertModel::UseFourChanPGMode() && (cdrLane % 4) == 0)
    {
        cdrModeChannel = new BertUICDRChannel(QString("checkBathtubChannel_%1").arg(core), parent, core, 5, 5, 100, 150);
        CONNECT(cdrModeChannel, listCDRChannelSelect_currentIndexChanged(int, int));
        CONNECT(cdrModeChannel, listCDRDivideRatio_currentIndexChanged(int, int));
        CONNECT(cdrModeChannel, checkCDRModeEnable_clicked(int, bool));
    }
    else
    {
        cdrModeChannel = NULL;
    }
}

BertChannel::~BertChannel()
{
    if (groupTemp) delete groupTemp;
    delete pg;
    delete ed;
    delete eyescan;
    delete checkEyeScanChannel;
    delete bathtub;
    delete checkBathtubChannel;
    if (cdrModeChannel) delete cdrModeChannel;
}


/*!
 \brief Reset (i.e. clear) the Core Temperature for this channel (if there is one!) to "-- °C"
*/
void BertChannel::resetCoreTemp()
{
    if (valueTemp) valueTemp->setText("-- °C");
}


