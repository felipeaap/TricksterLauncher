#include "LauncherWindow.h"

#include "Gui.h"

bool LauncherWindow::Create(const wchar_t* title) const noexcept
{
    if (!title)
        return false;

    gui::CreateHWindow(title);
    return gui::window != nullptr;
}

void LauncherWindow::Destroy() const noexcept
{
    gui::DestroyHWindow();
}

bool LauncherWindow::PumpMessages() const noexcept
{
    MSG message{};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            gui::isRunning = false;
            return false;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return gui::isRunning;
}
