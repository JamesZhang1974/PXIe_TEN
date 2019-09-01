#include "mainwindow.h"
#include <QApplication>

using namespace std;

// DEBUG Options: Define these to add debug output for various things
// They can also be defined by adding as parameters to the build,
// E.g. DEFINES+="..."

// #define BERT_CONSOLE_DEBUG         // Opens a console window and shows debug output there. Use when debugging without QT IDE
// #define BERT_MACRO_DEBUG           // Output details of macro read / write operations to the GT1724 chip
// #define BERT_REGISTER_DEBUG        // Output details of register read / write operations to the GT1724 chip
// #define BERT_USBISS_DEBUG          // Output details of low-level read / write operations to the USB-I2C adaptor
// #define BERT_DEBUG_DIV_RATIOS      // Add additional triger out divide ratios for debugging use
// #define BERT_ED_DEBUG              // Show extra debug info for the ED system



#ifdef BERT_CONSOLE_DEBUG
////// CONSOLE DEBUGGING: ///////////////////////////////////////////////////
#include <qapplication.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <io.h>
#include <Fcntl.h>
#include <iostream>
#include "BertFile.h"

/// This section sets up up debug output to a console window for
/// debugging on systems which don't have the QT editor set up.
/// PLATFORM SPECIFIC - WINDOWS ONLY!
/// Note that a windows application normally doesn't have any STDOUT
/// even when launched from the terminal. setupConsoleDebug opens a
/// new console for this application, and redirects STDOUT so that
/// messages printed with cout will go to the new console.
/// "debugOutput" is a debug message handler for QT, which replaces
/// the default debug message handling and prints messages to
/// STDOUT with "cout".

bool setupConsoleDebug()
{
    cout<<"Setting up debug console...\n";
    AllocConsole();
    cout<<"Redirecting STDOUT to debug console...\n";
    int m_nCRTOut = _open_osfhandle( (long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT );
    if( -1 == m_nCRTOut )
    {
        return false;
    }
    FILE *m_fpCRTOut = _fdopen( m_nCRTOut, "w" );
    if( !m_fpCRTOut )
    {
        return false;
    }
    *stdout = *m_fpCRTOut;
    // Need to clear cout, otherwise previous messages may interfere with the redirection.
    std::cout.clear();
    cout<<"Debug console ready.\n";
    return true;
}

void debugOutput(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        // fprintf(stdout, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        cout << localMsg.constData() << "\n";
        break;
    case QtWarningMsg:
        //fprintf(stdout, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        cout << "WARNING: " << localMsg.constData() << "\n";
        break;
    case QtCriticalMsg:
        //fprintf(stdout, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        cout << "CRITICAL: " << localMsg.constData() << "\n";
        break;
    case QtFatalMsg:
        //fprintf(stdout, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        cout << "FATAL: " << localMsg.constData() << "\n";
        abort();
    }
  #ifdef BERT_FILE_DEBUG
    // Also debug to file: See BertFile class.
    BertFile::debug(msg);
  #endif
}
/////////////////////////////////////////////////////////////////////////
#endif



int main(int argc, char *argv[])
{
#ifdef BERT_CONSOLE_DEBUG
    ////// DEBUGGING: Send debug to console: ////////////////////////////////
    setupConsoleDebug();                 // Set up a console to show debug output
    qInstallMessageHandler(debugOutput); // Install debug callback
    /////////////////////////////////////////////////////////////////////////
#endif

    QApplication a(argc, argv);

    qRegisterMetaType<QVector<double> >("QVector<double>");
    qRegisterMetaType<QString>("QString");
    qRegisterMetaType<QList<LMXFrequencyProfile> >("QList<LMXFrequencyProfile>");

    BertWindow *w = new BertWindow(NULL);
    w->show();
    w->enablePageChanges();

    return a.exec();

}
