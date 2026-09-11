#include "LauncherPresenter.h"

#include <filesystem>
#include <shellapi.h>
#define NOMINMAX
#include <windows.h>

#include "Config.h"
#include "Language.h"

LauncherPresenter::LauncherPresenter()
    : helper_(std::make_unique<Helper>())
{
}

LauncherPresenter::~LauncherPresenter()
{
    Shutdown();
}

void LauncherPresenter::Initialize() noexcept
{
    if (!helper_)
        helper_ = std::make_unique<Helper>();

    // Inform the view that we're checking server status
    LauncherState::SetStatus(lang::GetString("launcher_checking"));
    LauncherState::SetSpeed("");
    LauncherState::SetProgress(0.0f, 0.0f);
    LauncherState::SetButtons(false, false, false);

    std::string maintenanceCheck = helper_->GetFileFromURL(0);
    helper_->isMaintenance = (maintenanceCheck == "true");
    LauncherState::SetMaintenance(helper_->isMaintenance);

    if (!isVerifying_)
    {
        helper_->UpdateLauncher();
        helper_->CheckWorker(false);
        isVerifying_ = true;
    }
}

void LauncherPresenter::Update() noexcept
{
    if (!helper_)
        return;

    if (helper_->isWorkerDone.load(std::memory_order_acquire))
    {
        const bool canPlay = !helper_->isMaintenance;
        LauncherState::SetButtons(canPlay, true, true);
    }
    else
    {
        LauncherState::SetButtons(false, false, false);
    }
}

void LauncherPresenter::Shutdown() noexcept
{
    if (helper_ && helper_->workerThread.joinable())
    {
        helper_->workerThread.join();
    }
}

LauncherViewEvents LauncherPresenter::CreateViewEvents() noexcept
{
    LauncherViewEvents events;
    events.onPlayClicked = [this]() { OnPlay(); };
    events.onCheckClicked = [this]() { OnCheckFiles(); };
    events.onOptionClicked = [this]() { OnOption(); };
    events.onExitClicked = [this]() { OnExit(); };
    events.onLinkClicked = [this](const std::string& url) { OnOpenLink(url); };
    return events;
}

void LauncherPresenter::OnPlay() noexcept
{
    if (helper_ && helper_->isWorkerDone.load(std::memory_order_acquire) && !helper_->isMaintenance)
    {
        helper_->ClickPlayButton();
    }
}

void LauncherPresenter::OnCheckFiles() noexcept
{
    if (!helper_)
        return;

    LauncherState::SetButtons(false, false, false);
    helper_->isWorkerDone.store(false, std::memory_order_release);
    helper_->CheckWorker(true);
}

void LauncherPresenter::OnOption() noexcept
{
    if (!helper_)
        return;

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi{};
    const std::filesystem::path gameExe = helper_->GetGamePath() / config::OptionExecName.c_str();
    const std::string exePath = gameExe.string();
    if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        MessageBoxA(nullptr, lang::GetString("launcher_setup_fail").c_str(), "Error!", MB_OK);
    }
}

void LauncherPresenter::OnExit() noexcept
{
    shouldClose_ = true;
}

void LauncherPresenter::OnOpenLink(const std::string& url) noexcept
{
    if (!url.empty())
    {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}
