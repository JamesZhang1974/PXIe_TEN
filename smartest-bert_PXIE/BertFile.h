/*!
 \file   BertFile.h
 \brief  BERT File System Helper Class
 \author J Cole-Baker (For Smartest)
 \date   Jul 2016
*/

#ifndef BERTFILE_H
#define BERTFILE_H

#include <QString>
#include <QStringList>
#include <QFile>

/*!
 \brief BERT File System Helper Class
 This class provides some general methods for using the file system,
 e.g.:
  - Reading directory contents
  - Opening a file
  - Reading file contents
*/
class BertFile
{
public:
    BertFile();
    ~BertFile();

    static int readDirectory(const QString &path, QStringList &fileList);

    static int readFile(const QString &path, const size_t maxLines, QStringList &fileLines);

    static void debug(const QString &msg);

private:
    static QFile debugFile;

};

#endif // BERTFILE_H
