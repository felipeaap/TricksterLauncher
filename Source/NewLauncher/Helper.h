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
    bool isMaintenance;
    bool isWorkerDone;
    int g_PopupPage;
    int localVersion, currentVersion;
    std::string g_Message;
    std::vector<Arquivo> ListaArquivos;
    int updateCount;
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
    std::string GetDirectoryFromPath(const std::string& filepath);
    bool CreateDirectoryIfNotExists(const std::string& dirPath);
    void WorkerUpdating(int updateCount);
    bool DownloadFile(const std::string& remoteFile, const std::string& localPath, int fileIndex, int totalFiles);
    bool InjectDLL(HANDLE hProcess, const std::string& dllPath);
    void ClickPlayButton();
    void UpdateLauncher();
    std::filesystem::path GetGamePath();
};
