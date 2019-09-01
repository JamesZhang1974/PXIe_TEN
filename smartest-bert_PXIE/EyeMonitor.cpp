/*!
 \file   EyeMonitor.cpp
 \brief  Eye Monitor Functions - Implementation
 \author J Cole-Baker (For Smartest)
 \date   Aug 2018 (Adapted from old Eye Monitor)
*/

#include <cstdlib>
#include <QMutex>
#include <QDebug>

#include <QCoreApplication>

#include <math.h>
#include <stdint.h>
#include <windows.h>

#include "globals.h"

#include "EyeMonitor.h"


EyeMonitor::EyeMonitor(GT1724 *parent, int laneOffset, int lane)
 : laneOffset(laneOffset), scanLane(lane)
{
    this->parent = parent;
}


EyeMonitor::~EyeMonitor()
{}


/*!
 \brief Set up and start an eye scan

 Depending on the selected resolution options, the scan
 may require multiple macro calls and take some time to
 complete.

 This method will call "processEvents" periodically during its
 operations to check for the "EyeScanCancel" signal (see
 GT1724.cpp). It is essential that no other signals (particularly
 those which require the serial port) be received by the worker,
 as these could interrupt the eye scan process.

 The caller should lock all UI functions, leaving only a
 "Cancel Scan" button. This is connected to the "EyeScanCancel" signal
 which calls cancelScan().

 \param type          Type of scan: Eye or bathtub
 \param hStepIndex    Horizontal (phase) step index (from EYESCAN_VHSTEP_LOOKUP)
 \param vStepIndex    EYE SCAN ONLY: Vertical (voltage offset) step index (from EYESCAN_VHSTEP_LOOKUP) (use 0 for bathtub scan)
 \param vOffsetIndex  BATHTUB SCAN ONLY: Index for offset of row to scan (from EYESCAN_VOFF_LOOKUP) (use 0 for eye scan)
 \param countResIndex Resolution of error count at each data point (i.e. 'z' resolution):
                         0 = 1 bit
                         1 = 2 bit
                         2 = 4 bit
                         3 = 8 bit

 \return globals::OK
 \return [error code]
*/
int EyeMonitor::startScan(int type,
                          int hStepIndex,
                          int vStepIndex,
                          int vOffsetIndex,
                          int countResIndex)
{
    Q_ASSERT( (hStepIndex >= 0)    && (hStepIndex < GT1724::EYESCAN_VHSTEP_LOOKUP.size()) &&
              (vStepIndex >= 0)    && (vStepIndex < GT1724::EYESCAN_VHSTEP_LOOKUP.size()) &&
              (countResIndex >= 0) && (countResIndex <= 3) );

    stopFlag     = false;
    scanType     = type;
    scanHStep    = static_cast<uint8_t>(GT1724::EYESCAN_VHSTEP_LOOKUP[hStepIndex]);  // Horizontal (phase) step  (1 - 128)
    scanVStep    = static_cast<uint8_t>(GT1724::EYESCAN_VHSTEP_LOOKUP[vStepIndex]);  // Vertical (voltage offset) step (1 - 127) (EYE SCAN ONLY; use 1 for bathtub scan)
    scanVOffset  = static_cast<uint8_t>(GT1724::EYESCAN_VOFF_LOOKUP[vOffsetIndex]);  // Offset of row to scan (1 - 127) (BATHTUB SCAN ONLY; use 1 for eye scan)

    scanCountResIndex = static_cast<uint8_t>(countResIndex);      // Count res as index (0-3),  as used by 'Control Eye Sweep' macro
    scanCountResBits  = static_cast<uint8_t>(1 << countResIndex); // Converted to number of bits (1, 2, 4 or 8), for size calcs.

    scanRepeatCount = 1;

    qDebug() << "Eye Scan Configuration:";
    qDebug() << " Type:       " << ((scanType == GT1724::GT1724_EYE_SCAN) ? "Eye Scan" : "Bathtub Scan");
    qDebug() << " Lane:       " << scanLane;
    qDebug() << " H Step:     " << scanHStep;
    qDebug() << " V Step:     " << scanVStep;
    qDebug() << " V Offset:   " << scanVOffset;
    qDebug() << " Resolution: " << scanCountResBits << " (index " << scanCountResIndex << ")";

    return eyeScanRun(true);
}



/*!
 \brief Repeat Scan
        The previous scan is repeated with the same parameters.
        New data are added to the existing data, then normalised.
        NOTE: If a scan has not previously been run, an error is
        returned.
        See notes above re: slots / signals
 \return globals::OK
 \return [error code]
*/
int EyeMonitor::repeatScan()
{
    Q_ASSERT(scanRepeatCount > 0);
    if (scanRepeatCount == 0) return globals::GEN_ERROR;
    scanRepeatCount++;
    return eyeScanRun(false);
}




/*!
 \brief Cancel the eye scan
*/
void EyeMonitor::cancelScan()
  {  stopFlag = true;  }










////////////// PRIVATE: ////////////////////////////////////////////////////////////

/*!
 \brief Run the Eye Scan
*/
int EyeMonitor::eyeScanRun(bool resetFlag)
{
    int scanResult = globals::OK;
    uint8_t *rawDataBuffer = NULL;
    QVector<double> eyeDataBufferTmp;

    int numSamples = 1;
    int eyeDataBufferIndexMax = 1;
    int eyeDataBufferIndex = 0;

    ///////// Determine the output memory attributes: ///////////////////////
    uint16_t imageStartAddress;
    uint16_t imageMaximumSize;
    uint8_t imageAddressMSB, imageAddressLSB;
    uint8_t sizeMSB, sizeLSB;
    int bytesPerLine;
    imageAddressMSB = 0;
    imageAddressLSB = 0;
    sizeMSB    = 0;
    sizeLSB    = 0;
    scanResult = queryEyeScanMem( &imageAddressMSB, &imageAddressLSB, &sizeMSB, &sizeLSB );
    if (scanResult != globals::OK)
    {
        qDebug() << "Error reading output memory attributes: " << scanResult;
        goto finished;
    }
    imageStartAddress = static_cast<uint16_t>(static_cast<uint16_t>(imageAddressMSB) << 8) + static_cast<uint16_t>(imageAddressLSB);
    imageMaximumSize = static_cast<uint16_t>(static_cast<uint16_t>(sizeMSB) << 8) + static_cast<uint16_t>(sizeLSB);
    qDebug() << "Eye Scan - Image Start Address: " << imageStartAddress
             << "; Size: " << imageMaximumSize;

    /**** Eye Scan: *****************************************************/
    qDebug() << "**Starting Eye Scan**";

    uint8_t numPhaseSteps;
    uint8_t numOffsetStepsMax;
    uint8_t numOffsetSteps;

    uint8_t offsetStart, offsetStop;
    uint8_t thisOffsetStart, thisOffsetStop;

    uint8_t outputSizeMSB, outputSizeLSB;
    uint16_t outputSize;

    // Calculate the number of steps in one scan line:
    numPhaseSteps = 128 / scanHStep;

    /* OLD - From GT1724 Manual - DEPRECATED
     * Note: The calculation below is for arbitrary values of
     * phaseStart / phaseStop / scanHStep.
     * However, we simplify it (above) because we always scan
     * an entire row (phaseStart=0; phaseStop=127) and scanHStep
     * is 1, 2, 4 or 8.
    numPhaseSteps = (uint8_t)
            ceil( (double)(phaseStop - phaseStart + 1) / (double)scanHStep );
    */
    scanHRes = numPhaseSteps;

    // Calculate the number of lines that will fit in memory:
    bytesPerLine = (numPhaseSteps * scanCountResBits) / 8;  // Number of bytes in one 'horizontal' scan line (i.e. one offset step)
    numOffsetStepsMax = static_cast<uint8_t>(imageMaximumSize / bytesPerLine);

    /* OLD  - From GT1724 Manual - DEPRECATED
     * This is the general calculation for arbitraty values of numPhaseSteps, etc.
     * We simplify it above.
    // From section 5.5.2 of datasheet
    numOffsetStepsMax = (uint8_t)
            (  (double)imageMaximumSize /
               (
                   (double)numPhaseSteps * ( (double)scanCountResBits / 8.0 )
                   )
               );
    */

    qDebug() << " numPhaseSteps: " << numPhaseSteps
             << "; bytesPerLine: " << bytesPerLine
             << "; numOffsetStepsMax: " << numOffsetStepsMax;

    // Calculate the number of lines in the full scan:
    if (scanType == GT1724::GT1724_EYE_SCAN)
    {
        // FULL eye scan:
        offsetStart = 1;
        offsetStop  = 127;
        numOffsetSteps = 128 / scanVStep;
        if (numOffsetSteps == 128) numOffsetSteps = 127;

        /* OLD - From GT1724 Manual - DEPRECATED
         * Note: The calculation below is for arbitrary values of
         * offsetStart / offsetStop / scanVStep.
         * However, we simplify it (above) because we always scan
         * the entire diagram (Start=0; Stop=127) and scanVStep
         * is 1, 2, 4 or 8.
        numOffsetSteps = (uint8_t)
                ceil(  (double)(offsetStop - offsetStart + 1) / (double)scanVStep  );
        */

        scanVRes = numOffsetSteps;
        qDebug() << "Eye Scan - Total number of lines: " << numOffsetSteps;
    }
    else
    {
        // One line scan (bathtub plot):
        offsetStart = scanVOffset;
        offsetStop  = scanVOffset;
        thisOffsetStart = offsetStart;
        thisOffsetStop = offsetStop;
        numOffsetSteps = 1;
        scanVRes = 1;
        scanVStep = 1;
        qDebug() << "Bathtub Scan - One line at specified offset.";
    }

    // Calculate total number of samples, and allocate buffer for data:
    numSamples = numPhaseSteps * numOffsetSteps;
    bufferReset(eyeDataBufferTmp, numSamples);
    Q_ASSERT(eyeDataBufferTmp.size() == numSamples);  // Malloc error?
    eyeDataBufferIndex = 0;
    eyeDataBufferIndexMax = numSamples - 1;   // For sanity check
    rawDataBuffer = new uint8_t[imageMaximumSize];
    if (!rawDataBuffer || (eyeDataBufferTmp.size() != numSamples))
    {
        scanResult = globals::MALLOC_ERROR;
        goto finished;
    }

    ////// Eye scan will be run more than once for resolutions > 8 bit: /////////////////
    thisOffsetStart = offsetStart;
    outputSizeMSB = 0;
    outputSizeLSB = 0;

    //////////////////////////////////////////////////////////////
    /////// Scan the eye (in several steps if needed): ///////////
    //////////////////////////////////////////////////////////////

    if (numOffsetSteps > numOffsetStepsMax) thisOffsetStop = offsetStart + (numOffsetStepsMax - 1) * scanVStep;
    else                                    thisOffsetStop = offsetStop;

    parent->emitEyeScanProgressUpdate(laneOffset + scanLane, scanType, 0);

    while (thisOffsetStart <= offsetStop)
    {
        parent->eyeScanCheckForCancel();
        if (stopFlag)
        {
            qDebug() << "--Scan Cancelled. Stop.--";
            scanResult = globals::CANCELLED;
            goto finished;
        }
        //// SCAN: /////////////////////////////////////////////
        qDebug() << "** Starting part scan: **\n"
                 << "   phaseStart: " << 0
                 << "   phaseStop: " << 127
                 << "   phaseStep: " << scanHStep << "\n"
                 << "   thisOffsetStart: " << thisOffsetStart
                 << "   thisOffsetStop: " << thisOffsetStop
                 << "   offsetStep: " << scanVStep << "\n"
                 << "   resolution: " << scanCountResBits;

        scanResult = controlEyeSweep(0,        // phaseStart
                                     127,      // phaseStop
                                     scanHStep,
                                     thisOffsetStart,
                                     thisOffsetStop,
                                     scanVStep,
                                     scanCountResIndex,
                                     &outputSizeMSB,
                                     &outputSizeLSB);
        qDebug() << "** Part scan finished. Result: " << scanResult;
        if (scanResult != globals::OK)
        {
            qDebug() << "Error running scan: " << scanResult;
            goto finished;
        }
        parent->eyeScanCheckForCancel();
        if (stopFlag)
        {
            qDebug() << "--Scan Cancelled. Stop.--";
            scanResult = globals::CANCELLED;
            goto finished;
        }
        // Read back data:

        outputSize = static_cast<uint16_t>(static_cast<uint16_t>(outputSizeMSB) << 8) + static_cast<uint16_t>(outputSizeLSB);
        //// READ DATA: /////////////////////////////////////////////
        qDebug() << "--Reading back scan data: " << outputSize << " bytes --";

        scanResult = parent->rawRead24(0xFC,
                                       imageAddressMSB,
                                       imageAddressLSB,
                                       &rawDataBuffer[0],
                                       static_cast<size_t>(outputSize));
        if (scanResult != globals::OK)
        {
            qDebug() << "Error reading back scan data: " << scanResult;
            goto finished;
        }
        //// UNPACK DATA: //////////////////////////////////////////
        qDebug() << "--Unpacking scan data...\n"
                 << "  Tot Num Samples:  " << numSamples << "\n"
                 << "  THIS block size:  " << outputSize << "\n"
                 << "  Buffer Remaining: " << eyeDataBufferIndexMax - eyeDataBufferIndex + 1;



        for (size_t sampleIndex = 0; sampleIndex < static_cast<size_t>(outputSize); sampleIndex++)
        {
            // Sanity check: Make sure we aren't about to overflow the output buffer:
            Q_ASSERT(eyeDataBufferIndex <= eyeDataBufferIndexMax);
            if (eyeDataBufferIndex > eyeDataBufferIndexMax) { scanResult = globals::OVERFLOW; goto finished; }

            switch (scanCountResBits)
            {
            case 1: // 1  bit per sample:
                for (int bitIndex = 0; bitIndex < 8; bitIndex++)
                {
                    // For each bit, shift and mask, and store to output as uint8_t:
                    eyeDataBufferTmp[eyeDataBufferIndex] = static_cast<double>((rawDataBuffer[sampleIndex] >> (7-bitIndex)) & 0x01);
                    eyeDataBufferIndex++;
                }
                break;
            case 2: // 2  bits per sample:
                for (int bitIndex = 0; bitIndex < 8; bitIndex+=2)
                {
                    // For each bit, shift and mask, and store to output as uint8_t:
                    eyeDataBufferTmp[eyeDataBufferIndex] = static_cast<double>((rawDataBuffer[sampleIndex] >> (6-bitIndex)) & 0x03);
                    eyeDataBufferIndex++;
                }
                break;
            case 4: // 4  bits per sample:
                for (int bitIndex = 0; bitIndex < 8; bitIndex+=4)
                {
                    // For each bit, shift and mask, and store to output as uint8_t:
                    eyeDataBufferTmp[eyeDataBufferIndex] = static_cast<double>((rawDataBuffer[sampleIndex] >> (4-bitIndex)) & 0x0F);
                    eyeDataBufferIndex++;
                }
                break;
            case 8: // 8  bits per sample:
                eyeDataBufferTmp[eyeDataBufferIndex] = static_cast<double>(rawDataBuffer[sampleIndex]);
                eyeDataBufferIndex++;
            }
        }

        //// Advance start and stop offsets: ///////////////////////
        qDebug() << "--Adjusting offsets for next part...";
        //Start = Stop + OffsetStep:
        thisOffsetStart = thisOffsetStop + scanVStep;
        //Stop = Start + (NumOffsetStepsMax - 1) * OffsetStep:
        thisOffsetStop = thisOffsetStart + ( (numOffsetStepsMax - 1) * scanVStep );
        //If Stop is greater than OffsetStop then Stop = OffsetStop:
        if (thisOffsetStop > offsetStop) thisOffsetStop = offsetStop;

        // Update progress:
        parent->emitEyeScanProgressUpdate(laneOffset + scanLane, scanType, (eyeDataBufferIndex * 100) / numSamples);
    }

#define BERT_EYESCAN_SHIFT 1    // Define this to enable eye / bathtub "Shift" (make sure eye starts at left edge for visual appeal)
// #define BERT_EYESCAN_EXTEND 1   // Define this to enable eye "Extend" (copy some data from start of eye and place it at end for visual appeal; i.e. width of plot is greater than 1 UI)

    if (scanResult == globals::OK)
    {
        // Finished successfully.
        QVector<double> eyeDataBufferTmpAdj;  // Vector for adjusted data (shift / extend)

        if (scanType == GT1724::GT1724_EYE_SCAN)
        {
    #ifdef BERT_EYESCAN_SHIFT
            //////// SHIFT: //////////////////////////////////////////////////
            // For eye plots: We want the "eye" to be centred in the plot; however
            // sample 0 of scan data is actually before the eye start. Cut the left
            // end and splice it onto the right end. The amount to rotate depends on
            // whether we are also doing an "extend" (see below).
            // Nb: Only adjust the shift amount on the first scan after a reset:
            if (resetFlag)
            {
                uint8_t peakIndex = peakFind(eyeDataBufferTmp, numPhaseSteps, numOffsetSteps);
                const uint8_t xMid = (numPhaseSteps / 2);  // Index of mid point
         #ifdef BERT_EYESCAN_EXTEND
                // If "extending" the diagram, shift a bit less (for visual appeal):
                if (peakIndex <= xMid) nShift = (int)(peakIndex/scanHStep) + 1;
                else                   nShift = ((int)peakIndex - (int)numPhaseSteps) / (int)scanHStep - 1;
                nShift = nShift - (int)(0.11*(double)numPhaseSteps);
                if (nShift < (-1 * (int)numPhaseSteps)) nShift = (-1 * (int)numPhaseSteps);
        #else
                // Not using "Extend": shift "peak" errors to left edge.
                if (peakIndex <= xMid) nShift = static_cast<int>(peakIndex);
                else                   nShift = static_cast<int>(peakIndex - numPhaseSteps);

                /* OLD Don't know why this was divided by scanHStep ???
                if (peakIndex <= xMid) nShift = (int)(peakIndex/scanHStep);
                else                   nShift = ((int)peakIndex - (int)numPhaseSteps) / (int)scanHStep;
                */
        #endif
            }
            qDebug() << "SHIFT: Rotate eye plot " << nShift << " samples";
            QVector<double> eyeDataBufferTmpShf;  // Vector for shifted data
            scanResult = dataShift(eyeDataBufferTmp,
                                   eyeDataBufferTmpShf,
                                   numPhaseSteps,
                                   numOffsetSteps,
                                   nShift);
            if (scanResult != globals::OK) goto finished;
    #else   // No Shift:
            QVector<double> eyeDataBufferTmpShf;  // Vector for shifted data
            eyeDataBufferTmpShf = eyeDataBufferTmp;
    #endif  // Shift ?

    #ifdef BERT_EYESCAN_EXTEND
            //////// EXTEND: /////////////////////////////////////////////////
            // For eye plot: "extend" the eye width by copying the left side
            // and adding it to the right side (improves readability):
            uint8_t extraPhaseSteps = (uint8_t)((float)numPhaseSteps * 0.215f);
            qDebug() << "EXTEND: Extending eye by " << extraPhaseSteps << " steps.";

            uint8_t numPhaseStepsExt = numPhaseSteps + extraPhaseSteps;
            size_t numSamplesExt = numPhaseStepsExt * numOffsetSteps;
            eyeDataBufferTmpAdj.clear();
            eyeDataBufferTmpAdj.resize((int)numSamplesExt);
            eyeDataBufferTmpAdj.fill(0.0);

            size_t x, y, sampleIndex, storeIndex;
            storeIndex = 0;
            for (y = 0; y < numOffsetSteps; y++)
            {
                for (x = 0; x < numPhaseStepsExt; x++)
                {
                    if (x < numPhaseSteps) sampleIndex = (y * numPhaseSteps) + x;
                    else                   sampleIndex = (y * numPhaseSteps) + (x - numPhaseSteps);
                    eyeDataBufferTmpAdj[storeIndex] = eyeDataBufferTmpShf[sampleIndex];
                    storeIndex++;
                }
            }
            scanHRes = numPhaseStepsExt;
            ////////////////////////////////////////////////////////////////////
    #else   // No Extend:
        eyeDataBufferTmpAdj = eyeDataBufferTmpShf;
    #endif  // Extend?

        }
        else
        {
    #ifdef BERT_EYESCAN_SHIFT
            //////// SHIFT: ////////////////////////////////////////////////////
            // For bathtub plots: We want the plot to show one "eye" (tub),
            // with the left side being the max error point at the start of the
            // eye; however sample 0 of scan data is actually before the eye
            // start. Cut the left end and splice it onto the right end:
            // Nb: Only adjust the shift amount on the first scan after a reset:
            if (resetFlag)
            {
                uint8_t peakIndex = peakFind(eyeDataBufferTmp, numPhaseSteps, numOffsetSteps);
                const uint8_t xMid = (numPhaseSteps / 2);  // Index of mid point

                if (peakIndex <= xMid) nShift = static_cast<int>(peakIndex);
                else                   nShift = static_cast<int>(peakIndex - numPhaseSteps);

                /* OLD Don't know why this was divided by scanHStep ???
                if (peakIndex <= xMid) nShift = (int)(peakIndex/scanHStep);
                else                   nShift = ((int)peakIndex - (int)numPhaseSteps) / (int)scanHStep;
                */
            }
            qDebug() << "SHIFT: Rotate bathtub plot " << nShift << " samples";
            scanResult = dataShift(eyeDataBufferTmp,
                                   eyeDataBufferTmpAdj,
                                   numPhaseSteps,
                                   numOffsetSteps,
                                   nShift);
            if (scanResult != globals::OK) goto finished;
            ////////////////////////////////////////////////////////////////////
    #else   // No Shift:
            eyeDataBufferTmpAdj = eyeDataBufferTmp;
    #endif  // Shift ?

        }

        // Data are stored in a global buffer which accumulates data between
        // repeated scans, until a new scan is started.
        if (resetFlag)
        {
            // New Scan... reset the global scan data buffer.
            bufferReset(eyeDataBuffer, eyeDataBufferTmpAdj.size());
            bufferReset(eyeDataBufferNorm, eyeDataBufferTmpAdj.size());
        }
        // Ensure that global scan data buffer is the same size as the results from this scan:
        Q_ASSERT(eyeDataBuffer.size() == eyeDataBufferTmpAdj.size());
        if (eyeDataBuffer.size() != eyeDataBufferTmpAdj.size())
        {
            // For production, if the size changes for some reason, throw away the old data:
            bufferReset(eyeDataBuffer, eyeDataBufferTmpAdj.size());
            bufferReset(eyeDataBufferNorm, eyeDataBufferTmpAdj.size());
        }

        ////// ACCUMULATE / NORMALISE: ////////////////////////////////////////////////
        double nBitsAnalysed = (double)((uint16_t)(1 << scanCountResBits)) * (double)scanRepeatCount;
        const double eyeScanLogFloor = 1.0 / (double)nBitsAnalysed;

        // For debugging eye scan data as CSV: #define DEBUG_EYE_DATA(MSG) qDebug() << MSG;
        #define DEBUG_EYE_DATA(MSG)  // No debug.
        DEBUG_EYE_DATA("Repeats Done: " << scanRepeatCount << "; Bits Analysed: " << nBitsAnalysed << "; Log Floor: " << eyeScanLogFloor)
        DEBUG_EYE_DATA("-----------------------------------------")
        DEBUG_EYE_DATA("Repeats,TotalBits,Floor")
        DEBUG_EYE_DATA(scanRepeatCount << "," << nBitsAnalysed << "," << eyeScanLogFloor)
        DEBUG_EYE_DATA("")
        DEBUG_EYE_DATA("i,Errors,BER")

// Eye Data Quantisation Error: Quick Fix:
// Define EYE_DATA_QUANTISATION_QUICKFIX to crop counts of 1 (these contain quantisation error).
// Note this is a quick fix which introduces an opposite error, i.e. dropping some valid error counts.
//#define EYE_DATA_QUANTISATION_QUICKFIX 1

        for (int i = 0; i < eyeDataBufferTmpAdj.size(); i++)
        {
#ifdef EYE_DATA_QUANTISATION_QUICKFIX
            if (eyeDataBufferTmpAdj[i] > 1)
            {
                eyeDataBuffer[i] += eyeDataBufferTmpAdj[i];   // Add most recent scan to all previous scans
            }
            /* EXPERIMENTAL AND HACKY.
            else if (eyeDataBufferTmpAdj[i] == 1)
            {
                // Error count of 1: This count includes a slight over-estimate of errors, so fudge it a bit...
                // I.e. only count the error 75% of the time.
                if((rand() % 100) > 85) eyeDataBuffer[i]++;
            }
            */
#else
            eyeDataBuffer[i] += eyeDataBufferTmpAdj[i];   // Add most recent scan to all previous scans
#endif
            // Normalise the data point (using log 10):
            double thisValue = eyeDataBuffer[i] / nBitsAnalysed;
            if (thisValue < eyeScanLogFloor)
            {
                DEBUG_EYE_DATA("," << eyeDataBuffer[i] << "," << eyeScanLogFloor << ", V")
                if (scanType == GT1724::GT1724_EYE_SCAN) eyeDataBufferNorm[i] = log10(eyeScanLogFloor);
                else                                     eyeDataBufferNorm[i] = globals::BELOW_DETECTION_LIMIT;
                    // For Bathtub Plot Only: Set "floor" of plot (= error rate below detection limit) to a very negative value.
                    // Used by bathtub plot widget to hide invalid values at the bottom of the plot curve.
            }
            else
            {
                DEBUG_EYE_DATA("," << eyeDataBuffer[i] << "," << thisValue << ", ")
                eyeDataBufferNorm[i] = log10(thisValue);
            }
        }
        DEBUG_EYE_DATA("-----------------------------------------")
        ////////////////////////////////////////////////////////////////////////////////

        // Data successfully aquired!
        qDebug() << "--Scan data aquired! Transmitting results...";

//#define BERT_EYESCAN_EXTRA_DEBUG
#ifdef BERT_EYESCAN_EXTRA_DEBUG
        //// DEBUG: Dump Eye Data: ///////////////////////////
        qDebug() << "---- RAW Data ------------------------------------------";
        qDebug() << "scanHRes,scanVRes";
        qDebug() << scanHRes << "," << scanVRes;
        qDebug() << "";
        size_t xx, yy;
        for (yy=0; yy < scanVRes; yy++)
        {
            QString debugString;
            for (xx=0; xx < scanHRes; xx++)
            {
                debugString += QString::number(eyeDataBuffer[(yy * scanHRes) + xx]);
                if (xx < (scanHRes-1)) debugString += QString(", ");
            }
            qDebug() << debugString;
        }
        qDebug() << "--------------------------------------------------------" << endl;
        //////////////////////////////////////////////////////
#endif

        parent->emitEyeScanFinished(laneOffset + scanLane, scanType, eyeDataBufferNorm, scanHRes, scanVRes);
qDebug() << "--Transmitting data. scanHRes: " << scanHRes << "; scanVRes: " << scanVRes;
    }
    qDebug() << "**Eye Scan finshed OK. **";

  finished:
    // Clean up temp buffer:
    if (rawDataBuffer) delete [] rawDataBuffer;

    if (scanResult == globals::OVERFLOW) qDebug() << "ERROR: Ran out of space in output buffer!";
    if (scanResult != globals::OK) parent->emitEyeScanError(laneOffset + scanLane, scanType, scanResult);
    return scanResult;
}





/*!
 \brief Find the location of the "peak" in the centre row of the eye scan
 \param data   Reference to vector of scan data (doubles)
               The array pointed to by data must contain
               sizeX * sizeY elements
 \param sizeX  Number of sample points horizontally (minimum 2)
 \param sizeY  Number of rows (sample points vertically) (minimum 1)

 \return index  Index of maximum value on the x axis, for
                the row through the vertical centre of the plot
                This is a value between 0 and sizeX inclusive.
*/
uint8_t EyeMonitor::peakFind(QVector<double> &data,
                             const size_t sizeX,
                             const size_t sizeY)
{
qDebug() << "PEAK FIND:";
    double zMax = 0.0;
    size_t maxIndex = 0;
    const size_t yMid = sizeY / 2;
    const size_t startIndex = (yMid * sizeX);
    size_t sampleIndex = startIndex;
    for (size_t x = 0; x < sizeX; x++)
    {
        if (data[sampleIndex] >= zMax)
        {
            maxIndex = (sampleIndex - startIndex);
            zMax = data[sampleIndex];
        }
        sampleIndex++;
    }
    return maxIndex;
}




/*!
 \brief Rotate data array left or right

 Allocates a NEW data array of the same size as the
 source array. The CALLER must free the new array
 with  delete [] (BUT see notes on return value below)

 Each element is shifted left or right within its row, with
 the end of the row being 'wrapped' around, according to the
 value of nShift

 \param data         Reference to array of raw scan data (doubles)
                     The array pointed to by data must contain
                     sizeX * sizeY elements

 \param dataShifted  Reference to a vector to receive the shifted data.
                     If it contains any existing data, the data will be overwritten.

 \param sizeX  Number of sample points horizontally (minimum 2)
 \param sizeY  Number of rows (sample points vertically) (minimum 1)
 \param nShift Number of places to shift the data.
                 Positive: Shifts the corresponding plot RIGHT
                 Negative: Shifts the corresponding plot LEFT
               The absolute value of nShift must be between 0
               and sizeX (inclusive)

 \return globals::OK          Data shifted OK
 \return globals::GEN_ERROR   Couldn't shift (shift size too big?)
*/
int EyeMonitor::dataShift(QVector<double> &data,
                          QVector<double> &dataShifted,
                          const size_t sizeX,
                          const size_t sizeY,
                          const int nShift)
{
    // SHIFT: Cut one side off the array and place it on the other side.
    // A negative shift (shift plot to RIGHT) is actually a
    // complimentary positive shift, i.e. (size - abs(shift))
    size_t nShiftAbs;
    if (nShift >= 0)
      {  nShiftAbs = (size_t)nShift;  }
    else
      {  nShiftAbs = (size_t)(sizeX + nShift);  }
    // Check bounds: Must be between 0 and sizeX:
    if (nShiftAbs > sizeX) return globals::GEN_ERROR;

    const size_t nSamples = (sizeX * sizeY);
    Q_ASSERT(nSamples == (size_t)data.size());
    if (nSamples != (size_t)data.size()) return globals::GEN_ERROR;
    bufferReset(dataShifted, (int)nSamples);
    size_t x, y, sampleIndex, storeIndex;
    storeIndex = 0;
    for (y = 0; y < sizeY; y++)
    {
        for (x = 0; x < sizeX; x++)
        {
            sampleIndex = (y * sizeX) + x;
            if (x < nShiftAbs) storeIndex = (y * sizeX) + x + (sizeX - nShiftAbs);
            else               storeIndex = (y * sizeX) + x - nShiftAbs;
            dataShifted[storeIndex] = data[sampleIndex];
        }
    }
    return globals::OK;
}




/*!
 \brief Run Query Eye Scanner Output Memory Attributes Macro
 \param addressMSB  Pointer to a uint8_t to store MSB of address for scan data
 \param addressLSB  Pointer to a uint8_t to store LSB of address for scan data
 \param sizeMSB     Pointer to a uint8_t to store MSB of max data size
 \param sizeLSB     Pointer to a uint8_t to store LSB of max data size
                    If an error occurs, the values of the parameters will be set to 0.
 \return globals::OK      Success
 \return [Error Code]     Error from hardware/comms functions
*/
int EyeMonitor::queryEyeScanMem(uint8_t *addressMSB,
                                uint8_t *addressLSB,
                                uint8_t *sizeMSB,
                                uint8_t *sizeLSB)
{
    // Default results: Set to 0 (in case of error...).
    *addressMSB = 0;
    *addressLSB = 0;
    *sizeMSB    = 0;
    *sizeLSB    = 0;
    // Use Query Eye Scanner Output Memory Attributes:
    uint8_t outputData[4];
    int result;
    result = parent->runMacro( 0x41, NULL, 0, outputData, 4 );
    if (result != globals::OK) return result;     //  Macro error...
    // Copy results to output parameters:
    *addressMSB = outputData[0];
    *addressLSB = outputData[1];
    *sizeMSB    = outputData[2];
    *sizeLSB    = outputData[3];
    return globals::OK;
}




/*!
 \brief Set up and start an eye scan sweep
 Calls the "Control Eye Sweep" macro. See notes in
 GT1724 data sheet for notes on how to select the
 parameters.

 \param phaseStart  Starting Phase offset. Valid range is 0 to 127
 \param phaseStop   Phase offset limit. Valid range is 0 to 127
                    Phase Stop must be greater than or equal to
                    Phase Start
 \param phaseStep   Unsigned value for phase step size. Valid
                    range is 1 to 128
 \param offsetStart Starting voltage offset. Valid range is 1 to 127
 \param offsetStop  Voltage offset limit. Valid range is 1 to 127.
                    Offset Stop must be greater than or equal to
                    Offset Start
 \param offsetStep  Unsigned value for voltage offset step size.
                    Valid range is 1 to 127
 \param resolution  Resolution of the eye error count for each
                    data point:
                        0 = 1-bit
                        1 = 2-bit
                        2 = 4-bit
                        3 = 8-bit
 \param sizeMSB    Used to return MSB of size of the output image
 \param sizeLSB    Used to return LSB of size of the output image

 \return globals::OK        Success
 \return globals::OVERFLOW  Parameter out of range
 \return [Error Code]     Error from hardware/comms functions
*/
int EyeMonitor::controlEyeSweep(const uint8_t phaseStart,
                                const uint8_t phaseStop,
                                const uint8_t phaseStep,
                                const uint8_t offsetStart,
                                const uint8_t offsetStop,
                                const uint8_t offsetStep,
                                const uint8_t resolution,
                                uint8_t *sizeMSB,
                                uint8_t *sizeLSB)
{
    if ( (phaseStart > 127) ||
         (phaseStop > 127) ||
         (phaseStart > phaseStop) ||
         (phaseStep < 1) || (phaseStep > 128) ||
         (offsetStart < 1) || (offsetStart > 127) ||
         (offsetStop < 1) || (offsetStop > 127) ||
         (offsetStart > offsetStop) ||
         (offsetStep < 1) || (offsetStep > 127) ||
         (resolution > 3) ) return globals::OVERFLOW;  // Parameter out of range

    // Default results: Set to 0 (in case of error...).
    *sizeMSB = 0;
    *sizeLSB = 0;
    // Run the "Control Eye Sweep" macro:
    uint8_t inputData[8] = { (const uint8_t)scanLane,
                             phaseStart,
                             phaseStop,
                             phaseStep,
                             offsetStart,
                             offsetStop,
                             offsetStep,
                             resolution };
    uint8_t outputData[2];

    int result;
    result = parent->runMacro(0x42, inputData, 8, outputData, 2);
    if (result != globals::OK) return result;     //  Macro error...

    // Unpack the output data (memory size used):
    *sizeMSB = outputData[0];
    *sizeLSB = outputData[1];
    return globals::OK;
}




/*!
 \brief Clear a buffer of double values, and set to a new size.
        The new vector is filled with 0.0
 \param buffer
 \param newSize
*/
void EyeMonitor::bufferReset(QVector<double> &buffer, int newSize)
{
    buffer.clear();
    buffer.resize(newSize);
    buffer.fill(0.0);
}


