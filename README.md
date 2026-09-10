https://github.com/user-attachments/assets/1ee4a782-afbc-4f08-845f-88c6b3453b9b

# Trickster-Launcher

## How to build
1. Install **Visual Studio 2022** with the **Desktop development with C++** workload and the required VS 2022 C++ components.
2. Clone this repository.
3. Open **Trickster Launcher.sln**. The solution now contains both `NewLauncher` and the `FileListGen` tool.
4. Restore the required NuGet packages for the launcher project.
5. Adjust the launcher configuration in `Source/NewLauncher/Config.cpp` as needed.
6. Build the solution. Build outputs are generated under `Output/`, which is intentionally ignored by Git.

## FileListGen
`FileListGen` is an independent build tool located at `tools/FileListGen` and is included in the main Visual Studio solution.

The tool expects the update workspace to contain:

```text
<Update root>/
├── Update/
│   └── ... game files ...
└── version/
```

Run `FileListGen.exe` from the update workspace. It scans `Update/`, generates the next `version_N.json` when changes are detected, and updates `launcher.txt` from `Update\\Splash.exe`.

## What should be distributed to players?
Distribute the runtime files produced for the launcher. Development artifacts such as `.pdb` files are not required by players.

## Hosting the update files
The update host should expose the same directory structure expected by the launcher. Keep `Splash.exe` inside the `Update` directory so `FileListGen` can calculate its current launcher hash.

`maintenance.txt` controls maintenance mode in the existing launcher workflow: `true` enables maintenance mode and `false` keeps the launcher online.
