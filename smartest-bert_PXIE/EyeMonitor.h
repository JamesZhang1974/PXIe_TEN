/*!
 \file   EyeMonitor.h
 \brief  Eye Monitor Functions - Class Header
 \author J Cole-Baker (For Smartest)
 \date   Aug 2018 (Adapted from old Eye Monitor)
*/

#ifndef EYEMONITOR_H
#define EYEMONITOR_H

#include "GT1724.h"

/*!
 \brief Eye Monitor Functions
 This class includes methods for eye scanning and
 bathtub plots.
 This is a friend class for the GT1724 class; the GT1724 class
 will manage comms and signals.
*/
class EyeMonitor
{

public:

    EyeMonitor(GT1724 *parent, int laneOffset, int lane);
    ~EyeMonitor();

    int startScan(int type,
                  int hStepIndex,
                  int vStepIndex,      // Nb: Eye scan only; use 0 for Bathtub scan
                  int vOffsetIndex,    // Nb: Bathtub scan only; use 0 for Eye scan
                  int countResIndex);

    int repeatScan();  // Repeats the previous scan, and adds the new data to the existing data

    void cancelScan();


private:

    GT1724 *parent;        // Used to call GT1724 member functions to send signals
    const int laneOffset;  // Lane offset from parent GT1724 class (0, 4, etc); Set in constructor
    const int scanLane;    // Lane to which this eye scanner will be attached:
                           // One of the ED input lanes (1 or 3); Set in constructor

    int scanType;  // Full eye scan or just bathtub scan? Constants defined in GT1724.h

    uint8_t  scanHStep     = 1;  // Step between sample points
    uint8_t  scanVStep     = 1;  //
    uint8_t  scanHRes      = 1;  // Number of sample points
    uint8_t  scanVRes      = 1;  //
    uint8_t  scanVOffset   = 0;  // Only used for bathtub scan
    int      nShift        = 0;  // Number of samples to rotate plot (horizontal)

    uint8_t  scanCountResIndex = 0;
    uint8_t  scanCountResBits  = 1;

    int scanRepeatCount = 0;

    bool stopFlag = false;

    QVector<double> eyeDataBuffer;
    QVector<double> eyeDataBufferNorm;

    int eyeScanRun(bool resetFlag);

    uint8_t peakFind( QVector<double> &data,
                      const size_t sizeX,
                      const size_t sizeY );

    int dataShift( QVector<double> &data,
                   QVector<double> &dataShifted,
                   const size_t sizeX,
                   const size_t sizeY,
                   const int nShift );

    int queryEyeScanMem( uint8_t *addressMSB,
                         uint8_t *addressLSB,
                         uint8_t *sizeMSB,
                         uint8_t *sizeLSB );

    int controlEyeSweep( const uint8_t phaseStart,
                         const uint8_t phaseStop,
                         const uint8_t phaseStep,
                         const uint8_t offsetStart,
                         const uint8_t offsetStop,
                         const uint8_t offsetStep,
                         const uint8_t resolution,
                         uint8_t *sizeMSB,
                         uint8_t *sizeLSB );

    void bufferReset(QVector<double> &buffer, int newSize);

};


#endif // EYEMONITOR_H
