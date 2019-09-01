#ifndef BERTCOMPONENT_H
#define BERTCOMPONENT_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "globals.h"

class BertComponent : public QObject
{
    Q_OBJECT
public:
    BertComponent();
    ~BertComponent();

   // These are general signals, not specific to a hardware component

#define BERT_COMPONENT_SIGNALS \
    void Result(int result, int lane);                                               \
    void ListPopulate(QString name, int lane, QStringList items, int defaultIndex);  \
    void ListSelect(QString name, int lane, int index);                              \
    void UpdateBoolean(QString name, int lane, bool value);                          \
    void SetPGLedStatus(int lane, bool laneOn);                                      \
    void UpdateString(QString name, int lane, QString value);                        \
    void ShowMessage(QString message, bool append = false);


#define BERT_COMPONENT_CONNECT_SIGNALS(CLIENT, COMPONENT) \
    connect(COMPONENT, SIGNAL(Result(int, int)),                             CLIENT, SLOT(Result(int, int)));                             \
    connect(COMPONENT, SIGNAL(ListPopulate(QString, int, QStringList, int)), CLIENT, SLOT(ListPopulate(QString, int, QStringList, int))); \
    connect(COMPONENT, SIGNAL(ListSelect(QString, int, int)),                CLIENT, SLOT(ListSelect(QString, int, int)));                \
    connect(COMPONENT, SIGNAL(UpdateBoolean(QString, int, bool)),            CLIENT, SLOT(UpdateBoolean(QString, int, bool)));            \
    connect(COMPONENT, SIGNAL(SetPGLedStatus(int, bool)),                    CLIENT, SLOT(SetPGLedStatus(int, bool)));                    \
    connect(COMPONENT, SIGNAL(UpdateString(QString, int, QString)),          CLIENT, SLOT(UpdateString(QString, int, QString)));          \
    connect(COMPONENT, SIGNAL(ShowMessage(QString, bool)),                   CLIENT, SLOT(ShowMessage(QString, bool)));


signals:
    BERT_COMPONENT_SIGNALS

public slots:


};

#endif // BERTCOMPONENT_H
