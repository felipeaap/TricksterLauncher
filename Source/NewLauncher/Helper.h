#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

#include "UpdateTypes.h"

class Helper
{
public:
    Helper();

    bool isMaintenance = false;
    std::atomic<bool> isWorkerDone{ false };
    int g_PopupPage = 0;
    int localVersion = 0;
    int currentVersion = 0;
    std::string g_Message;
    std::vector<Arquivo> ListaArquivos;
    int updateCount = 0;
    std::thread workerThread;
    std::atomic<bool> isRunning{ false };

    void GetLocalVersion();
    void SaveLocalVersion(int version);
    void ParseVersionedFileLists(bool isFullCheck = false);
    std::string GetFileFromURL(const std::string& customPath);
    std::string GetFileFromURL(int iType);
    bool iequals(const std::string& a, const std::string& b);
    void FileCheckUpdate();
    void CheckWorker(bool isFullCheck = false);
    bool WorkerUpdating(int updateCount);
    bool InjectDLL(HANDLE hProcess, const std::string& dllPath);
    void ClickPlayButton();
    void UpdateLauncher();
    std::filesystem::path GetGamePath();
};
