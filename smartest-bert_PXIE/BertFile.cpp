/*!
 \file   BertFile.cpp
 \brief  BERT File System Helper Class Implementation
 \author J Cole-Baker (For Smartest)
 \date   Jul 2016
*/

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>

#include "globals.h"
#include "BertFile.h"

QFile BertFile::debugFile;


BertFile::BertFile()
{}

BertFile::~BertFile()
{
    if (debugFile.isOpen()) debugFile.close();
}

/*!
 \brief Read the contents of a directory

 \param path
 \param errorCode  Indicates whether there was an error reading the directory listing.
              globals::OK                   Directory found and file list read
              globals::DIRECTORY_NOT_FOUND  Directory didn't exist

 \return List of files in the directory. May be empty. If empty, check errorCode
         for possible error.
*/
int BertFile::readDirectory(const QString &path, QStringList &fileList)
{
    // Get executable directory:
    if (!QDir().exists(path)) return globals::DIRECTORY_NOT_FOUND;

    // qDebug() << "Checking for files in " << path << endl;

    QDir dir = QDir(path);
    fileList << dir.entryList(QDir::Files);
    return globals::OK;
}


/*!
 \brief Read the contents of a file, and return a list of lines

 \param fileName
 \param maxLines
 \param fileLines
 \return
*/
int BertFile::readFile(const QString &fileName, const size_t maxLines, QStringList &fileLines)
{
    QFile thisFile;
    size_t lineCount = 0;

    // qDebug() << "Opening file: " << fileName << endl;
    thisFile.setFileName(fileName);
    int result = thisFile.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!result) return globals::FILE_ERROR;

    while ( !thisFile.atEnd() && (lineCount < maxLines) )
    {
        QByteArray fileLine = thisFile.readLine();
        fileLines << QString(fileLine);
        lineCount++;
    }
    thisFile.close();
    return globals::OK;
}


/*!
 \brief BertFile::debug
 \param msg
*/
void BertFile::debug(const QString &msg)
{
    if (!debugFile.isOpen() && globals::getAppPath().length() > 0)
    {
        QString nowLong = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString nowShort = QDateTime::currentDateTime().toString("yyyyMMddThhmmss");
        QString fileName = QString("%1\\debug_%2.txt").arg(globals::getAppPath()).arg(nowShort);

        qDebug() << "Opening debug file: " << fileName;
        debugFile.setFileName(fileName);
        int result = debugFile.open(QIODevice::ReadWrite | QIODevice::Text);
        if (!result)
        {
            qDebug() << "ERROR opening debug file! (" << result << ")";
        }
        else
        {
            debugFile.write("------------------------------------------------------------\n");
            debugFile.write(QString("DEBUG %1\n").arg(nowLong).toLatin1().data());
            debugFile.write("------------------------------------------------------------\n");
        }
    }
    if (debugFile.isOpen())
    {
        debugFile.write(msg.toLatin1().data());
        debugFile.write("\n");
    }

}


