#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <vector>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setCentralWidget(ui->splitter);
    ui->processTable->insertColumn(0);
    ui->processTable->insertColumn(1);
    ui->processTable->insertColumn(2);
    ui->processTable->setHorizontalHeaderLabels({"Name", "PID", "Memory Usage"});
    ui->processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui->actionRefresh, &QAction::triggered, this, &MainWindow::RefreshProcesses);
}

MainWindow::~MainWindow()
{
    delete ui;
}

struct ProcessInfo {
    DWORD pid;
    QString name;
    SIZE_T memoryUsage;
};

QPixmap hIconToPixmap(HICON hIcon) {
    if (!hIcon) return QPixmap();

    // Создаём QImage из HICON
    ICONINFO iconInfo;
    GetIconInfo(hIcon, &iconInfo);

    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32);
    HDC hdc = CreateCompatibleDC(NULL);
    SelectObject(hdc, iconInfo.hbmColor);

    // Сохраняем данные пикселей в QImage
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight; // инвертируем ось Y
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    // Копируем пиксели
    GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, image.bits(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // Освобождаем ресурсы
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DeleteDC(hdc);

    // Возвращаем QPixmap
    return QPixmap::fromImage(image);
}

QString getProcessPath(HANDLE hProcess) {
    TCHAR processPath[MAX_PATH];
    if (GetModuleFileNameEx(hProcess, NULL, processPath, MAX_PATH)) {
        return QString::fromWCharArray(processPath);
    }
    return QString();
}

QIcon getProcessIcon(const QString& processPath) {
    SHFILEINFO shFileInfo;
    if (SHGetFileInfo(processPath.toStdWString().c_str(), 0, &shFileInfo, sizeof(SHFILEINFO), SHGFI_ICON)) {
        QPixmap pixmap = hIconToPixmap(shFileInfo.hIcon);
        DestroyIcon(shFileInfo.hIcon);
        return QIcon(pixmap);
    }
    return QIcon();
}

std::vector<ProcessInfo> getProcess(){

    DWORD processes[1024], cbNeeded;
    std::vector<ProcessInfo> processList;
    if (!EnumProcesses(processes, sizeof(processes), &cbNeeded)){
        return processList;
    }


    for (unsigned int i = 0; i < cbNeeded / sizeof(DWORD); ++i){
        if (processes[i] != 0){
            ProcessInfo pInfo;
            pInfo.pid = processes[i];

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
            if (hProcess) {
                TCHAR processName[MAX_PATH] = TEXT("<unknown>");
                HMODULE hModule;
                DWORD cbNeeded;

                if (EnumProcessModules(hProcess, &hModule, sizeof(hModule), &cbNeeded)) {
                    GetModuleBaseName(hProcess, hModule, processName, sizeof(processName) / sizeof(TCHAR));
                }


                PROCESS_MEMORY_COUNTERS memCounters;
                if (GetProcessMemoryInfo(hProcess, &memCounters, sizeof(memCounters))) {
                    pInfo.memoryUsage = memCounters.WorkingSetSize;
                }

                pInfo.name = QString::fromWCharArray(processName);
                processList.push_back(pInfo);
                CloseHandle(hProcess);

            }
        }
    }


    return processList;
}

std::vector<QString> GetModules(DWORD processID) {
    std::vector<QString> modules;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (!hProcess) return modules;

    HMODULE hModules[1024];
    DWORD cbNeeded;


    if (EnumProcessModulesEx(hProcess, hModules, sizeof(hModules), &cbNeeded, LIST_MODULES_ALL)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR moduleName[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, hModules[i], moduleName, sizeof(moduleName) / sizeof(TCHAR))) {
                modules.push_back(QString::fromWCharArray(moduleName));
            }
        }
    }
    CloseHandle(hProcess);
    return modules;
}

void MainWindow::RefreshProcesses(){
    ui->processTable->setRowCount(0);
    auto processes = getProcess();


    for (const auto &p : processes) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, p.pid);
        QString processPath = getProcessPath(hProcess);
        QIcon processIcon = getProcessIcon(processPath);

        int row = ui->processTable->rowCount();
        ui->processTable->insertRow(row);
        QTableWidgetItem* iconItem = new QTableWidgetItem(processIcon,(p.name));
        ui->processTable->setItem(row, 0, iconItem);
        ui->processTable->setItem(row, 1, new QTableWidgetItem(QString::number(p.pid)));
        ui->processTable->setItem(row, 2, new QTableWidgetItem(QString::number(p.memoryUsage / 1024) + " KB"));
    }
}


void MainWindow::on_processTable_itemClicked(QTableWidgetItem *item)
{
    ui->moduleTable->clear();
    auto processes = getProcess();
    auto modules = GetModules(processes[item->row()].pid);
    for (const auto p : modules){
        ui->moduleTable->addItem(p);
    }
}
