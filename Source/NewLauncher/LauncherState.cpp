#include "LauncherState.h"

namespace LauncherState
{
    std::atomic<float> fileProgress{ 0.f };
    std::atomic<float> totalProgress{ 0.f };
    std::mutex fileStringMutex;
    std::string fileString;
    std::mutex speedStringMutex;
    std::string speedString{ "0 B/s" };
}
