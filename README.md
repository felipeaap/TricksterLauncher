# Trickster Online - Game Launcher

High-performance, modern game launcher and patch client for Trickster Online built with C++20, Direct3D 9, and Dear ImGui.

---

## 🚀 Key Features

- **Modern Vector UI**: Fluid animations, reactive status indicators, and integrated in-launcher login flow built with Dear ImGui and Direct3D 9.
- **Fast Integrity Verification**: Multi-threaded SHA-256 verification and atomic delta updates.
- **Robust CDN & Mirror Failover**: Automatic retry policies, download telemetry, and host rotation.
- **Secure Self-Updating**: Standalone self-updater (`apps/LauncherUpdater.exe`) with binary swap verification.
- **Zero External Runtimes**: Self-contained C++ native stack without WebView or .NET dependencies.

---

## 🛠️ How to Build

1. Install **Visual Studio 2022** with the **Desktop development with C++** workload.
2. Clone this repository:
   ```bash
   git clone https://github.com/felipeaap/TricksterLauncher.git
   ```
3. Open `Trickster Launcher.sln`.
4. Select `Release` configuration and `x86` platform.
5. Build the solution (`Ctrl+Shift+B` or via MSBuild).

Build outputs are generated in the `Output/` directory:
- `Output/Trickster Launcher/Splash.exe` (Main launcher application)
- `Output/Trickster Launcher/LauncherUpdater.exe` (Self-updater helper)
- `Output/FileListGen/FileListGen.exe` (Manifest and patch generator utility)
- `Output/Tests/TricksterLauncherTests.exe` (Unit test suite)

---

## 🧪 Running Unit Tests

Run the test suite from the root directory:
```powershell
.\Output\Tests\TricksterLauncherTests.exe
```

---

## 📦 Patch Server Deployment

The repository includes a ready-to-use deploy package in `deploy/`:

```text
deploy/
├── README_DEPLOY.txt
└── patch/
    ├── FileListGen.exe    # Scans files and generates manifest.json and launcher.txt
    ├── Splash.exe         # Latest launcher binary for self-updating
    ├── launcher.txt       # SHA-256 hash of Splash.exe
    ├── manifest.json      # Game client file manifest
    └── auth.php           # Authentication proxy endpoint
```

To update client files on the CDN, place new game client files under `deploy/patch/`, navigate to `deploy/patch/` in terminal, and execute:
```powershell
.\FileListGen.exe
```

---

## ⚙️ Client Configuration (`LauncherData/config.json`)

```json
{
  "window_title": "Trickster Online",
  "subtitle": "Game Launcher",
  "cdn": "127.0.0.1/patch",
  "use_ssl": false,
  "game_exec": "Trickster/trickster.bin",
  "region": "thailand"
}
```
