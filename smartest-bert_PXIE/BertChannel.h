#ifndef BERTCHANNEL_H
#define BERTCHANNEL_H

#include "widgets/BertUIPGChannel.h"
#include "widgets/BertUIEDChannel.h"
#include "widgets/BertUIEyescanChannel.h"
#include "widgets/BertUIBathtubChannel.h"
#include "widgets/BertUICDRChannel.h"

#include "BertModel.h"

/*!
 \brief Bert Channel class
        Data and methods to manage a "channel" on the BERT instrument

        "Channels" are a software concept which are closley tied to the
        user interface representation of a BERT instument. Users see
        channels numbered from 1 - n on each UI page. These channels map
        to "lanes" on the actual hardware device, as defined by the
        hardware support back end (GT124 class, etc).

        For example, a dual-chip instrument will provide 4 "channels" on
        each page, numbered 1 to 4:
         - 4 Pattern Generator channels, mapped as follows:
             Channels 1 and 2 = Lanes 0 and 2 -> Lanes 0 and 2 on FIRST GT1724
             Channels 3 and 4 = Lanes 4 and 6 -> Lanes 0 and 2 on SECOND GT1724
         - 4 Error Detector channels, mapped as follows:
             Channels 1 and 2 = Lanes 1 and 3 -> Lanes 1 and 3 on FIRST GT1724
             Channels 3 and 4 = Lanes 5 and 7 -> Lanes 1 and 3 on SECOND GT1724
         - 4 Eye Scan channels and 4 Bathtub Plot channels, mapped as follows:
             Channels 1 and 2 = Error Detectors 0/1 and 2/3 on FIRST GT1724
             Channels 3 and 4 = Error Detectors 0/1 and 2/3 on SECOND GT1724

       Each Gennum core will also generate:
           1 x temperature display widget
           1 x CDR mode controls

       UI Channels may be created as needed during the instrument start up
       process, e.g. if a slave board is detected. Additional GT1724 chips
       continue the same numbering pattern.

 */
class BertChannel
{
public:
    BertChannel(int channel, QWidget *parent, bool showCDRBypassOptions = true);
    ~BertChannel();

    // Status Flags:
    bool eyeScanStartedFlag = false;
    bool edErrorflasherOn = false;
    bool edOptionsChanged = false;

    // Lane to channel conversions:
    static int laneToChannel(int lane)    { if (!BertModel::UseFourChanPGMode()) return (lane / 2) + 1; else return lane + 1; }  // Horrible and hacky! Improve...
    static int laneToCore(int lane)       { return lane / 4;       }
    static int laneToBoard(int lane)      { return lane / 8;       }

    int getRow() const { return (channel-1)/2; }  // Assuming this channel will be displayed in a 2 col x n row grid, these methods return
    int getCol() const { return (channel+1)%2; }  // the row and column where the widget will be placed, derived from channel number.

    int getRow1Col() const { return channel-1; }  // Assuming this channel will be displayed in a 1 col x n row grid, this returns the row (derived from channel number)

    int getChannel()  const { return channel;   }
    int getCore()     const { return core;      }
    int getBoard()    const { return board;     }
    int getPGLane()   const { return pgLane;    }
    int getEDLane()   const { return edLane;    }
    int getMetaLane() const { return core * 4;  }


    bool getEyeScanChannelEnabled() const { return checkEyeScanChannel->isChecked(); }
    bool getBathtubChannelEnabled() const { return checkBathtubChannel->isChecked(); }
    bool hasTemperatureWidget() const { return groupTemp != NULL; }
    bool hasCDRModeChannel() const { return cdrModeChannel != NULL; }

    BertUIPane           *getTemperature()     const { return groupTemp; }
    BertUIPGChannel      *getPG()              const { return pg; }
    BertUIEDChannel      *getED()              const { return ed; }
    BertUICheckBox       *getEDCheckbox()      const { return getED()->getEDCheckbox(); }
    BertUIEyescanChannel *getEyescan()         const { return eyescan; }
    BertUICheckBox       *getEyescanCheckbox() const { return checkEyeScanChannel; }
    BertUIBathtubChannel *getBathtub()         const { return bathtub; }
    BertUICheckBox       *getBathtubCheckbox() const { return checkBathtubChannel; }
    BertUICDRChannel     *getCDRMode()         const { return cdrModeChannel; }

    void resetCoreTemp();

private:
    const int channel;    // Channel number as displayed to the user; numbered from 1
    const int core;       // GT1724 IC where this channel resides; numbered from 0
    const int board;      // Board where this channel resides, numbered from 0: typically, 0 = Master and contains first 4 channels; 1 = Slave and contains next 4 channels.
    const int pgLane;     // Lane number for this channel for Pattern Generator controls (OUTPUT lane)
    const int pg4Lane;    // Lane number for this channel for Pattern Generator controls (OUTPUT lane) when running as 4 channel PG only (no ED lanes)
    const int edLane;     // Lane number for this channel for Error Detector controls (INPUT lane)
    const int esLane;     // Lane number for this channel for Eye scanner (incl. Bathtub plot)
    const int ctLane;     // Lane number for this channel for core temperature readout (1 per GT1724 IC)
    const int cdrLane;    // Lane number for this channel for clock data recovery mode (1 per GT1724 IC)

    // Channel-specific Widgets:
    BertUIPane           *groupTemp;
    BertUILabel          *labelTemp;
    BertUILabel          *valueTemp;
    BertUIPGChannel      *pg;
    BertUIEDChannel      *ed;
    BertUIEyescanChannel *eyescan;
    BertUICheckBox       *checkEyeScanChannel;
    BertUIBathtubChannel *bathtub;
    BertUICheckBox       *checkBathtubChannel;
    BertUICDRChannel     *cdrModeChannel;

};

#endif // BERTCHANNEL_H
