#include <QDebug>
#include "tlc59108.h"
#include "globals.h"
#include "I2CComms.h"
#include "BertModel.h"
#include "mainwindow.h"



/*
* \brief Constructor    - Stores a ref to the comms module and the I2C address for the TLC59108
* \param comms            Reference to a BertComms object representing the connection to an
*                         instrument. Used to carry out I2C operations
* \param i2cAddress       I2C slave address of this TLC59108 component

*/


TLC59108::TLC59108(I2CComms *comms, const uint8_t i2cAddress, int deviceID)
: comms(comms), i2cAddress(i2cAddress), deviceID(deviceID)
{}


/*!
\brief Check an I2C address to see whether there is a TLC59108 present
       Note: Relies on writing to a register on the TCL59108 and reading the value back.
       If another hardware device exists on the specified address, it may be damaged,
       produce unexpected behaviour or respond in a way that looks like a TLC59108!

\param comms       Pointer to I2C Comms object (created elsewhere and already connected)
\param i2cAddress  Address to check on
\return true       TLC59108 found
\return false      No response or unexpected response
*/
bool TLC59108::ping(I2CComms *comms, const uint8_t i2cAddress)
{
   qDebug() << "TLC59108: Searching on address " << INT_AS_HEX(i2cAddress,2) << "...";
   Q_ASSERT(comms->portIsOpen());
   if (!comms->portIsOpen()) return globals::NOT_CONNECTED;

   int result;

   result = comms->pingAddress(i2cAddress);
   if (result != globals::OK)
   {
       qDebug() << "TLC59108 not found (no ACK on I2C address; result: " << result << ")";
       return false;
   }
   else
   {
       qDebug() << "TLC59108 found (no ACK on I2C address; result: " << result << ")";
       return true;
   }

}


/*!
\brief Get Options for TLC59108
       Requests that this module emit signals describing its available options lists to client
*/
void TLC59108::getOptions()
{
  // Nothing to do; No options lists for TLC59108.
}




/*!
\brief TLC59108        Initialisation - set up LED Mode1
\return globals::OK    Success!
\return [error code]   Error connecting or setting up. Comms problem or invalid I2C address?

*/
int TLC59108::init()
{
   qDebug() << "TLC59108: Init for TLC59108 " << INT_AS_HEX(i2cAddress,2);
   Q_ASSERT(comms->portIsOpen());
   int result;
   uint8_t regAddress = 0;
   uint8_t data_Mode1 = 0;

   // Write the Control register - Set Mode1:
   result = comms->write8(i2cAddress, regAddress, &data_Mode1, 1);

   if (result != globals::OK)
   {
       qDebug() << "TLC59108: Error setting LED Mode1 (" << result << ")";
       return result;
   }

    //Turn off all LEDs
    ledOn[0] = false;   //Lane0 LEDs initial value
    ledOn[1] = false;   //Lane1 LEDs initial value
    ledOn[2] = false;   //Lane2 LEDs initial value
    ledOn[3] = false;   //Lane3 LEDs initial value
    ledData0 = 0x00; //
    ledData1 = 0x00; //

    comms->write8(i2cAddress,ledOut0,&ledData0,1); //Lane0&Lane1 turn-off
    comms->write8(i2cAddress,ledOut1,&ledData1,1); //Lane2&lane3 turn-off

    return globals::OK;
}




//********************Slot******************//

void TLC59108::ChangPGLedStatus(int lane, bool laneOn)
{
     // if (!comms->portIsOpen()) return globals::NOT_CONNECTED;
    qDebug() << "Received Sig ChangPGLedStatus for lane " << lane << ", laneOn " << laneOn;
    // Write the LED register:
    Q_ASSERT(comms->portIsOpen());

    ledOn[lane] = laneOn;

    //4Ch PG, ledOut0 ->PG1(lane0) and PG2(lane1), ledOut1 ->PG3(lane2) &PG4(lane4)
    if(BertModel::UseFourChanPGMode())
    {
           if(lane == 0 || lane == 1)
           {
                  if(ledOn[1] == true && ledOn[0] == false)
                  {
                       ledData0 = 0x40;
                  }
                  else if(ledOn[1] == true && ledOn[0] == true)
                  {
                       ledData0 = 0x44;
                  }
                  else if(ledOn[1] == false && ledOn[0] == true)
                  {
                       ledData0 = 0x04;
                  }
                  else
                  {
                       ledData0 = 0x00;
                  }
                   comms->write8(i2cAddress,ledOut0,&ledData0,1);
            }
            else if(lane == 2 || lane == 3)
            {
                   if(ledOn[3] == true && ledOn[2] == false)
                   {
                        ledData1 = 0x10;
                   }
                   else if(ledOn[3] == true && ledOn[2] == true)
                   {
                        ledData1 = 0x11;
                   }
                   else if(ledOn[3] == false && ledOn[2] == true)
                   {
                        ledData1 = 0x01;
                   }
                   else if(ledOn[3] == false && ledOn[2] == false)
                   {
                        ledData1 = 0x00;
                   }
                   comms->write8(i2cAddress,ledOut1,&ledData1,1);
             }
     }

    //2CH PG - ledOut0 ->PG1(lane0) and PG2(lane2)
     if(ledOn[2] == true && ledOn[0] == false)
     {
         ledData0 = 0x40;
     }
     else if(ledOn[2] == true && ledOn[0] == true)
     {
         ledData0 = 0x44;
     }
     else if(ledOn[2] == false && ledOn[0] == true)
     {
         ledData0 = 0x04;
     }
     else
     {
         ledData0 = 0x00;
     }
     comms->write8(i2cAddress,ledOut0,&ledData0,1);
     qDebug()<<"Change LED successfully! PG" <<lane+1<<"Status"<<laneOn;
    // return globals::OK;

}


void TLC59108::EDStartLed()
{
    ledOn[1] = false;
    ledOn[3] = false;
    Green[1] = true;
    Green[3] = true;
    edUpdateCounter[1] = 0;
    edUpdateCounter[3] = 0;
    qDebug() <<" EDs are ready to go.";
}




void TLC59108::EDLedFlash(int lane, bool edRunning, bool edErroflashOn)
{
    //Update ED Leds every 0.25s
    if(edUpdateCounter[lane]>=4) edUpdateCounter[lane] = 0;
       edUpdateCounter[lane]++;
    if(!edRunning)
    {
        ledOn[1] = false;
        ledOn[3] = false;
        ledData1 = 0x00;
        comms->write8(i2cAddress,ledOut1,&ledData1,1);
    }
    else
    {
        if(edUpdateCounter[lane] == 1)
        {
            ledOn[lane] = false;
            ChangEDLedStatus();
            if(edErroflashOn)  Green[lane] = false;
            else Green[lane] = true;
        }
        else
        {
            ledOn[lane] = true;
            ChangEDLedStatus();
            if(edErroflashOn)  Green[lane] = false;
            else Green[lane] = true;
        }
    }
}



void TLC59108::EDStopLed()
{
    ledOn[1] = false;
    ledOn[3] = false;
    ledData1 = 0x00;
    comms->write8(i2cAddress,ledOut1,&ledData1,1);
    qDebug() <<" All ED LEDs are off.";
}




//2CH ED - ledOut1 ->ED1(lane1) and ED2(lane3)
void TLC59108::ChangEDLedStatus()
{
        if(ledOn[1] == false && ledOn[3] == false) ledData1 = 0x00; // both ED1&ED2 off
        else if(ledOn[1] == false && ledOn[3] == true)  // ED1 off and ED2 on
        {
              if(Green[3] == false) ledData1 = 0x40;  // ED2 Green
              else ledData1 = 0x10;                        // ED2 Red
        }
        else if(ledOn[1] == true && ledOn[3] == false)  //ED1 on and ED2 off
        {
              if(Green[1] ==false) ledData1 = 0x04;       //ED1 Red
              else ledData1 = 0x01;                           //ED1 Green
         }
         else if(ledOn[1] == true && ledOn[3] == true)  //both ED1 and ED2 on
         {
              if(Green[1] == true && Green[3] == false ) ledData1 = 0x41; //ED1 Green and ED2 Red
              else if (Green[1] == false && Green[3] == true) ledData1 = 0x14; //ED1 Red and ED2 Green
              else if (Green[1] == false && Green[3] == false) ledData1 = 0x44; //ED1 Red and ED2 Red
              else if(Green[1] == true && Green[3] == true) ledData1 = 0x11; //ED1 Green and ED2 Green
         }
         comms->write8(i2cAddress,ledOut1,&ledData1,1);

         qDebug() <<"Change the ED's LEDs successfully.";

}

