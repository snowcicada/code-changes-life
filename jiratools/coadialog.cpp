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
#define APP_VERSION "Jira工具 v1.5"
#define ZIP_FILENAME tr("Jira工具v1.5.zip")
#define MY_TASK_URL "http://jira.woobest.com/secure/IssueNavigator.jspa?mode=hide&requestId=11559"

COaDialog::COaDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::COaDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::WindowCloseButtonHint);
    setFixedSize(this->width(), this->height());
    setWindowTitle(tr(APP_VERSION));

    //init
    CCurl::GlobalInit();

    initUI();

    //配置
    readSettings();
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

    if (ui->lineUser->text().isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请输入用户名"));
        return;
    }

    if (ui->linePwd->text().isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请输入密码"));
        return;
    }

//    if (ui->comboDepartment->currentIndex() == 0) {
//        QMessageBox::warning(this, tr("提示"), tr("请选择部门"));
//        return;
//    }

    if (ui->chkQa->isChecked() && ui->comboQaDepartment->currentIndex() == 0) {
        QMessageBox::warning(this, tr("提示"), tr("QA请选择部门"));
        return;
    }

    //保存配置
    writeSettings();

    enableCtrl(false);

    if (!login())
    {
        QMessageBox::warning(this, tr("提示"), tr("登录失败"));
        return;
    }

    if (!ui->chkQa->isChecked())
    {
        if (!spider())
        {
            enableCtrl(true);
            return;
        }
    }
    else
    {
        if (!spiderQa())
        {
            enableCtrl(true);
            return;
        }
    }

    QString strDepartment = ui->comboDepartment->currentText();
    QString strHead = m_mapDepartment[strDepartment];

    filterData(strHead, m_listTask);
    if (m_listTask.empty())
    {
        QMessageBox::warning(this, tr("提示"), tr("没有数据"));
        enableCtrl(true);
        return;
    }

    getTaskInfo(m_listTask);

    outputResult(m_listTask);

    enableCtrl(true);
    ui->progressBar->setValue(0);
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

    bRet = m_curl.Get(MY_TASK_URL, strHtml);
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
    bRet = m_curl.Get(MY_TASK_URL, strHtml);
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

    QString strQa = m_mapQaDepartment[ui->comboQaDepartment->currentText()];
    strFieldInfo = tr("jqlQuery=%1currentUser%28%29&runQuery=true&autocomplete=on").arg(strQa);
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

void COaDialog::filterData(const QString &strHead, std::list<stTaskId>& listTask)
{
    uint begin = 0, end = 0;
    getBeginEndTime(begin, end);

    listTask.clear();
    for (std::map<QString, uint>::iterator it = m_mapTask.begin(); it != m_mapTask.end(); ++it)
    {
        if (it->second >= begin && it->second <= end && (tr("所有") == strHead || it->first.startsWith(strHead)))
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

    QString strFileName;
    if (ui->chkQa->isChecked()) {
        strFileName = tr("%1年%2%3QA任务单(%4).txt").
                arg(ui->comboYear->currentText()).
                arg(ui->comboQuarter->currentText()).
                arg(ui->comboDepartment->currentText()).
                arg(ui->lineUser->text());
    } else {
        strFileName = tr("%1年%2%3任务单(%4).txt").
                arg(ui->comboYear->currentText()).
                arg(ui->comboQuarter->currentText()).
                arg(ui->comboDepartment->currentText()).
                arg(ui->lineUser->text());
    }

    QFile file(strFileName);
    if (!file.open(QFile::Truncate | QFile::WriteOnly))
    {
        return;
    }

    QString strLine, strTmp;
    bool bHasDate = ui->chkHasDate->isChecked();
    for (std::list<stTaskId>::iterator it = listTask.begin(); it != listTask.end(); ++it)
    {
        stTaskId& task = *it;
        if (task.state == tr("已修复"))
        {
            if (bHasDate) {
                strTmp = tr("%1 %2 %3").arg(QDateTime::fromTime_t(task.date).toString("yyyy-MM-dd")).arg(task.id).arg(task.title);
                strTmp = QTextDocumentFragment::fromHtml(strTmp).toPlainText() + " \r\n";
                strLine.append(strTmp);
//                strLine.append(tr("%1 %2 %3 \r\n").arg(QDateTime::fromTime_t(task.date).toString("yyyy-MM-dd")).arg(task.id).arg(task.title));
            } else {
                strTmp = tr("%1 %2 \r\n").arg(task.id).arg(task.title);
                strTmp = QTextDocumentFragment::fromHtml(strTmp).toPlainText() + " \r\n";
                strLine.append(strTmp);
//                strLine.append(tr("%1 %2 \r\n").arg(task.id).arg(task.title));
            }
        }
    }
    strLine.append("--------------------------------------------------------\r\n");
    QByteArray ba = strLine.toLocal8Bit();
    file.write(ba, ba.size());
    file.close();

    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(strFileName).absoluteFilePath()));
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

int COaDialog::getCurrentQuarter()
{
    static const QString s_begin[] = {"/01/01", "/04/01", "/07/01", "/10/01"};
    static const QString s_end[] = {"/03/31", "/06/30", "/09/30", "/12/31"};

    int year = QDateTime::currentDateTime().date().year();
    QString strCurrentDate = QDateTime::currentDateTime().toString("yyyy/MM/dd");
    for (int i = 0; i < MAX_QUARTER; i++) {
        QString strBegin = tr("%1%2").arg(year).arg(s_begin[i]);
        QString strEnd = tr("%1%2").arg(year).arg(s_end[i]);
        if (strCurrentDate >= strBegin && strCurrentDate <= strEnd) {
            return i;
        }
    }
}

QString COaDialog::getQuarterTip()
{
    static const QString s_begin[] = {"01/01", "04/01", "07/01", "10/01"};
    static const QString s_end[] = {"03/31", "06/30", "09/30", "12/31"};

    QString strText;
    for (int i = 0; i < MAX_QUARTER; i++) {
        strText.append(tr("第%1季度 %2 - %3").arg(i + 1).arg(s_begin[i]).arg(s_end[i]));
        if (i != MAX_QUARTER - 1) {
            strText.append("\r\n");
        }
    }
    return strText;
}

void COaDialog::initUI()
{
    //ui init
    for (int i = 0; i < 90; i++)
    {
        ui->comboYear->addItem(QString(tr("%1").arg(i + BEGIN_YEAR)));
    }
    int year = QDate::currentDate().year();
    int index = year - BEGIN_YEAR;
    ui->comboYear->setCurrentIndex(index);

    ui->comboQuarter->setCurrentIndex(getCurrentQuarter());

    QString strQuarter = getQuarterTip();
    ui->labelQuarter->setToolTip(strQuarter);
    ui->comboQuarter->setToolTip(strQuarter);

    updateUI();

    //部门名称
    m_mapDepartment[tr("P18")] = "NEX";
    m_mapDepartment[tr("P18运营组")] = "MXOPERATIONS";
    m_mapDepartment[tr("P20")] = "DF";
    m_mapDepartment[tr("P20运营组")] = "DFOPERATINS";
    m_mapDepartment[tr("P22运营组")] = "DY";
    m_mapDepartment[tr("P22（道友请留步）")] = "PRO";
    m_mapDepartment[tr("P23")] = "MOW";
    m_mapDepartment[tr("P23运营组")] = "MWO";
    m_mapDepartment[tr("P25运营组")] = "JJ";
    m_mapDepartment[tr("P26")] = "JQB";
    m_mapDepartment[tr("P26运营组")] = "PDJQB";
    m_mapDepartment[tr("P27")] = "NSY";
    m_mapDepartment[tr("P28运营组")] = "JZMY";
    m_mapDepartment[tr("T08")] = "BBC";
    m_mapDepartment[tr("产品部")] = "PRODUCT";
    m_mapDepartment[tr("人力资源部")] = "HR";
    m_mapDepartment[tr("原味三国")] = "YWSG";
    m_mapDepartment[tr("商务部")] = "BUSINESS";
    m_mapDepartment[tr("客服部")] = "SERVICE";
    m_mapDepartment[tr("市场部")] = "MARKETING";
    m_mapDepartment[tr("平台开发部")] = "PS";
    m_mapDepartment[tr("技术部")] = "TECH";
    m_mapDepartment[tr("美术部")] = "ART";
    m_mapDepartment[tr("行政部")] = "ADMINISTRATION";
    m_mapDepartment[tr("财务部")] = "FINANCIAL";

    ui->comboDepartment->addItem(tr("所有"));
    for (auto it : m_mapDepartment) {
        ui->comboDepartment->addItem(it.first);
    }
    ui->comboDepartment->setCurrentIndex(0);

    //QA部门名称,URL编码，为了方便，网上手动转换的
    m_mapQaDepartment[tr("P18")] = "%E9%AA%8C%E6%94%B6%E8%80%85+%3D+";//验收者
    m_mapQaDepartment[tr("P22")] = "%e9%aa%8c%e6%94%b6%e4%ba%ba+%3d+";//验收人
    m_mapQaDepartment[tr("P23")] = "%E9%AA%8C%E6%94%B6%E8%80%85+%3D+";//验收者
    m_mapQaDepartment[tr("P26")] = "%E9%AA%8C%E6%94%B6%E8%80%85+%3D+";//验收者
    m_mapQaDepartment[tr("P27")] = "%e9%aa%8c%e6%94%b6%e5%91%98+%3d+";//验收员
    m_mapQaDepartment[tr("原味三国")] = "%E9%AA%8C%E6%94%B6%E8%80%85+%3D+";//验收者

    ui->comboQaDepartment->addItem(tr("无"));
    for (auto it : m_mapQaDepartment) {
        ui->comboQaDepartment->addItem(it.first);
    }
    ui->comboQaDepartment->setCurrentIndex(0);
}

void COaDialog::updateUI()
{
    ui->comboQaDepartment->setVisible(ui->chkQa->isChecked());
}

void COaDialog::enableCtrl(bool bEnabled)
{
    ui->lineUser->setEnabled(bEnabled);
    ui->linePwd->setEnabled(bEnabled);
    ui->comboQuarter->setEnabled(bEnabled);
    ui->comboYear->setEnabled(bEnabled);
    ui->comboDepartment->setEnabled(bEnabled);
    ui->comboQaDepartment->setEnabled(bEnabled);
    ui->chkQa->setEnabled(bEnabled);
    ui->pushButton->setEnabled(bEnabled);
    ui->chkHasDate->setEnabled(bEnabled);
    ui->btnUpdate->setEnabled(bEnabled);
    ui->btnMyTask->setEnabled(bEnabled);
    ui->btnSuggest->setEnabled(bEnabled);
}

void COaDialog::readSettings()
{
    QString strIniFileName = qApp->applicationDirPath() + "/jiratools.ini";
    QSettings settings(strIniFileName, QSettings::IniFormat);
    ui->lineUser->setText(settings.value("jiratools/user").toString());
    QString strPwdEncode = settings.value("jiratools/pwd").toString();
    QString strPwdUncode = QByteArray::fromHex(QByteArray::fromBase64(strPwdEncode.toLatin1()));
    ui->linePwd->setText(strPwdUncode);
    if (settings.contains("jiratools/department")) {
        ui->comboDepartment->setCurrentIndex(settings.value("jiratools/department").toInt());
    }
    if (settings.contains("jiratools/qa")) {
        ui->chkQa->setChecked(settings.value("jiratools/qa").toBool());
    }
    if (settings.contains("jiratools/qadepartment")) {
        ui->comboQaDepartment->setCurrentIndex(settings.value("jiratools/qadepartment").toInt());
    }
    if (settings.contains("jiratools/hasdate")) {
        ui->chkHasDate->setChecked(settings.value("jiratools/hasdate").toBool());
    }
}

void COaDialog::writeSettings()
{
    QString strIniFileName = qApp->applicationDirPath() + "/jiratools.ini";
    QString strPwd = ui->linePwd->text().toLatin1().toHex().toBase64().data();
    QSettings settings(strIniFileName, QSettings::IniFormat);
    settings.setValue("jiratools/user", ui->lineUser->text());
    settings.setValue("jiratools/pwd", strPwd);
    settings.setValue("jiratools/department", ui->comboDepartment->currentIndex());
    settings.setValue("jiratools/qa", ui->chkQa->isChecked());
    settings.setValue("jiratools/qadepartment", ui->comboQaDepartment->currentIndex());
    settings.setValue("jiratools/hasdate", ui->chkHasDate->isChecked());
}

void COaDialog::on_chkQa_toggled(bool checked)
{
    updateUI();
}

void COaDialog::on_btnUpdate_clicked()
{
    bool bHasUpdate = false;
    QDir dir("\\\\share\\everyone\\guozs\\jiratools\\");
    QStringList strFilterList;
    strFilterList << "*.zip";
    QStringList strFileNameList = dir.entryList(strFilterList);
    for (int i = 0; i < strFileNameList.size(); i++) {
        if (strFileNameList.at(i) > ZIP_FILENAME) {
            bHasUpdate = true;
            break;
        }
    }

    if (bHasUpdate) {
        QMessageBox::information(this, tr("提示"), tr("检测到更新，请前往下载！"));
        QDesktopServices::openUrl(QUrl::fromLocalFile("file://///share/everyone/guozs/jiratools/"));
    } else {
        QMessageBox::information(this, tr("提示"), tr("已经是最新版本！"));
    }

    return;
}

void COaDialog::on_btnMyTask_clicked()
{
    QDesktopServices::openUrl(QUrl(MY_TASK_URL));
}

void COaDialog::on_btnSuggest_clicked()
{
    QUrl url((tr("mailto:guozs@woobest.com?subject=Make a suggestion by %1&body=Please make a suggestion!").arg(ui->lineUser->text())));
    QDesktopServices::openUrl(url);
}
