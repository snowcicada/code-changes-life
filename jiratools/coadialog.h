/***************************************************************************
** Name         : coadialog.h
** Author       : snowcicada
** Date         : 20170331
** Description  : Getting jira tasks by year and quarter.
** GitHub       : https://github.com/snowcicada
** E-mail       : snowcicadas#gmail.com (# -> @)
** This file may be redistributed under the terms of the GNU Public License.
***************************************************************************/

#ifndef COADIALOG_H
#define COADIALOG_H

#include <QDialog>
#include <vector>
#include <map>
#include <list>
#include "ccurl.h"

struct stTaskId
{
    QString id;
    uint date;
    QString state;
    QString title;

    stTaskId()
    {
        state = "";
        id = "";
        date = 0;
    }

    stTaskId(QString strId, uint nDate)
    {
        id = strId;
        date = nDate;
    }

    //desc
    bool operator <(const stTaskId& b) const
    {
        return date < b.date;
    }
};

typedef std::pair<QString, uint> PAIR;

namespace Ui {
class COaDialog;
}

class COaDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit COaDialog(QWidget *parent = 0);
    ~COaDialog();
    
private slots:
    void on_pushButton_clicked();

    void on_chkQa_toggled(bool checked);

private:
    bool login();
    bool spider();
    bool spiderQa();
    bool parseHtml(const QString& strHtml);
    void parseTaskId(const QString& strHtml, std::vector<QString>& vec);
    void parseDate(const QString& strHtml, std::vector<uint> &vec);
    int getTaskNum(const QString& strHtml);
    QString getTaskState(const QString& strHtml);
    QString getTaskTitle(const QString& strHtml);
    QString ToUtf8(const QString &str);
    void WriteToFile(const QString& strFile, const QString& strText);
    void filterData(const QString& strHead, std::list<stTaskId>& listTask);
    void getTaskInfo(std::list<stTaskId>& listTask);
    void outputResult(std::list<stTaskId>& listTask);
    void getBeginEndTime(uint& begin, uint& end);
    void initUI();
    void updateUI();
    void enableCtrl(bool bEnabled);
    void readSettings();
    void writeSettings();

    static bool compareMapValueDesc(const PAIR &a, const PAIR &b);
    static bool compareTaskDesc(const stTaskId& a, const stTaskId& b);

private:
    static const int BEGIN_YEAR = 2010;

private:
    Ui::COaDialog *ui;

    CCurl m_curl;
    std::map<QString, uint> m_mapTask;
    std::list<stTaskId> m_listTask;
    std::map<QString, QString> m_mapDepartment;
    std::map<QString, QString> m_mapQaDepartment;
};

#endif // COADIALOG_H
