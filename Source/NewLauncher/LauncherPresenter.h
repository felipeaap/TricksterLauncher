#pragma once

#include <memory>
#include <string>
#include "Helper.h"
#include "LauncherState.h"
#include "LauncherView.h"

class LauncherPresenter
{
public:
    LauncherPresenter();
    ~LauncherPresenter();

    LauncherPresenter(const LauncherPresenter&) = delete;
    LauncherPresenter& operator=(const LauncherPresenter&) = delete;

    void Initialize() noexcept;
    void Update() noexcept;
    void Shutdown() noexcept;

    LauncherViewEvents CreateViewEvents() noexcept;

    bool ShouldClose() const noexcept { return shouldClose_; }
    void RequestClose() noexcept { shouldClose_ = true; }

    void OnPlay() noexcept;
    void OnCheckFiles() noexcept;
    void OnOption() noexcept;
    void OnExit() noexcept;
    void OnOpenLink(const std::string& url) noexcept;

private:
    std::unique_ptr<Helper> helper_;
    bool isVerifying_ = false;
    bool shouldClose_ = false;
};
