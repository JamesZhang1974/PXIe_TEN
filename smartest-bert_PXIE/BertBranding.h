/*!
 \file   BertBranding.h
 \brief  Constants to control branding and style of UI
 \author J Cole-Baker (For Smartest)
 \date   Mar 2019
*/

#ifndef BERTBRANDING_H
#define BERTBRANDING_H

#include <QString>
#include <QSize>

class BertBranding
{
public:

    static const QString BRAND;        // Smartest / Coherent / CEYear
    static const QString APP_TITLE;    // Appears in title bar of window

    // Tweaks to control appearance and branding of app:
    // See branding.cpp for implementation
    static const bool    USE_CHANNEL_BG_COLORS;
    static const QString LOGO_FILE_SMALL;
    static const QSize   LOGO_SIZE_SMALL;
    static const QString LOGO_FILE_LARGE;
    static const QSize   LOGO_SIZE_LARGE;
    static const QString ABOUT_BLURB;

    // Style sheets
    static const QString BG_STYLESHEET;
    static const QString MAIN_TAB_STYLE;
    static const QString UI_STYLESHEET;

    // Layout:
    static const int TAB_WIDTH_MIN = 1000;
    static const int TAB_HEIGHT_MIN = 550;

};

#endif // BERTBRANDING_H
