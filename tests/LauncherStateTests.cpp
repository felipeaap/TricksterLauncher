#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

#include "LauncherState.h"

void RunLauncherStateTests()
{
    std::cout << "[TEST] Running LauncherState tests..." << std::endl;

    // Test 1: Default initial values
    {
        LauncherViewState snap = LauncherState::GetSnapshot();
        assert(snap.fileProgress >= 0.0f && snap.fileProgress <= 1.0f);
        assert(snap.totalProgress >= 0.0f && snap.totalProgress <= 1.0f);
    }

    // Test 2: Progress and string updates
    {
        LauncherState::SetProgress(0.45f, 0.75f);
        LauncherState::SetStatus("Downloading test.dat (1/10)");
        LauncherState::SetSpeed("2.5 MB/s");

        LauncherViewState snap = LauncherState::GetSnapshot();
        assert(snap.fileProgress == 0.45f);
        assert(snap.totalProgress == 0.75f);
        assert(snap.fileString == "Downloading test.dat (1/10)");
        assert(snap.speedString == "2.5 MB/s");
    }

    // Test 3: Button states
    {
        LauncherState::SetButtons(true, true, false);
        LauncherViewState snap = LauncherState::GetSnapshot();
        assert(snap.isGameEnabled == true);
        assert(snap.isCheckEnabled == true);
        assert(snap.isOptionEnabled == false);

        LauncherState::SetMaintenance(true);
        snap = LauncherState::GetSnapshot();
        assert(snap.isMaintenance == true);
    }

    // Test 4: Concurrency test with multiple threads mutating & reading snapshots
    {
        std::atomic<bool> keepRunning{ true };
        std::vector<std::thread> workers;

        for (int i = 0; i < 4; ++i)
        {
            workers.emplace_back([i, &keepRunning]()
            {
                int counter = 0;
                while (keepRunning.load(std::memory_order_relaxed))
                {
                    float p = static_cast<float>((counter % 100)) / 100.0f;
                    LauncherState::SetProgress(p, p);
                    LauncherState::SetStatus("Worker " + std::to_string(i) + " file " + std::to_string(counter));
                    LauncherState::SetSpeed(std::to_string(counter) + " KB/s");
                    LauncherState::SetButtons((counter % 2) == 0, (counter % 3) == 0, (counter % 5) == 0);
                    counter++;
                }
            });
        }

        // Reader threads
        for (int i = 0; i < 2; ++i)
        {
            workers.emplace_back([&keepRunning]()
            {
                while (keepRunning.load(std::memory_order_relaxed))
                {
                    LauncherViewState snap = LauncherState::GetSnapshot();
                    assert(snap.fileProgress >= 0.0f && snap.fileProgress <= 1.0f);
                    assert(snap.totalProgress >= 0.0f && snap.totalProgress <= 1.0f);
                    assert(!snap.fileString.empty());
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        keepRunning.store(false, std::memory_order_relaxed);

        for (auto& t : workers)
        {
            if (t.joinable())
                t.join();
        }
    }

    std::cout << "[PASS] LauncherState tests passed!" << std::endl;
}
