#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "../Source/NewLauncher/DownloadManager.h"

void RunDownloadManagerTests()
{
    std::cout << "[TEST] Running DownloadManager mock HTTP tests..." << std::endl;

    httplib::Server svr;

    // Generate 1 MB test payload
    std::string testData(1024 * 1024, 'X');
    for (size_t i = 0; i < testData.size(); i += 1024)
        testData[i] = static_cast<char>('A' + (i / 1024) % 26);

    svr.Get("/Update/testfile.bin", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (req.has_header("Range"))
        {
            std::string range = req.get_header_value("Range");
            // Basic range parser: bytes=START-END
            size_t eq = range.find('=');
            size_t dash = range.find('-', eq);
            if (eq != std::string::npos && dash != std::string::npos)
            {
                size_t start = static_cast<size_t>(std::stoull(range.substr(eq + 1, dash - eq - 1)));
                size_t end = testData.size() - 1;
                std::string endStr = range.substr(dash + 1);
                if (!endStr.empty())
                    end = static_cast<size_t>(std::stoull(endStr));

                if (start < testData.size() && end >= start)
                {
                    size_t len = std::min(end - start + 1, testData.size() - start);
                    res.status = 206;
                    res.set_header("Content-Range", "bytes " + std::to_string(start) + "-" +
                                                    std::to_string(start + len - 1) + "/" +
                                                    std::to_string(testData.size()));
                    res.set_header("Accept-Ranges", "bytes");
                    res.set_content(testData.data() + start, len, "application/octet-stream");
                    return;
                }
            }
        }
        res.status = 200;
        res.set_header("Accept-Ranges", "bytes");
        res.set_content(testData, "application/octet-stream");
    });

    svr.Get("/test_small.txt", [&](const httplib::Request&, httplib::Response& res)
    {
        res.status = 200;
        res.set_content("Hello from mock server!", "text/plain");
    });

    const int port = 18088;
    std::thread serverThread([&]()
    {
        svr.listen("127.0.0.1", port);
    });

    // Wait for server startup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try
    {
        DownloadManager::Options options;
        options.maxRetries = 2;
        options.connectionTimeoutSeconds = 3;
        options.maxConnections = 2;
        options.multiConnectionThresholdBytes = 100 * 1024; // Lower threshold to test multi-connection
        options.segmentSizeBytes = 256 * 1024; // 256 KB segments

        DownloadManager dm("127.0.0.1:" + std::to_string(port), false, options);

        // 1. Test Get()
        std::string smallResponse = dm.Get("/test_small.txt");
        assert(smallResponse == "Hello from mock server!");

        // 2. Test Download() with multi-connection segments
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "TricksterLauncherTest";
        std::filesystem::create_directories(tempDir);
        const std::filesystem::path localOut = tempDir / "downloaded_test.bin";

        std::error_code ec;
        std::filesystem::remove(localOut, ec);

        long long totalProgressReceived = 0;
        bool errorTriggered = false;

        bool ok = dm.Download(
            "testfile.bin",
            localOut.string(),
            [&](long long curr, long long total) { totalProgressReceived = curr; },
            [](double) {},
            [&](const std::string& err) { errorTriggered = true; }
        );

        assert(ok);
        assert(!errorTriggered);
        assert(std::filesystem::exists(localOut));
        assert(std::filesystem::file_size(localOut) == testData.size());

        // Verify content integrity
        std::ifstream inFile(localOut, std::ios::binary);
        std::string downloadedContent((std::istreambuf_iterator<char>(inFile)),
                                      std::istreambuf_iterator<char>());
        assert(downloadedContent == testData);

        // 3. Test Download() with hash matching
        const std::string testHash = "dummy_hash_12345";
        bool okWithHash = dm.Download(
            "testfile.bin",
            localOut.string(),
            [](long long, long long) {},
            [](double) {},
            [](const std::string&) {},
            testHash
        );
        assert(okWithHash);
        assert(std::filesystem::exists(localOut));
        assert(std::filesystem::file_size(localOut) == testData.size());

        // Cleanup
        inFile.close();
        std::filesystem::remove(localOut, ec);
        std::filesystem::remove_all(tempDir, ec);

        std::cout << "[PASS] DownloadManager mock HTTP tests passed!" << std::endl;
    }
    catch (...)
    {
        svr.stop();
        if (serverThread.joinable())
            serverThread.join();
        throw;
    }

    svr.stop();
    if (serverThread.joinable())
        serverThread.join();
}
