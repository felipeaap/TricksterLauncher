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
#include <future>
#include <semaphore>
#include <cctype>
#include <array>

#include "json.hpp"
#include "../../Source/NewLauncher/ManifestSecurity.h"
#include "openssl/evp.h"

using json = nlohmann::json;

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
        FILE_ATTRIBUTE_NORMAL,
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
    std::array<unsigned char, 1024 * 1024> buffer{};
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

void listarArquivosRecursivo(const std::string& basePath,
                             const std::string& subPath,
                             std::vector<std::string>& arquivos) noexcept
{
    const std::string busca = basePath + "\\" + subPath + "\\*";
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(busca.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        const std::string nome = fd.cFileName;
        if (nome == "." || nome == "..")
            continue;

        const std::string caminhoRelativo = subPath.empty() ? nome : (subPath + "\\" + nome);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            listarArquivosRecursivo(basePath, caminhoRelativo, arquivos);
        else
            arquivos.push_back(caminhoRelativo);
    }
    while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

enum class ExecMode { Sequential, Parallel };

std::vector<Arquivo> gerarListaArquivos(const std::string& pastaRaiz, ExecMode modo) noexcept
{
    constexpr int maxThreads = 8;
    std::counting_semaphore<maxThreads> sem(maxThreads);
    std::vector<std::string> caminhosRelativos;
    listarArquivosRecursivo(pastaRaiz, "", caminhosRelativos);

    std::vector<std::string> caminhosValidos;
    caminhosValidos.reserve(caminhosRelativos.size());
    for (const auto& relPath : caminhosRelativos)
    {
        if (NormalizePath(relPath).find("splash.") != std::string::npos)
            continue;
        caminhosValidos.push_back(relPath);
    }

    std::vector<Arquivo> arquivos;
    arquivos.reserve(caminhosValidos.size());

    auto makeArquivo = [&](int fileID, const std::string& relPath, const std::string& hash)
    {
        const std::wstring fullPath = utf8ToUtf16(pastaRaiz + "\\" + relPath);
        arquivos.push_back({fileID, hash, relPath, false, GetFileSizeW(fullPath)});
    };

    if (modo == ExecMode::Sequential)
    {
        int fileID = 1;
        for (const auto& relPath : caminhosValidos)
        {
            const auto hash = CreateSHA256FromFileW(utf8ToUtf16(pastaRaiz + "\\" + relPath));
            if (!hash.empty())
                makeArquivo(fileID++, relPath, hash);
        }
        return arquivos;
    }

    std::vector<std::future<std::string>> futures;
    futures.reserve(caminhosValidos.size());
    for (const auto& relPath : caminhosValidos)
    {
        const std::wstring fullPath = utf8ToUtf16(pastaRaiz + "\\" + relPath);
        sem.acquire();
        futures.emplace_back(std::async(std::launch::async, [fullPath, &sem]()
        {
            const auto hash = CreateSHA256FromFileW(fullPath);
            sem.release();
            return hash;
        }));
    }

    int fileID = 1;
    for (size_t i = 0; i < caminhosValidos.size(); ++i)
    {
        const auto hash = futures[i].get();
        if (!hash.empty())
            makeArquivo(fileID++, caminhosValidos[i], hash);
    }

    return arquivos;
}

std::map<std::string, Arquivo> carregarArquivosAntigos(const std::string& pastaVersion)
{
    std::map<std::string, Arquivo> arquivos;
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA((pastaVersion + "\\*.json").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return arquivos;

    do
    {
        std::ifstream f(pastaVersion + "\\" + fd.cFileName);
        if (!f)
            continue;

        try
        {
            json document;
            f >> document;
            if (document.is_object())
                document = document.value("files", json::array());
            if (!document.is_array())
                continue;

            for (const auto& entry : document)
            {
                Arquivo arq;
                arq.FileID = entry.value("FileID", 0);
                arq.FileHash = entry.value("FileHash", "");
                arq.FilePath = entry.value("FilePath", "");
                arq.ToUpdate = entry.value("ToUpdate", false);
                arq.FileSize = entry.value("FileSize", 0LL);
                arquivos[NormalizePath(arq.FilePath)] = arq;
            }
        }
        catch (const json::exception&)
        {
            continue;
        }
    }
    while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return arquivos;
}

int proximaVersao(const std::string& pastaVersion)
{
    int maior = 0;
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA((pastaVersion + "\\version_*.json").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 1;

    do
    {
        int v = 0;
        if (sscanf_s(fd.cFileName, "version_%d.json", &v) == 1)
            maior = std::max(maior, v);
    }
    while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return maior + 1;
}

void salvarVersaoLegada(const std::string& pastaVersion,
                        int versao,
                        const std::vector<Arquivo>& arquivos)
{
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

    std::ostringstream nome;
    nome << pastaVersion << "\\version_" << versao << ".json";
    std::ofstream output(nome.str(), std::ios::binary);
    if (output)
        output << document.dump(4);
}

bool salvarManifesto(const std::vector<Arquivo>& arquivos,
                     int versao,
                     const std::string& privateKeyPath)
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
            return false;

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
    }

    std::ofstream output("manifest.json", std::ios::binary);
    if (!output)
        return false;

    output << document.dump(4);
    return output.good();
}

void salvarLauncherHash(const std::string& caminhoSplash)
{
    const std::string hash = CreateSHA256FromFileW(utf8ToUtf16(caminhoSplash));
    std::ofstream outLauncher("launcher.txt", std::ios::binary);
    if (outLauncher)
        outLauncher << hash;
}

int main(int argc, char* argv[])
{
    ExecMode modo = ExecMode::Parallel;
    if (argc > 1 && std::string(argv[1]) == "sequential")
        modo = ExecMode::Sequential;

    const std::string privateKeyPath = argc > 2 ? argv[2] : std::string{};

    const auto antigos = carregarArquivosAntigos("version");
    const auto novos = gerarListaArquivos("Update", modo);

    std::vector<Arquivo> alterados;
    for (const auto& novo : novos)
    {
        const auto it = antigos.find(NormalizePath(novo.FilePath));
        if (it == antigos.end() || it->second.FileHash != novo.FileHash ||
            it->second.FileSize != novo.FileSize)
            alterados.push_back(novo);
    }

    const int versao = proximaVersao("version");
    if (!alterados.empty())
        salvarVersaoLegada("version", versao, alterados);

    if (!salvarManifesto(novos, versao, privateKeyPath))
        return 1;

    salvarLauncherHash("Update\\Splash.exe");
    return 0;
}
