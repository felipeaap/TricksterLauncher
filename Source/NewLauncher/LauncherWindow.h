#pragma once

#include <Windows.h>

class LauncherWindow
{
public:
    bool Create(const wchar_t* title) const noexcept;
    void Destroy() const noexcept;
    bool PumpMessages() const noexcept;
};
