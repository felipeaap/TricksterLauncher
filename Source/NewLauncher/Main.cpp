#define MI_MALLOC_OVERRIDE
#include <mimalloc-new-delete.h>

#include "LauncherApplication.h"

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR arguments, int commandShow)
{
    (void)previousInstance;
    (void)arguments;

    LauncherApplication application;
    return application.Run(instance, commandShow);
}
