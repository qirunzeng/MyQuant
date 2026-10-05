# Windows build, deployment, and upgrade

Use Qt 6.5 or newer and a matching compiler. This checkout was verified with
MSYS2 UCRT64 GCC 15.2 and Qt 6.10.0. For MSVC Qt, use a Visual Studio developer
PowerShell with Ninja on PATH; do not combine an MSVC Qt kit with MinGW.

```powershell
# MSYS2 UCRT64 example; use your actual kit path.
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake --preset windows-release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build --preset windows-release --parallel 4
$env:MYQUANT_TEST_PYTHON = 'C:\msys64\ucrt64\bin\python.exe'
ctest --preset windows-release
./tools/package_windows.ps1 -QtBin C:/msys64/ucrt64/bin
```

The ZIP under `dist` includes only the application, deployed dependencies,
helper scripts, public seed universe, and license notices. Each packaging run
uses a fresh stage; an existing version ZIP must be moved before another run.
The script rejects UI verification builds. Python/AkShare remain optional
external dependencies; configure the real Python executable in Settings.
MSYS2 packaging also resolves non-Qt DLL imports from the selected kit and
writes `qt.conf` so the installed program finds its own plugins and QML modules.

## Safe upgrade

Close MyQuant. Keep a private backup of `%LOCALAPPDATA%\MyQuant`, then install
the ZIP into a new directory:

```powershell
./tools/update_windows.ps1 -Archive ./dist/MyQuant-0.2.0-windows.zip -InstallRoot "$env:LOCALAPPDATA\Programs\MyQuant"
```

Open the printed executable path and update any shortcut to that executable.
The old application directory is retained for rollback. The update script
never reads or writes the runtime folder. To remove the application, delete
only its program directory; retain `%LOCALAPPDATA%\MyQuant`. A rollback of
application binaries does not roll back a database migration: restore a
matching private database backup if a future release changes schemas.

## Isolated UI capture

```powershell
cmake --preset windows-release -DMYQUANT_UI_VERIFY=ON
cmake --build --preset windows-release --parallel 4
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QUICK_BACKEND = 'software'
$env:QT_FORCE_STDERR_LOGGING = '1'
foreach ($scale in @('1','1.25','1.5','2')) {
    $env:QT_SCALE_FACTOR = $scale
    $env:MYQUANT_UI_CAPTURE_DIR = "$PWD/build/ui-check/$scale"
    Start-Process ./build/windows-release/MyQuant.exe -WindowStyle Hidden -Wait
}
cmake --preset windows-release -DMYQUANT_UI_VERIFY=OFF
cmake --build --preset windows-release --parallel 4
```

Capture mode always uses temporary data. It produces 16 frames per scale:
four pages at 900 and 1440 logical pixels, light then dark. These captures
check initial layouts, not scrolling, native graphics drivers, editing, or
populated brokerage data. Perform interactive checks on the target monitor
before release. Do not distribute capture-mode binaries.
