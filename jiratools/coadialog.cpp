/***************************************************************************
** Name         : coadialog.cpp
** Author       : snowcicada
** Date         : 20170331
** Description  : Getting jira tasks by year and quarter.
** GitHub       : https://github.com/snowcicada
** E-mail       : snowcicadas#gmail.com (# -> @)
** This file may be redistributed under the terms of the GNU Public License.
***************************************************************************/

#include "coadialog.h"
#include "ui_coadialog.h"
#include <QtGui>

#define UNKNOWN_STATE "unknown"

COaDialog::COaDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::COaDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::WindowCloseButtonHint);
    setFixedSize(this->width(), this->height());
    setWindowTitle(tr("Jira工具 v1.1"));

    //init
    CCurl::GlobalInit();

    m_ePart = Part_None;

    //ui init
    for (int i = 0; i < 90; i++)
    {
        ui->comboYear->addItem(QString(tr("%1").arg(i + BEGIN_YEAR)));
    }
    int year = QDate::currentDate().year();
    int index = year - BEGIN_YEAR;
    ui->comboYear->setCurrentIndex(index);
}

COaDialog::~COaDialog()
{
    delete ui;
    CCurl::GlobalCleanup();
}

void COaDialog::on_pushButton_clicked()
{
    m_mapTask.clear();
    m_listTask.clear();

    if (ui->lineUser->text().isEmpty())
    {
        QMessageBox::warning(this, tr("提示"), tr("请输入大侠的用户名"));
        return;
    }

    if (ui->linePwd->text().isEmpty())
    {
        QMessageBox::warning(this, tr("提示"), tr("请输入大侠的密码"));
        return;
    }

    if (ui->rdoDesign->isChecked())
    {
        m_ePart = Part_Design;
    }
    else if (ui->rdoProgram->isChecked())
    {
        m_ePart = Part_Program;
    }
    else if (ui->rdoQa->isChecked())
    {
        m_ePart = Part_Qa;
    }

    if (Part_None == m_ePart)
    {
        QMessageBox::warning(this, tr("提示"), tr("请选择部门"));
        return;
    }

    ui->comboQuarter->setEnabled(false);
    ui->comboYear->setEnabled(false);
    ui->pushButton->setEnabled(false);

    if (!login())
    {
        QMessageBox::warning(this, tr("提示"), tr("登录失败"));
        return;
    }

    if (Part_Design == m_ePart || Part_Program == m_ePart)
    {
        if (!spider())
        {
            return;
        }
    }
    else
    {
        if (!spiderQa())
        {
            return;
        }
    }

    filterData(m_listTask);
    if (m_listTask.empty())
    {
        QMessageBox::warning(this, tr("提示"), tr("没有数据"));
        ui->comboQuarter->setEnabled(true);
        ui->comboYear->setEnabled(true);
        ui->pushButton->setEnabled(true);
        return;
    }

    getTaskInfo(m_listTask);

    outputResult(m_listTask);

    ui->comboQuarter->setEnabled(true);
    ui->comboYear->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->progressBar->setValue(0);

    QMessageBox::warning(this, tr("成功"), tr("成功为大侠批量导出%1年%2的任务单").arg(ui->comboYear->currentText()).arg(ui->comboQuarter->currentText()));
}

bool COaDialog::login()
{
    QString strUser = ui->lineUser->text();
    QString strPwd = ui->linePwd->text();
    QString strField = tr("os_username=%1&os_password=%2&os_captcha=").arg(strUser).arg(strPwd);
    QString strHtml;
    bool bRet = m_curl.Post("http://jira.woobest.com/rest/gadget/1.0/login", strField, strHtml);
    if (!bRet)
    {
        return false;
    }

    bRet = m_curl.Get("http://jira.woobest.com/secure/IssueNavigator.jspa?mode=hide&requestId=11559", strHtml);
    if (!bRet || strHtml.indexOf("results-count-total") == -1)
    {
        return false;
    }
    return true;
}

bool COaDialog::spider()
{
    bool bRet = false;
    QString strHtml;

    //first visit my jira
    bRet = m_curl.Get("http://jira.woobest.com/secure/IssueNavigator.jspa?mode=hide&requestId=11559", strHtml);
    if (!bRet)
    {
        return false;
    }

    int nTaskNum = getTaskNum(strHtml);
    if (nTaskNum < 0)
    {
        QMessageBox::warning(this, tr("提示"), tr("没有任务"));
        return false;
    }

    QString strUrl;
    int nPages = (int)(nTaskNum / 50) + 1;
    ui->progressBar->setMaximum(nPages);
    for (int i = 0; i < nPages; i++)
    {
        strUrl = tr("http://jira.woobest.com/secure/IssueNavigator.jspa?pager/start=%1").arg(i * 50);
        bRet = m_curl.Get(strUrl, strHtml);
        if (!bRet)
        {
            return false;
        }

        parseHtml(strHtml);
        ui->progressBar->setValue(i+1);
        QCoreApplication::processEvents();
    }

    return true;
}

bool COaDialog::spiderQa()
{
    bool bRet = false;
    QString strHtml, strFieldInfo;

    //first visit my jira
    bRet = m_curl.Get("http://jira.woobest.com/secure/IssueNavigator!executeAdvanced.jspa", strHtml);
    if (!bRet)
    {
        return false;
    }

    strFieldInfo = "jqlQuery=%E9%AA%8C%E6%94%B6%E8%80%85+%3D+currentUser%28%29&runQuery=true&autocomplete=on";
    bRet = m_curl.Post("http://jira.woobest.com/secure/IssueNavigator!executeAdvanced.jspa", strFieldInfo, strHtml);
    if (!bRet)
    {
        return false;
    }


    int nTaskNum = getTaskNum(strHtml);
    if (nTaskNum < 0)
    {
        QMessageBox::warning(this, tr("提示"), tr("没有任务"));
        return false;
    }

    QString strUrl;
    int nPages = (int)(nTaskNum / 50) + 1;
    ui->progressBar->setMaximum(nPages);
    for (int i = 0; i < nPages; i++)
    {
        strUrl = tr("http://jira.woobest.com/secure/IssueNavigator.jspa?pager/start=%1").arg(i * 50);
        bRet = m_curl.Get(strUrl, strHtml);
        if (!bRet)
        {
            return false;
        }

        parseHtml(strHtml);
        ui->progressBar->setValue(i+1);
        QCoreApplication::processEvents();
    }

    return true;
}

bool COaDialog::parseHtml(const QString& strHtml)
{
    std::vector<QString> vecTaskId;
    std::vector<uint> vecDate;
    parseTaskId(strHtml, vecTaskId);
    parseDate(strHtml, vecDate);
    if (vecTaskId.size() != vecDate.size())
    {
        QMessageBox::warning(this, tr("提示"), tr("解析不匹配"));
        return false;
    }

    int len = vecTaskId.size();
    for (int i = 0; i < len; i++)
    {
        m_mapTask[vecTaskId[i]] = vecDate[i];
//        qDebug() << vecTaskId[i] << QDateTime::fromTime_t(vecDate[i]).toString("yyyy-MM-dd");
    }

//    std::vector<PAIR> vecSort(m_mapTask.begin(), m_mapTask.end());
//    std::sort(vecSort.begin(), vecSort.end(), compareMapValueDesc);
//    for (std::vector<PAIR>::iterator it = vecSort.begin(); it != vecSort.end(); ++it)
//    {
//        qDebug() << (*it).first << (*it).second << QDateTime::fromTime_t((*it).second).toString("yyyy-MM-dd");
//    }

    return true;
}

void COaDialog::parseTaskId(const QString& strHtml, std::vector<QString>& vec)
{
    vec.clear();
    QString strPattern = "data-issuekey=\"(.*)\"";
    QRegExp regExp;
    regExp.setMinimal(true);//非贪婪
    regExp.setPattern(strPattern);
    int pos = 0;
    while ((pos = regExp.indexIn(strHtml, pos)) != -1)
    {
        if (regExp.captureCount() > 0)
        {
            vec.push_back(regExp.cap(1));
        }
        pos += regExp.matchedLength();
    }
}

void COaDialog::parseDate(const QString& strHtml, std::vector<uint>& vec)
{
    vec.clear();
    QString strPattern = "<td.*updated\">.*datetime.*>(.*)</time>";
    QRegExp regExp;
    regExp.setMinimal(true);//非贪婪
    regExp.setPattern(strPattern);
    int pos = 0;
    while ((pos = regExp.indexIn(strHtml, pos)) != -1)
    {
        if (regExp.captureCount() > 0)
        {
            QDateTime dt = QDateTime::fromString(regExp.cap(1), "yyyy/MM/dd");
            vec.push_back(dt.toTime_t());
        }
        pos += regExp.matchedLength();
    }
}

int COaDialog::getTaskNum(const QString& strHtml)
{
    QRegExp regExp;
    regExp.setMinimal(true);//非贪婪
    regExp.setPattern("<strong class=\"results-count-total\">(.*)</strong>");
    regExp.indexIn(strHtml);
    if (regExp.captureCount() > 0)
    {
        QString str = regExp.cap(1);
        return str.toInt();
    }
    return 0;
}

QString COaDialog::getTaskState(const QString& strHtml)
{
    QRegExp regExp;
    regExp.setMinimal(true);//非贪婪
    regExp.setPattern("value resolved.*>(.*)</span>");
    regExp.indexIn(strHtml);
    if (regExp.captureCount() > 0)
    {
        QString str = regExp.cap(1).trimmed();
        return str;
    }
    return QString(UNKNOWN_STATE);
}

QString COaDialog::getTaskTitle(const QString& strHtml)
{
    QRegExp regExp;
    regExp.setMinimal(true);//非贪婪
    regExp.setPattern("summary-val\">(.*)</h1>");
    regExp.indexIn(strHtml);
    if (regExp.captureCount() > 0)
    {
        QString str = regExp.cap(1).trimmed();
//        qDebug() << str;
        return str;
    }
    return "";
}

QString COaDialog::ToUtf8(const QString &str)
{
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    return codec->toUnicode(str.toAscii());
}

void COaDialog::WriteToFile(const QString& strFile, const QString& strText)
{
    QFile file(strFile);
    if (file.open(QFile::ReadWrite | QFile::Truncate))
    {
        file.write(strText.toAscii().data(), strText.size());
        file.close();
    }
}

bool COaDialog::compareMapValueDesc(const PAIR& a, const PAIR& b)
{
    return a.second > b.second;
}

bool COaDialog::compareTaskDesc(const stTaskId& a, const stTaskId& b)
{
    return a.date > b.date;
}

void COaDialog::filterData(std::list<stTaskId>& listTask)
{
    uint begin = 0, end = 0;
    getBeginEndTime(begin, end);

    listTask.clear();
    for (std::map<QString, uint>::iterator it = m_mapTask.begin(); it != m_mapTask.end(); ++it)
    {
        if (it->second >= begin && it->second <= end)
        {
            listTask.push_back(stTaskId(it->first, it->second));
        }
    }
}

void COaDialog::getTaskInfo(std::list<stTaskId>& listTask)
{
    QString strUrl, strHtml;
    ui->progressBar->setMaximum(listTask.size());
    int count = 0;
    for (std::list<stTaskId>::iterator it = listTask.begin(); it != listTask.end(); ++it)
    {
        ui->progressBar->setValue(++count);
        stTaskId& task = *it;
        strUrl = tr("http://jira.woobest.com/browse/%1").arg(task.id);
        bool bRet = m_curl.Get(strUrl, strHtml);
//        WriteToFile(task.id, strHtml);
        if (bRet)
        {
            task.state = getTaskState(strHtml);
            task.title = getTaskTitle(strHtml);
        }
        else
        {
            task.state = UNKNOWN_STATE;
            continue;
        }
        QCoreApplication::processEvents();
    }
}

void COaDialog::outputResult(std::list<stTaskId>& listTask)
{
    if (listTask.empty())
    {
        return;
    }
    //std::sort(listTask.begin(), listTask.end(), compareTaskDesc);
    listTask.sort();

    QFile file("task.txt");
    if (!file.open(QFile::Truncate | QFile::WriteOnly))
    {
        return;
    }

    QString strLine;
    for (std::list<stTaskId>::iterator it = listTask.begin(); it != listTask.end(); ++it)
    {
        stTaskId& task = *it;
        if (task.state == tr("已修复"))
        {
            strLine.append(tr("%1 %2 %3 \r\n").arg(QDateTime::fromTime_t(task.date).toString("yyyy-MM-dd")).arg(task.id).arg(task.title));
        }
    }
    strLine.append("--------------------------------------------------------\r\n");
    QByteArray ba = strLine.toLocal8Bit();
    file.write(ba, ba.size());
    file.close();

    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo("task.txt").absoluteFilePath()));
}

void COaDialog::getBeginEndTime(uint& begin, uint& end)
{
    static const QString s_begin[] = {"/01/01", "/04/01", "/07/01", "/10/01"};
    static const QString s_end[] = {"/03/31", "/06/30", "/09/30", "/12/31"};
    int year = ui->comboYear->currentText().toInt();
    int index = ui->comboQuarter->currentIndex();
    QString strBegin = tr("%1%2").arg(year).arg(s_begin[index]);
    QString strEnd = tr("%1%2").arg(year).arg(s_end[index]);
    QDateTime dtBegin = QDateTime::fromString(strBegin, "yyyy/MM/dd");
    QDateTime dtEnd = QDateTime::fromString(strEnd, "yyyy/MM/dd");
    begin = dtBegin.toTime_t();
    end = dtEnd.toTime_t();
}
