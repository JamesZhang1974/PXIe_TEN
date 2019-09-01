/*!
 \file   PCA9557A.cpp
 \brief  Texas Instruments PCA9557 I/O Expander Hardware Interface
         This class provides an interface to control a PCA9557 I/O expander
 \author J Cole-Baker (For Smartest)
 \date   Jul 2018
*/

#include <QDebug>

#include "PCA9557A.h"

// -- Constants for Trigger Divide Ratio (pins p6 and p7): --//UNUSED: Remove to PCA9557B.
//const uint8_t PCA9557::TRIGGER_DIVIDE_BITMASK = 0xC0;  // Mask with bits 6 and 7 set
// Constants used to populate the Trigger Divide Ratio list:
//const QList<uint8_t> PCA9557::TRIGGER_DIVIDE_LOOKUP =  { 0xC0,  0x80,  0x40  };
//const QStringList PCA9557::TRIGGER_DIVIDE_LIST      =  { "1/2", "1/4", "1/8" };
//const size_t PCA9557::TRIGGER_DIVIDE_DEFAULT_INDEX = 0;

// -- Constants for EEPROM Write Control (pin p2): --
const uint8_t PCA9557A::EEPROM_WC_BITMASK    = 0x04;   // Mask with bit 2 set
const uint8_t PCA9557A::EEPROM_WRITE_ENABLE  = 0x00;   // Bit CLEAR = Write ENABLE
const uint8_t PCA9557A::EEPROM_WRITE_DISABLE = 0x04;   // Bit SET = Write DISABLE


/*!
 * \brief Constructor - Stores a ref to the comms module and the I2C address for the PCA9557A
 * \param comms        Reference to a BertComms object representing the connection to an
 *                         instrument. Used to carry out I2C operations
 * \param i2cAddress  I2C slave address of this PCA9557A(SlaveAddress:0x1C) component
 * \param deviceID         Unique ID for this device in systems where there may be
 *                         several PCA9557A ICs. Assigned by the instantiator, probably
 *                         starting from 0.
 */
PCA9557A::PCA9557A(I2CComms *comms, const uint8_t i2cAddress, const int deviceID)
 : comms(comms), i2cAddress(i2cAddress), deviceID(deviceID)
{}


/*!
 \brief Check an I2C address to see whether there is a PCA9557A present
        Note: Relies on writing to a register on the PCA9557A and reading the value back.
        If another hardware device exists on the specified address, it may be damaged,
        produce unexpected behaviour or respond in a way that looks like a PCA9557A!

 \param comms       Pointer to I2C Comms object (created elsewhere and already connected)
 \param i2cAddress  Address to check on
 \return true   PCA9557A found
 \return false  No response or unexpected response
*/
bool PCA9557A::ping(I2CComms *comms, const uint8_t i2cAddress)
{
    qDebug() << "PCA9557A: Searching on address " << INT_AS_HEX(i2cAddress,2) << "...";
    Q_ASSERT(comms->portIsOpen());
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;

    int result;

    result = comms->pingAddress(i2cAddress);
    if (result != globals::OK)
    {
        qDebug() << "PCA9557A not found (no ACK on I2C address; result: " << result << ")";
        return false;
    }

    // Read initial register values
    qDebug() << "-Reading initial register value:";
    uint8_t regPolarity = 0;
    result = comms->read8(i2cAddress, REG_POLARITY, &regPolarity, 1);
    if (result != globals::OK)
    {
        qDebug() << "PCA9557A not found (error reading register: " << result << ")";
        return false;
    }
    qDebug() << "-Result: " << result << "; Register Value: " << INT_AS_HEX(regPolarity, 2);
    // OK! Set to a test value...
    qDebug() << "-Setting POLARITY INVERSION register to '01010101b' (0x55):";
    uint8_t regPolarityNew = 0x55;
    result = comms->write8(i2cAddress, REG_POLARITY, &regPolarityNew, 1);
    if (result != globals::OK)
    {
        qDebug() << "PCA9557A not found (error writing test value to register: " << result << ")";
        return false;
    }
    qDebug() << "-Reading back POLARITY INVERSION register...";
    result = comms->read8(i2cAddress, REG_POLARITY, &regPolarityNew, 1);
    if (result != globals::OK)
    {
        qDebug() << "PCA9557A not found (error reading test value from register: " << result << ")";
        return false;
    }
    qDebug() << QString("  Value: [0x%1]").arg(regPolarityNew,2,16,QChar('0'));
    if (regPolarityNew == 0x55) qDebug() << "  -OK!";
    else                        qDebug() << "  -Value didn't match!";

    // Set the polarity inversion register back to original value:
    qDebug() << "-Restoring POLARITY INVERSION register...";
    comms->write8(i2cAddress, REG_POLARITY, &regPolarity, 1);

    return (regPolarityNew == 0x55);
}


/*!
 \brief Get Options for PCA9557A
        Requests that this module emit signals describing its available options lists to client
*/
void PCA9557A::getOptions()
{
    // No option for PCA9557A
    // emit ListPopulate("listLMXTrigOutDivRatio", globals::ALL_LANES, TRIGGER_DIVIDE_LIST, TRIGGER_DIVIDE_DEFAULT_INDEX);
}




 /*!
 \brief PCA9557A  Initialisation
 \return globals::OK    Success!
 \return [error code]   Error connecting or setting up. Comms problem or invalid I2C address?
 SIGNALS:
  emits ShowMessage(...) to show progress messages to user
*/
int PCA9557A::init()
{
    qDebug() << "PCA9557A: Init for PCA9557A with ID " << deviceID << "; I2C Address " << INT_AS_HEX(i2cAddress,2);
    Q_ASSERT(comms->portIsOpen());

    // Configure the PCA9557A pins as inputs or outputs as required by the instrument functions (see schematics):
    int result;
    result = configurePins(NORMAL_INPUT,  // IO0: !CRST_A    GT1724 A Reset: Pulled high by R29; Unused, Cfg as Input; Change to OUTPUT if required.
                           NORMAL_INPUT,  // IO1: !CRST_B    GT1724 B Reset: Pulled high by R38; Unused, Cfg as Input; Change to OUTPUT if required.
                           OUTPUT,        // IO2: !WC        M24M02 EEPROM (U12) Write Control: Drive HIGH to DISABLE writes; Floating = Write Enable
                           NORMAL_INPUT,  // IO3: MISO/LCKD  LMX Clock MISO serial OR VCO Lock: Using as VCO lock input
                           NORMAL_INPUT,  // IO4: LOS_A      GT1724 A LOS Indicator (High = Loss Of Signal)
                           NORMAL_INPUT,  // IO5: LOS_B      GT1724 B LOS Indicator (High = Loss Of Signal)
                           NORMAL_INPUT,  // IO6: //UNUSED
                           NORMAL_INPUT); // IO7: //UNUSED
    if (result != globals::OK)
    {
        qDebug() << "PCA9557a: Error setting pin configuration (" << result << ")";
        emit ShowMessage("Error configuring I/O controller!");
        return result;
    }

    // Set safe default settings:

    //  - EEPROM Write DISABLED
    result = setPins(EEPROM_WRITE_DISABLE);    // Could bitwise OR other pin values here.

    if (result != globals::OK)
    {
        qDebug() << "PCA9557A: Error setting pins (" << result << ")";
        emit ShowMessage("Error configuring I/O controller!");
    }
    return result;
}


//---------------------------------------------------------
// SLOTS
//---------------------------------------------------------



/*!
 \brief SLOT: Set EEPROM Write Enable
 \param enable  True  - EEPROM can be written
                False - EEPROM Locked (can't be written)
*/
void PCA9557A::SetEEPROMWriteEnable(bool enable)
{
    uint8_t newValue;
    if (enable) newValue = EEPROM_WRITE_ENABLE;
    else        newValue = EEPROM_WRITE_DISABLE;

    int result = updatePins(EEPROM_WC_BITMASK, newValue);
    emit Result(result, globals::ALL_LANES);
}


/*!
 \brief SLOT: Read level of "Lock Detect" pin from LMX clock (wired to our input IO3)
 Assumes that IO3 is already set up as an input!
 Note: emits LMXLockDetect on success
 Does NOT emit Result (designed to be used in the background from timer).
*/
void PCA9557A::ReadLMXLockDetect()
{
    uint8_t value = 0;
    int result = PCA9557A::getPins(&value);
    if (result == globals::OK)
    {
        // qDebug() << "PCA9557A: ReadLMXLockDetect: " << result << "; Pins: " << value;
        if ((value >> 3) & 0x01) emit LMXLockDetect(deviceID, true);  // Lock Detect = Bit 3
        else                     emit LMXLockDetect(deviceID, false);
    }
    else
    {
        qDebug() << "PCA9557A: ReadLMXLockDetect: Error reading pins: " << result;
    }
}




/**********************************************************************************/
/*  PRIVATE Methods                                                               */
/**********************************************************************************/



/*!
 * \brief PCA9557A::configurePins
 * \param p0Dir  Direction of a pin (input / ouput)
 * \param p1Dir  Direction of a pin (input / ouput)
 * \param p2Dir  Direction of a pin (input / ouput)
 * \param p3Dir  Direction of a pin (input / ouput)
 * \param p4Dir  Direction of a pin (input / ouput)
 * \param p5Dir  Direction of a pin (input / ouput)
 * \param p6Dir  Direction of a pin (input / ouput)
 * \param p7Dir  Direction of a pin (input / ouput)
 * \return globals::OK   IO expander configured OK
 * \return [Error code]  Error occurred (comms not connected, etc). IO Expander in unknown state.
 */
int PCA9557A::configurePins(const PinDirection p0Dir,
                           const PinDirection p1Dir,
                           const PinDirection p2Dir,
                           const PinDirection p3Dir,
                           const PinDirection p4Dir,
                           const PinDirection p5Dir,
                           const PinDirection p6Dir,
                           const PinDirection p7Dir)
{
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;
    int result;

    // Set up the Configuration register:
    // 0 = Output; 1 = Input
    regConfig = 0x00;
    if (p0Dir != OUTPUT) regConfig |= 0x01;
    if (p1Dir != OUTPUT) regConfig |= 0x02;
    if (p2Dir != OUTPUT) regConfig |= 0x04;
    if (p3Dir != OUTPUT) regConfig |= 0x08;
    if (p4Dir != OUTPUT) regConfig |= 0x10;
    if (p5Dir != OUTPUT) regConfig |= 0x20;
    if (p6Dir != OUTPUT) regConfig |= 0x40;
    if (p7Dir != OUTPUT) regConfig |= 0x80;

    // Write the Configuration register:
    result = comms->write8(i2cAddress, REG_CONFIG, &regConfig, 1);
    if (result != globals::OK)
    {
        qDebug() << "Error writing to PCA9557A configuration register: " << result;
        return result;
    }
    // Set up the Polarity Inversion register:
    // 0 = Output or normal input; 1 = Inverted input
    regPolarity = 0x00;
    if (p0Dir == INVERTED_INPUT) regPolarity |= 0x01;
    if (p1Dir == INVERTED_INPUT) regPolarity |= 0x02;
    if (p2Dir == INVERTED_INPUT) regPolarity |= 0x04;
    if (p3Dir == INVERTED_INPUT) regPolarity |= 0x08;
    if (p4Dir == INVERTED_INPUT) regPolarity |= 0x10;
    if (p5Dir == INVERTED_INPUT) regPolarity |= 0x20;
    if (p6Dir == INVERTED_INPUT) regPolarity |= 0x40;
    if (p7Dir == INVERTED_INPUT) regPolarity |= 0x80;

    // Write the Polarity Inversion register:
    result = comms->write8(i2cAddress, REG_POLARITY, &regPolarity, 1);
    if (result != globals::OK)
    {
        qDebug() << "Error writing to PCA9557A Polarity Inversion register: " << result;
        return result;
    }

    // OK!
    return globals::OK;
}


/* // DEPRECATED  Not used, Confusing and not needed (for now)
 *
 * \brief Set internal state of ONE pin to a value
 *        NOTE: This only sets the relevant pin value inside the class instance;
 *        it DOESN'T update the actual hardware register (i.e. it doesn't set
 *        the REAL pin). Call writePins method to write the internal state to
 *        the hardware (i.e. update actual pin voltages).
 *        This method can be called for any pin; however, if the pin was set up
 *        as an INPUT (see configurePins), the call will have no useful effect.
 * \param pin     Pin to set
 * \param value   New value: 0 = Low, 1 = High
 *
void PCA9557::setPin(const PinNumber pin, const uint8_t value)
{
    // Update the bit in the output register:
    const uint8_t VALUE_FIXED = (value & 0x01) << pin;   // Clear all bits except LSB, then shift to correct position.
    const uint8_t CLEAR_MASK = ~(0x01 << pin);           // E.g. 11111101 if setting pin P1
    regOutput = (regOutput & CLEAR_MASK) | VALUE_FIXED;  // Clear the existing bit, then set the new value
}
 *
 * \brief Get internal state of ONE pin
 *        NOTE: This only gets the relevant pin value within the class instance;
 *        it DOESN'T read the actual hardware register (i.e. it doesn't get
 *        the REAL pin value). Call readPins method FIRST to update the internal
 *        state to reflect the hardware (i.e. read actual pin voltages).
 *        This method can be called for any pin; however, if the pin was set up
 *        as an OUTPUT (see configurePins), the returned value will be whatever was
 *        last set by calls to setPin and writePins.
 * \param pin Pin to get
 * \return [0 or 1] Value of the pin (from the last time readPins was called)
 *                  Note: The value will be inverted if the pin was configured as INVERTED_INPUT
 *
uint8_t PCA9557::getPin(const PinNumber pin)
{
    // Return the relevant bit from the input register, shifted to LSB:
    return (regInput >> pin) & 0x01;
}
*/


/*!
 * \brief Set ALL pins
 *        Writes the specified value to the output register to set pin levels.
 *        This method can be called for any pin configuration; however, pins set up
 *        as an INPUTs (see configurePins) will not be set.
 * \param value   New value for pins (OUTPUT register) as a UINT 8: Bit 0 = Pin 0; 0 = Low, 1 = High
 * \return globals::OK  Value set OK
 * \return [error code]
 */
int PCA9557A::setPins(const uint8_t value)
{
    regOutput = value;
    return writePins();
}



/*!
 * \brief Get ALL pins
 *        Reads the hardware and gets the pin values from the input register
 *        This method can be called for any pin configuration; however, pins set up
 *        as OUTPUTs (see configurePins) will return whatever was
 *        last set by calls to setPin and writePins.
 * \param value   Pointer to uint8_t; will be set to the input register contents
 *                 Bit 0 = Pin 0; 0 = Low, 1 = High
 *                 Note: The value will be inverted if the pin was configured as INVERTED_INPUT
 * \return globals::OK  Value read OK
 * \return [error code]
 */
int PCA9557A::getPins(uint8_t *value)
{
    int result = readPins();
    *value = regInput;
    return result;
}



/*!
 * \brief Update output pins using a mask
 * This method updates specific pins while preserving other pins
 * at whatever value they were previusly set to. It assumes regOutput
 * reflects the current setting of the output pins, and updates the
 * bits using mask and value, then writes the new value to the output
 * register with setPins.
 * \param mask   Bit mask with '1' in the location of each output pin to be updated
 * \param value  New value for output pins; only bits where mask = 1 will be updated.
 *               Other pins will be unchanged.
 * \return globals::OK  Pins updated OK
 * \return [error code]
 */
int PCA9557A::updatePins(const uint8_t mask, const uint8_t value)
{
    // Update to new settings, preserving other bits we don't want to change:
    uint8_t regNew = (regOutput & (~mask)) | value;  // Mask out the pins to be updated (zeros those pins; preserves other pins); then bitwise OR with new value
    int result = setPins(regNew);
    if (result != globals::OK)
    {
        qDebug() << "PCA9557A: Error setting pins (" << result << ")";
        emit ShowMessage("I/O controller error!");
        emit Result(result, globals::ALL_LANES);
    }
    return result;
}




/*!
 * \brief Write latest values to PCA9557A pins
 *        This method updates the hardware adaptor with the latest set of
 *        pin values set by calls to setPin.
 * \return globals::OK   IO expander output register set OK
 * \return [Error code]  Error occurred (comms not connected, etc).
 */
int PCA9557A::writePins()
{
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;
    // Write the output register:
    int result = comms->write8(i2cAddress, REG_OUPUT, &regOutput, 1);
    if (result != globals::OK)
    {
        qDebug() << "Error writing to PCA9557A output register: " << result;
        return result;
    }
    // OK!
    return globals::OK;
}



/*!
 * \brief Read latest pin values from PCA9557A
 *        This method reads the adaptor input register and updates the
 *        internal buffer with the latest pin values read from the hardware
 *        Pin values can then be obtained with calls to getPin.
 * \return globals::OK   IO expander input register read OK
 * \return [Error code]  Error occurred (comms not connected, etc).
 */
int PCA9557A::readPins()
{
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;
    // Read the input register:
    int result = comms->read8(i2cAddress, REG_INPUT, &regInput, 1);
    if (result != globals::OK)
    {
        qDebug() << "Error reading from PCA9557A input register: " << result;
        return result;
    }
    // OK!
    return globals::OK;
}


// Macro to test the content of 'result' and return with
// error message if not globals::OK
#define RESULT_CHECK(MSG)              \
    if (result != globals::OK)         \
    {                                  \
        qDebug() << MSG << result;     \
        return result;                 \
    }


/*!
 * \brief Test PCA9557A Interface - Used for testing and debugging.
 *        This method sets the "Polarity Inversion" register on the PCA9557A
 *        to two different test values, and reads each value back to check
 *        the operation of the comms interface.
 *        If loopBackTest is true, it then sets up Pin 7 as an output and Pin 6
 *        as an input. It toggles the output level on pin 7 and samples pin 6,
 *        and checks whether the values match. It is assumed that a loopback
 *        from pin 7 to pin 6 will be set up for this test.
 * \return globals::OK   IO expander responded OK; all tests passed.
 * \return [Error code]  Error occurred (comms not connected, etc).
 */
int PCA9557A::test(bool loopBackTest)
{
    if (!comms->portIsOpen()) return globals::NOT_CONNECTED;
    qDebug() << "Starting PCA9557 Interface Tests...";
    int result;

    // Read initial register values:
    qDebug() << "-Reading initial register values:";
    uint8_t regInput = 0;
    uint8_t regOutput = 0;
    uint8_t regPolarity = 0;
    uint8_t regConfig = 0;

    result = comms->read8(i2cAddress, REG_INPUT,    &regInput,    1);
    RESULT_CHECK("Error reading PCA9557A input register: ");
    qDebug() << QString("  INPUT:    [0x%1]").arg(regInput,2,16,QChar('0'));

    result = comms->read8(i2cAddress, REG_OUPUT,    &regOutput,   1);
    RESULT_CHECK("Error reading PCA9557A output register: ");
    qDebug() << QString("  OUTPUT:   [0x%1]").arg(regOutput,2,16,QChar('0'));

    result = comms->read8(i2cAddress, REG_POLARITY, &regPolarity, 1);
    RESULT_CHECK("Error reading PCA9557A polarity inversion register: ");
    qDebug() << QString("  POLARITY: [0x%1]").arg(regPolarity,2,16,QChar('0'));

    result = comms->read8(i2cAddress, REG_CONFIG,   &regConfig,   1);
    RESULT_CHECK("Error reading PCA9557A configuration register: ");
    qDebug() << QString("  CONFIG:   [0x%1]").arg(regConfig,2,16,QChar('0'));

    // OK!
    qDebug() << "-Setting POLARITY INVERSION register to '01010101b' (0x55):";
    uint8_t regPolarityNew = 0x55;
    result = comms->write8(i2cAddress, REG_POLARITY, &regPolarityNew, 1);
    RESULT_CHECK("Error writing PCA9557A polarity inversion register: ");
    qDebug() << "-Reading back POLARITY INVERSION register...";
    result = comms->read8(i2cAddress, REG_POLARITY, &regPolarityNew, 1);
    RESULT_CHECK("Error reading PCA9557A polarity inversion register: ");
    qDebug() << QString("  Value: [0x%1]").arg(regPolarityNew,2,16,QChar('0'));
    if (regPolarityNew == 0x55)
    {
        qDebug() << "  -OK!";
    }
    else
    {
        qDebug() << "  -Value didn't match!";
        return globals::GEN_ERROR;
    }

    // Set the polarity inversion register back to original value:
    qDebug() << "-Restoring POLARITY INVERSION register...";
    result = comms->write8(i2cAddress, REG_POLARITY, &regPolarity, 1);
    RESULT_CHECK("Error writing PCA9557A polarity inversion register: ");
    qDebug() << "-Reading back POLARITY INVERSION register...";
    result = comms->read8(i2cAddress, REG_POLARITY, &regPolarityNew, 1);
    RESULT_CHECK("Error reading PCA9557A polarity inversion register: ");
    qDebug() << QString("  Value: [0x%1]").arg(regPolarity,2,16,QChar('0'));
    if (regPolarityNew == regPolarity)
    {
        qDebug() << "  -OK!";
    }
    else
    {
        qDebug() << "  -Value didn't match!";
        return globals::GEN_ERROR;
    }

    uint8_t tempReg;
    uint8_t tempRegConfig;
    uint8_t tempRegPolarity;

    if (!loopBackTest) goto testFinished;  // Not doing loopback test. Drop out.

    // Loop back test: Set up pin 6 as input, pin 7 as output:
    qDebug() << "-Loopback test: Setting up Pin 6 as input, Pin 7 as output:";
    tempRegConfig = (regConfig & 0x3F) | 0x40;  // Clear upper 2 bits, then set b7 to 0, b6 to 1.
    result = comms->write8(i2cAddress, REG_CONFIG, &tempRegConfig, 1);
    RESULT_CHECK("Error writing PCA9557A configuration register: ");

    // Also need to make sure pin 6 is not inverted:
    tempRegPolarity = regPolarity & 0xBF;  // Clear bit 6.
    result = comms->write8(i2cAddress, REG_POLARITY, &tempRegPolarity, 1);
    RESULT_CHECK("Error writing PCA9557A polarity inversion register: ");

    // Clear Pin 7:
    qDebug() << "-CLEAR Pin 7:";
    tempReg = regOutput & 0x7F;
    result = comms->write8(i2cAddress, REG_OUPUT, &tempReg, 1);
    RESULT_CHECK("Error writing PCA9557A output register: ");
    globals::sleep(500);
    // Read Pin 6:
    qDebug() << "-READ Pin 6:";
    result = comms->read8(i2cAddress, REG_INPUT, &tempReg, 1);
    RESULT_CHECK("Error reading PCA9557A input register: ");
    if ((tempReg & 0x40) == 0x00)
    {
        qDebug() << "  Pin 6 CLEAR! Loopback OK.";
    }
    else
    {
        qDebug() << "  Pin 6 SET! Loopback Error!";
        return globals::GEN_ERROR;
    }
    // Set Pin 7:
    qDebug() << "-SET Pin 7:";
    tempReg = regOutput | 0x80;
    result = comms->write8(i2cAddress, REG_OUPUT, &tempReg, 1);
    RESULT_CHECK("Error writing PCA9557A output register: ");
    globals::sleep(500);
    // Read Pin 6:
    qDebug() << "-READ Pin 6:";
    result = comms->read8(i2cAddress, REG_INPUT, &tempReg, 1);
    RESULT_CHECK("Error reading PCA9557A input register: ");
    if ((tempReg & 0x40) != 0x00)
    {
        qDebug() << "  Pin 6 SET! Loopback OK.";
    }
    else
    {
        qDebug() << "  Pin 6 CLEAR! Loopback Error!";
        return globals::GEN_ERROR;
    }

  testFinished:
    qDebug() << "-Restoring registers...";
    // Set output, config and polarity inversion registers back to original values:
    result = comms->write8(i2cAddress, REG_CONFIG, &regConfig, 1);
    RESULT_CHECK("Error writing PCA9557A configuration register: ");
    result = comms->write8(i2cAddress, REG_POLARITY, &regPolarity, 1);
    RESULT_CHECK("Error writing PCA9557A polarity inversion register: ");
    result = comms->write8(i2cAddress, REG_OUPUT, &regOutput, 1);
    RESULT_CHECK("Error writing PCA9557A output register: ");

    qDebug() << "PCA9557A interface tests finished OK!";
    return globals::OK; // Done!
}

