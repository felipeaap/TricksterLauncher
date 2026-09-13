#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <map>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cctype>
#include <array>

#include "json.hpp"
#include "../../Source/NewLauncher/ManifestSecurity.h"
#include "openssl/evp.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct Arquivo
{
    int FileID = 0;
    std::string FileHash;
    std::string FilePath;
    bool ToUpdate = false;
    long long FileSize = 0;
};

std::wstring utf8ToUtf16(const std::string& str)
{
    if (str.empty()) return L"";
    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstr(sizeNeeded, L'\0');
    if (sizeNeeded > 0)
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), wstr.data(), sizeNeeded);
    return wstr;
}

std::string utf16ToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string str(sizeNeeded, '\0');
    if (sizeNeeded > 0)
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), str.data(), sizeNeeded, nullptr, nullptr);
    return str;
}

std::string NormalizePath(const std::string& value)
{
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}

long long GetFileSizeW(const std::wstring& filePath) noexcept
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &data))
        return 0;

    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return static_cast<long long>(size.QuadPart);
}

std::string CreateSHA256FromFileW(const std::wstring& filePath) noexcept
{
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"[ERROR] Could not open file: " << filePath << std::endl;
        return {};
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
    {
        CloseHandle(hFile);
        return {};
    }

    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digestLength = 0;
    std::array<unsigned char, 64 * 1024> buffer{};
    bool success = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;

    while (success)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
        {
            success = false;
            break;
        }

        if (bytesRead == 0)
            break;

        success = EVP_DigestUpdate(context, buffer.data(), bytesRead) == 1;
    }

    if (success)
        success = EVP_DigestFinal_ex(context, digest, &digestLength) == 1;

    EVP_MD_CTX_free(context);
    CloseHandle(hFile);

    if (!success)
        return {};

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLength; ++i)
        result << std::setw(2) << static_cast<unsigned int>(digest[i]);
    return result.str();
}

bool ShouldIgnoreFile(const std::string& normalizedRelPath) noexcept
{
    if (normalizedRelPath.empty())
        return true;

    // Filter launcher executable, generator and manifests
    if (normalizedRelPath == "splash.exe" || normalizedRelPath.ends_with("/splash.exe") ||
        normalizedRelPath == "filelistgen.exe" || normalizedRelPath.ends_with("/filelistgen.exe") ||
        normalizedRelPath == "manifest.json" || normalizedRelPath.ends_with("/manifest.json") ||
        normalizedRelPath == "launcher.txt" || normalizedRelPath.ends_with("/launcher.txt"))
        return true;

    // Filter web endpoints, tokens and documentation
    if (normalizedRelPath == "auth.php" || normalizedRelPath.ends_with("/auth.php") ||
        normalizedRelPath == "launcher_auth.php" || normalizedRelPath.ends_with("/launcher_auth.php") ||
        normalizedRelPath == "auth_token.txt" || normalizedRelPath.ends_with("/auth_token.txt") ||
        normalizedRelPath == "auth_token.enc" || normalizedRelPath.ends_with("/auth_token.enc") ||
        normalizedRelPath == "maintenance.txt" || normalizedRelPath.ends_with("/maintenance.txt") ||
        normalizedRelPath == "readme_deploy.txt" || normalizedRelPath.ends_with("/readme_deploy.txt"))
        return true;

    // Filter temp / IDE / VCS / tools / keys / zip files
    if (normalizedRelPath.starts_with("version/") || normalizedRelPath.starts_with("tools/") ||
        normalizedRelPath.starts_with(".git/") || normalizedRelPath.starts_with(".vs/") ||
        normalizedRelPath.ends_with(".pem") || normalizedRelPath.ends_with(".zip") ||
        normalizedRelPath.ends_with(".tmp") || normalizedRelPath.ends_with(".part") ||
        normalizedRelPath.ends_with(".meta") || normalizedRelPath.ends_with(".bak") ||
        normalizedRelPath.ends_with(".log") || normalizedRelPath.ends_with(".pdb") ||
        normalizedRelPath.ends_with("thumbs.db") || normalizedRelPath.ends_with("desktop.ini"))
        return true;

    return false;
}

void ListFilesRecursiveUnicode(const fs::path& basePath, const fs::path& relativeToPath, std::vector<std::string>& outFiles) noexcept
{
    std::error_code ec;
    if (!fs::exists(basePath, ec) || !fs::is_directory(basePath, ec))
        return;

    for (const auto& entry : fs::recursive_directory_iterator(basePath, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }

        if (entry.is_regular_file(ec))
        {
            const auto rel = fs::relative(entry.path(), relativeToPath, ec);
            if (!ec)
            {
                std::string relStr = rel.generic_string();
                if (!ShouldIgnoreFile(NormalizePath(relStr)))
                {
                    outFiles.push_back(relStr);
                }
            }
            ec.clear();
        }
    }
}

enum class ExecMode { Sequential, Parallel };

std::vector<Arquivo> gerarListaArquivos(
    const std::string& pastaRaiz,
    const std::string& baseRelativa,
    ExecMode modo,
    const std::map<std::string, Arquivo>& cacheAntigo) noexcept
{
    std::vector<std::string> caminhosRelativos;
    const fs::path rootPath(utf8ToUtf16(pastaRaiz));
    const fs::path relBasePath = baseRelativa.empty() ? fs::current_path() : fs::path(utf8ToUtf16(baseRelativa));
    ListFilesRecursiveUnicode(rootPath, relBasePath, caminhosRelativos);

    std::vector<Arquivo> arquivos(caminhosRelativos.size());
    const size_t totalFiles = caminhosRelativos.size();
    std::atomic<size_t> cachedCount{ 0 };
    std::atomic<size_t> hashedCount{ 0 };

    const unsigned int threadCount = (modo == ExecMode::Sequential)
        ? 1u
        : (std::max)(1u, std::thread::hardware_concurrency());

    std::cout << "[INFO] Processing " << totalFiles << " files using " << threadCount << " worker thread(s)..." << std::endl;

    std::atomic<size_t> currentIndex{ 0 };
    auto workerFunc = [&]()
    {
        while (true)
        {
            const size_t i = currentIndex.fetch_add(1, std::memory_order_relaxed);
            if (i >= totalFiles)
                break;

            const std::string& relPath = caminhosRelativos[i];
            const fs::path fullPath = relBasePath / utf8ToUtf16(relPath);
            const long long fileSize = GetFileSizeW(fullPath.wstring());
            const std::string normalized = NormalizePath(relPath);

            std::string hash;
            const auto it = cacheAntigo.find(normalized);
            if (it != cacheAntigo.end() && it->second.FileSize == fileSize && !it->second.FileHash.empty())
            {
                // Quick cache verification: if size matches, reuse known hash
                hash = it->second.FileHash;
                cachedCount.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                hash = CreateSHA256FromFileW(fullPath.wstring());
                hashedCount.fetch_add(1, std::memory_order_relaxed);
            }

            arquivos[i] = { static_cast<int>(i + 1), hash, relPath, false, fileSize };
        }
    };

    if (threadCount <= 1)
    {
        workerFunc();
    }
    else
    {
        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        for (unsigned int t = 0; t < threadCount; ++t)
            threads.emplace_back(workerFunc);
        for (auto& th : threads)
            th.join();
    }

    std::cout << "[INFO] Hashing complete: " << hashedCount.load() << " computed, "
              << cachedCount.load() << " reused from cache." << std::endl;

    // Filter out any entries where hashing failed (e.g. locked files)
    std::vector<Arquivo> resultadoValido;
    resultadoValido.reserve(arquivos.size());
    int validId = 1;
    for (auto& arq : arquivos)
    {
        if (!arq.FileHash.empty())
        {
            arq.FileID = validId++;
            resultadoValido.push_back(std::move(arq));
        }
    }

    return resultadoValido;
}

std::map<std::string, Arquivo> carregarArquivosAntigos(const std::string& pastaVersion)
{
    std::map<std::string, Arquivo> arquivos;
    std::error_code ec;
    const fs::path vDir(utf8ToUtf16(pastaVersion));
    if (!fs::exists(vDir, ec) || !fs::is_directory(vDir, ec))
        return arquivos;

    for (const auto& entry : fs::directory_iterator(vDir, ec))
    {
        if (ec) break;
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
        {
            std::ifstream f(entry.path());
            if (!f) continue;

            try
            {
                json document;
                f >> document;
                if (document.is_object())
                    document = document.value("files", json::array());
                if (!document.is_array())
                    continue;

                for (const auto& item : document)
                {
                    Arquivo arq;
                    arq.FileID   = item.value("FileID", 0);
                    arq.FileHash = item.value("FileHash", "");
                    arq.FilePath = item.value("FilePath", "");
                    arq.ToUpdate = item.value("ToUpdate", false);
                    arq.FileSize = item.value("FileSize", 0LL);
                    if (!arq.FilePath.empty())
                        arquivos[NormalizePath(arq.FilePath)] = arq;
                }
            }
            catch (const json::exception&)
            {
                continue;
            }
        }
    }

    return arquivos;
}

int proximaVersao(const std::string& pastaVersion)
{
    int maior = 0;
    std::error_code ec;
    const fs::path vDir(utf8ToUtf16(pastaVersion));
    if (!fs::exists(vDir, ec) || !fs::is_directory(vDir, ec))
        return 1;

    for (const auto& entry : fs::directory_iterator(vDir, ec))
    {
        if (ec) break;
        if (entry.is_regular_file(ec))
        {
            const std::string filename = entry.path().filename().string();
            int v = 0;
            if (sscanf_s(filename.c_str(), "version_%d.json", &v) == 1)
                maior = (std::max)(maior, v);
        }
    }

    return maior + 1;
}

void salvarVersaoLegada(const std::string& pastaVersion,
                        int versao,
                        const std::vector<Arquivo>& arquivos)
{
    std::error_code ec;
    fs::create_directories(fs::path(utf8ToUtf16(pastaVersion)), ec);

    json document = json::array();
    for (const auto& arq : arquivos)
    {
        document.push_back({
            {"FileID", arq.FileID},
            {"FileHash", arq.FileHash},
            {"FilePath", arq.FilePath},
            {"ToUpdate", arq.ToUpdate},
            {"FileSize", arq.FileSize}
        });
    }

    const fs::path outputName = fs::path(utf8ToUtf16(pastaVersion)) / ("version_" + std::to_string(versao) + ".json");
    std::ofstream output(outputName, std::ios::binary);
    if (output)
        output << document.dump(4);
}

bool salvarManifesto(const std::vector<Arquivo>& arquivos,
                     int versao,
                     const std::string& privateKeyPath,
                     const std::string& manifestOutputPath)
{
    json document;
    document["version"] = versao;
    document["algorithm"] = "sha256";
    document["files"] = json::array();

    for (const auto& arq : arquivos)
    {
        document["files"].push_back({
            {"FileID", arq.FileID},
            {"FileHash", arq.FileHash},
            {"FilePath", arq.FilePath},
            {"ToUpdate", arq.ToUpdate},
            {"FileSize", arq.FileSize}
        });
    }

    if (!privateKeyPath.empty())
    {
        std::ifstream keyFile(privateKeyPath, std::ios::binary);
        if (!keyFile)
        {
            std::cerr << "[ERROR] Could not open private key file: " << privateKeyPath << std::endl;
            return false;
        }

        std::stringstream keyBuffer;
        keyBuffer << keyFile.rdbuf();
        const std::string signature = manifest_security::Sign(document.dump(), keyBuffer.str());
        if (signature.empty())
        {
            std::cerr << "[ERROR] Could not sign manifest." << std::endl;
            return false;
        }

        document["signature"] = {
            {"algorithm", "SHA256"},
            {"value", signature}
        };
        std::cout << "[INFO] Manifest successfully signed." << std::endl;
    }

    const fs::path outPath(utf8ToUtf16(manifestOutputPath));
    std::error_code ec;
    if (outPath.has_parent_path())
        fs::create_directories(outPath.parent_path(), ec);

    std::ofstream output(outPath, std::ios::binary);
    if (!output)
    {
        std::cerr << "[ERROR] Could not create output manifest: " << manifestOutputPath << std::endl;
        return false;
    }

    output << document.dump(4);
    return output.good();
}

void salvarLauncherHash(const std::string& updateDir, const std::string& manifestOutputPath)
{
    const fs::path outDir = fs::path(utf8ToUtf16(manifestOutputPath)).has_parent_path()
        ? fs::path(utf8ToUtf16(manifestOutputPath)).parent_path()
        : fs::current_path();

    std::vector<fs::path> candidateSplashPaths = {
        fs::path(utf8ToUtf16(updateDir)) / L"Splash.exe",
        fs::path(utf8ToUtf16(updateDir)) / L"splash.exe",
        outDir / L"Splash.exe",
        outDir / L"splash.exe",
        fs::current_path() / L"Splash.exe"
    };

    std::string splashHash;
    for (const auto& p : candidateSplashPaths)
    {
        std::error_code ec;
        if (fs::exists(p, ec))
        {
            splashHash = CreateSHA256FromFileW(p.wstring());
            if (!splashHash.empty())
            {
                std::wcout << L"[INFO] Extracted Splash.exe hash from: " << p.wstring() << std::endl;
                break;
            }
        }
    }

    if (!splashHash.empty())
    {
        const fs::path launcherTxtPath = outDir / L"launcher.txt";
        std::ofstream outLauncher(launcherTxtPath, std::ios::binary | std::ios::trunc);
        if (outLauncher)
        {
            outLauncher << splashHash;
            std::wcout << L"[SUCCESS] Generated launcher.txt in: " << launcherTxtPath.wstring() << std::endl;
        }
    }
}

void PrintHelp()
{
    std::cout << "FileListGen - High-Performance Manifest Generator for TricksterLauncher\n\n"
              << "Usage: FileListGen.exe [options]\n\n"
              << "Options:\n"
              << "  -s, --sign <key_path>       Sign manifest with RSA private key (PEM format)\n"
              << "  -g, --genkey <dir>          Generate a new RSA-PSS 3072 key pair and exit\n"
              << "  -m, --mode <parallel|seq>   Execution mode (default: parallel multi-thread)\n"
              << "  -u, --update-dir <path>     Directory to index (default: Trickster)\n"
              << "  -v, --version-dir <path>    Version history directory (default: version)\n"
              << "  -o, --output <path>         Output manifest file path (default: manifest.json)\n"
              << "  -h, --help                  Show this help message\n";
}

int main(int argc, char* argv[])
{
    const auto startTime = std::chrono::steady_clock::now();

    ExecMode modo = ExecMode::Parallel;
    std::string privateKeyPath;
    std::string genKeyDir;
    std::string updateDir = "Trickster";
    std::string versionDir = "version";
    std::string manifestOutput = "manifest.json";
    bool userSpecifiedUpdateDir = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?")
        {
            PrintHelp();
            return 0;
        }
        else if ((arg == "-s" || arg == "--sign") && i + 1 < argc)
        {
            privateKeyPath = argv[++i];
        }
        else if ((arg == "-g" || arg == "--genkey") && i + 1 < argc)
        {
            genKeyDir = argv[++i];
        }
        else if ((arg == "-m" || arg == "--mode") && i + 1 < argc)
        {
            std::string val = argv[++i];
            if (val == "sequential" || val == "seq")
                modo = ExecMode::Sequential;
            else
                modo = ExecMode::Parallel;
        }
        else if ((arg == "-u" || arg == "--update-dir") && i + 1 < argc)
        {
            updateDir = argv[++i];
            userSpecifiedUpdateDir = true;
        }
        else if ((arg == "-v" || arg == "--version-dir") && i + 1 < argc)
        {
            versionDir = argv[++i];
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc)
        {
            manifestOutput = argv[++i];
        }
        else if (arg == "sequential")
        {
            modo = ExecMode::Sequential;
        }
        else if (privateKeyPath.empty() && arg.find(".pem") != std::string::npos)
        {
            privateKeyPath = arg;
        }
    }

    if (!genKeyDir.empty())
    {
        std::cout << "[INFO] Generating RSA-PSS 3072 key pair in: " << genKeyDir << std::endl;
        std::string priv, pub;
        if (!manifest_security::GenerateKeyPair(priv, pub))
        {
            std::cerr << "[ERROR] Key pair generation failed." << std::endl;
            return 1;
        }

        std::error_code ec;
        fs::create_directories(fs::path(utf8ToUtf16(genKeyDir)), ec);
        std::ofstream privFile(fs::path(utf8ToUtf16(genKeyDir)) / L"private_key.pem", std::ios::binary);
        std::ofstream pubFile(fs::path(utf8ToUtf16(genKeyDir)) / L"public_key.pem", std::ios::binary);
        if (!privFile || !pubFile)
        {
            std::cerr << "[ERROR] Could not write key files to: " << genKeyDir << std::endl;
            return 1;
        }
        privFile << priv;
        pubFile << pub;
        std::cout << "[SUCCESS] Generated private_key.pem and public_key.pem in " << genKeyDir << std::endl;
        return 0;
    }

    // If update directory was not explicitly given and Trickster doesn't exist, check fallback
    std::error_code ecCheck;
    if (!userSpecifiedUpdateDir && !fs::exists(fs::path(utf8ToUtf16(updateDir)), ecCheck))
    {
        if (fs::exists(L"Update", ecCheck))
        {
            updateDir = "Update";
        }
    }

    const fs::path outPath(utf8ToUtf16(manifestOutput));
    const std::string baseRelativa = outPath.has_parent_path()
        ? utf16ToUtf8(outPath.parent_path().wstring())
        : "";

    const auto antigos = carregarArquivosAntigos(versionDir);
    const auto novos = gerarListaArquivos(updateDir, baseRelativa, modo, antigos);

    std::vector<Arquivo> alterados;
    for (const auto& novo : novos)
    {
        const auto it = antigos.find(NormalizePath(novo.FilePath));
        if (it == antigos.end() || it->second.FileHash != novo.FileHash ||
            it->second.FileSize != novo.FileSize)
        {
            alterados.push_back(novo);
        }
    }

    const int versao = proximaVersao(versionDir);
    if (!alterados.empty())
        salvarVersaoLegada(versionDir, versao, alterados);

    if (!salvarManifesto(novos, versao, privateKeyPath, manifestOutput))
        return 1;

    salvarLauncherHash(updateDir, manifestOutput);

    const double elapsedSec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - startTime).count();

    std::cout << "[SUCCESS] Manifest generated (version " << versao << ", "
              << novos.size() << " files, " << alterados.size() << " changed) in "
              << std::fixed << std::setprecision(2) << elapsedSec << "s." << std::endl;

    return 0;
}
