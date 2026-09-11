#include <iostream>

void RunDownloadPolicyTests();
void RunManifestSecurityTests();
void RunDownloadManagerTests();
void RunLauncherStateTests();
void RunConfigTests();

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " TricksterLauncher Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    try
    {
        RunDownloadPolicyTests();
        RunManifestSecurityTests();
        RunDownloadManagerTests();
        RunLauncherStateTests();
        RunConfigTests();

        std::cout << "========================================" << std::endl;
        std::cout << " ALL TESTS PASSED SUCCESSFULLY! (5/5 suites)" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] Unhandled exception in tests: " << ex.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "[FAIL] Unknown exception in tests." << std::endl;
        return 1;
    }
}
