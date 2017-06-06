#include <QApplication>
#include <QTextCodec>
#include "coadialog.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));//QTextCodec::codecForLocale()
    //QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForTr(QTextCodec::codecForLocale());

    COaDialog w;
    w.show();
    
    return a.exec();
}
