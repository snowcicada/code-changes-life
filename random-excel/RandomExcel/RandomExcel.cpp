#include <afx.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <algorithm>
#include "CSpreadSheet.h"

using namespace std;

//存储所有行
std::vector<CStringArray*> g_allRows;

//存储所有参与随机的格子
std::vector<CString> g_allRandomCells;

//存储标题
CStringArray g_title;

//释放所有数据
void FreeRow(std::vector<CStringArray*>& vecRow)
{
    for (std::vector<CStringArray*>::iterator it = vecRow.begin(); it != vecRow.end(); ++it)
    {
        if (*it)
        {
            (*it)->RemoveAll();
            delete *it;
            *it = NULL;
        }
    }
    vecRow.clear();
}

//读取所有行数据
void ReadAllRows(CSpreadSheet& excel, std::vector<CStringArray*>& vecRow, CStringArray& arrTitle)
{
    FreeRow(vecRow);

    //读取标题
    arrTitle.RemoveAll();
    excel.ReadRow(arrTitle, 1);

    //第2行开始才是数据
    int nRow = excel.GetTotalRows();
    for (int i = 2; i < nRow; ++i)
    {
        CStringArray* pArr = new CStringArray;
        excel.ReadRow(*pArr, i);
        vecRow.push_back(pArr);
    }
}

//读取需要随机的列数据
void ReadRandomColumns(CSpreadSheet& excel, const std::vector<CString>& vecTitle, std::vector<CString>& vecCell)
{
    if (vecTitle.empty())
    {
        cout << "【错误】请配置需要参与随机的列标题。" << endl;
        return;
    }

    vecCell.clear();
    CStringArray arr;
    for (int i = 0; i < vecTitle.size(); ++i)
    {
        arr.RemoveAll();
        excel.ReadColumn(arr, vecTitle[i]);
        //第一行是标题
        for (int j = 1; j < arr.GetSize(); ++j)
        {
            vecCell.push_back(arr[j]);
        }
    }

    //随机
    if (!vecCell.empty())
    {
        std::random_shuffle(vecCell.begin(), vecCell.end());
    }
}

//标题转换成列索引
std::vector<int> Title2Column(CStringArray& arrAllTitle, const std::vector<CString>& vecTitle)
{
    std::vector<int> vec;
    for (int i = 0; i < vecTitle.size(); ++i)
    {
        for (int j = 0; j < arrAllTitle.GetSize(); ++j)
        {
            if (arrAllTitle[j] == vecTitle[i])
            {
                vec.push_back(j);
            }
        }
    }
    return vec;
}

//合并数据
void MergeExcelData(std::vector<CStringArray*>& vecRow, const std::vector<int>& vecCol, std::vector<CString>& vecCell)
{
    DWORD nColNum = vecCol.size();
    DWORD nRow = vecRow.size();
    DWORD nCellRow = vecCell.size() / nColNum;
    if (nCellRow != nCellRow)
    {
        printf("【错误】合并数据失败,行数不匹配:总行数=%u 随机区域行数=%u\n", nRow, nCellRow);
        return;
    }

    //将列放到数组提高性能
    int* aCol = new int[nColNum];
    if (NULL == aCol)
    {
        return;
    }
    for (int i = 0; i < nColNum; ++i)
    {
        aCol[i] = vecCol[i];
    }

    //合并数据
    DWORD nCount = 0;
    DWORD nCol = 0;
    for (std::vector<CStringArray*>::iterator it = vecRow.begin(); it != vecRow.end(); ++it)
    {
        if (NULL == *it)
        {
            printf("数据异常\n");
            delete[] aCol;
            return;
        }
        CStringArray& rRow = *(*it);
        for (DWORD i = 0; i < nColNum; ++i)
        {
            nCol = aCol[i];
            if (vecCell.empty())
            {
                printf("【错误】数据不匹配,第%u行失败\n", nCount);
                delete[] aCol;
                return;
            }
            else
            {
                rRow.SetAt(nCol, vecCell.back());
                vecCell.pop_back();
            }
        }
        ++nCount;
    }

    delete[] aCol;
}

//探测一个未使用的文件名
CString GetNotExistsFileName(const CString& strLeftName, const CString& strRightName)
{
    CString strFileName = strLeftName + "_bak." + strRightName;
    int nCount = 0;
    while (nCount < 1000)
    {
        strFileName.Format("%s_%04d.%s", strLeftName.GetString(), ++nCount, strRightName.GetString());
        if (GetFileAttributes(strFileName) == INVALID_FILE_ATTRIBUTES)
        {
            break;
        }
    }
    return strFileName;
}

//写入新excel
bool SaveNewExcel(const CString& strFileName, const CString& strSheet, CStringArray& arrAllTitle, std::vector<CStringArray*>& vecRow, CString& strDst)
{
    int nIndex = strFileName.ReverseFind('.');
    if (-1 == nIndex)
    {
        printf("【错误】生成失败,文件名不合法\n");
        return false;
    }
    CString strLeftName = strFileName.Left(nIndex);
    CString strRightName = strFileName.Right(strFileName.GetLength() - nIndex - 1);
    CString strDstName = GetNotExistsFileName(strLeftName, strRightName);

    CString strSheetName = "Sheet1"; //默认
    if (!strSheet.IsEmpty())
    {
        strSheetName = strSheet;
    }
    CSpreadSheet excel(strDstName, strSheetName, "false");
    excel.BeginTransaction();
    excel.AddHeaders(arrAllTitle);
    for (std::vector<CStringArray*>::iterator it = vecRow.begin(); it != vecRow.end(); ++it)
    {
        if (*it)
        {
            excel.AddRow(*(*it));
        }
        else
        {
            printf("【错误】写入出错,有数据为空\n");
            break;
        }
    }
    strDst = strDstName;
    return excel.Commit();
}

#define INI_FILE "excel.ini"

int main()
{
    srand(time(NULL)); //下种子,否则每次随机结果一样

    cout << "使用方法:\n"
        " 1.首次运行会产生一个配置文件excel.ini\n"
        " 2.在excel.ini中配置filename文件名称\n"
        " 3.在excel.ini中配置columns随机的列标题,以#号分隔\n"
        " 4.在excel.ini中配置sheet,默认为空不要配置,除非读取表格中的某个子表\n"
        " 5.列标题不允许存在纯数字\n"
        " 6.exe和xls存放在同一目录并双击运行exe,生成一个新文件\n"
        << endl;

    char szPath[256] = {0};
    GetCurrentDirectory(sizeof(szPath), szPath);

    CString strIni;
    strIni.Format("%s\\%s", szPath, INI_FILE);

    if (GetFileAttributes(INI_FILE) == INVALID_FILE_ATTRIBUTES)
    {
        CFile file;
        if (!file.Open(INI_FILE, CFile::modeCreate | CFile::modeWrite))
        {
            printf("【错误】创建配置文件失败\n");
            system("pause");
            return 0;
        }
        file.Close();
        WritePrivateProfileString("excel", "filename", "test.xls", strIni);
        WritePrivateProfileString("excel", "columns", "title1#title2#title3", strIni);
        WritePrivateProfileString("excel", "sheet", "", strIni);
        printf("请在%s文件中配置信息\n", INI_FILE);
        system("pause");
        return 0;
    }

    char szFileName[256] = {0};
    char szTitle[1024] = {0};
    char szSheet[256] = {0};
    GetPrivateProfileString("excel", "filename", "", szFileName, sizeof(szFileName), strIni);
    GetPrivateProfileString("excel", "columns", "", szTitle, sizeof(szTitle), strIni);
    GetPrivateProfileString("excel", "sheet", "", szSheet, sizeof(szSheet), strIni);
    if (strlen(szFileName) == 0 || strlen(szTitle) == 0)
    {
        printf("【错误】请在%s文件配置正确的信息\n", INI_FILE);
        system("pause");
        return 0;
    }

    //解析标题
    std::vector<CString> vecTitle;
    CString strTitles(szTitle);
    CString strFileName(szFileName);
    CString strSheet(szSheet);
    int nStart = 0;
    CString strSub = strTitles.Tokenize("#", nStart);
    while (nStart != -1)
    {
        if (!strSub.IsEmpty())
        {
            vecTitle.push_back(strSub);
            //cout << strSub << endl;
        }
        strSub = strTitles.Tokenize("#", nStart);
    }
    if (vecTitle.empty())
    {
        cout << "【错误】请配置随机区域标题" << endl;
        system("pause");
        return 0;
    }

    CSpreadSheet excel(strFileName, strSheet, false);
    cout << "1.加载数据..." << endl;
    ReadAllRows(excel, g_allRows, g_title); //加载数据
    
//     for (int i = 0; i < g_title.GetSize(); ++i)
//     {
//         cout << i << " -> " << g_title[i] << endl;
//     }
    if (g_title.IsEmpty())
    {
        cout << "【错误】表格没有数据" << endl;
        system("pause");
        return 0;
    }

    cout << "2.随机区域..." << endl;
    ReadRandomColumns(excel, vecTitle, g_allRandomCells); //加载随机区域
    std::vector<int> vecCol = Title2Column(g_title, vecTitle);
    if (vecCol.empty() || vecCol.size() != vecTitle.size())
    {
        cout << "【错误】配置文件的标题与表格数据不对应" << endl;
        system("pause");
        return 0;
    }
    cout << "3.合并数据..." << endl;
    MergeExcelData(g_allRows, vecCol, g_allRandomCells); //合并数据
    cout << "4.写入数据...";
    CString strDstName;
    bool bSave = SaveNewExcel(strFileName, strSheet, g_title, g_allRows, strDstName);
    if (bSave)
    {
        cout << "成功写入文件 " << strDstName << endl;
    }
    else
    {
        cout << "写入失败" << endl;
    }
    cout << "5.释放内存..." << endl;
    FreeRow(g_allRows);
    cout << "处理完成" << endl;
    system("pause");

    return 0;
}