param(
    [Parameter(Mandatory=$true)][string]$QtBin,
    [string]$BuildDir = 'build/windows-release'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$build = (Resolve-Path (Join-Path $repo $BuildDir)).Path
if (Select-String -LiteralPath (Join-Path $build 'CMakeCache.txt') -Pattern '^MYQUANT_UI_VERIFY:BOOL=ON$' -Quiet) {
    throw 'Disable MYQUANT_UI_VERIFY and rebuild before release packaging'
}
$qt = (Resolve-Path $QtBin).Path
$deploy = Join-Path $qt 'windeployqt.exe'
if (!(Test-Path $deploy)) { $deploy = Join-Path $qt 'windeployqt6.exe' }
if (!(Test-Path $deploy)) { throw "Missing windeployqt: $deploy" }
$exe = Join-Path $build 'MyQuant.exe'
if (!(Test-Path $exe)) { throw "Build MyQuant.exe first: $exe" }
$version = (Get-Item $exe).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Missing executable version metadata' }
$dist = Join-Path $repo 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
# Every run stages a new directory. Never copy a build tree or runtime folder.
$stage = Join-Path $dist ('MyQuant-' + $version + '-windows-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $exe -Destination $stage
Copy-Item -LiteralPath (Join-Path $repo 'resources/scripts'),(Join-Path $repo 'resources/seed') -Destination (New-Item -ItemType Directory -Path (Join-Path $stage 'resources')).FullName -Recurse
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE'),(Join-Path $repo 'NOTICE.md') -Destination $stage
$oldPath = $env:PATH
try {
    $env:PATH = $qt + ';' + (Join-Path $qt '../share/qt6/bin') + ';' + $oldPath
    & $deploy (Join-Path $stage 'MyQuant.exe') --release --qmldir (Join-Path $repo 'qml') --compiler-runtime --verbose 0
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed: $LASTEXITCODE" }
} finally { $env:PATH = $oldPath }
# MSYS2 Qt depends on additional libraries that windeployqt does not collect.
# Resolve the import closure from this kit only, never from arbitrary PATHs.
$objdump = Join-Path $qt 'objdump.exe'
if (Test-Path $objdump) {
    $scanned = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    do {
        $added = $false
        $binaries = Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in '.dll','.exe' }
        foreach ($binary in $binaries) {
            if (!$scanned.Add($binary.FullName)) { continue }
            $imports = & $objdump -p $binary.FullName
            if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $($binary.FullName)" }
            foreach ($line in $imports) {
                if ($line -match 'DLL Name:\s*(\S+)') {
                    $name = $Matches[1]
                    $source = Join-Path $qt $name
                    $target = Join-Path $stage $name
                    if ((Test-Path -LiteralPath $source) -and !(Test-Path -LiteralPath $target)) {
                        Copy-Item -LiteralPath $source -Destination $target
                        $added = $true
                    }
                }
            }
        }
    } while ($added)
}
if (!(Test-Path (Join-Path $stage 'platforms/qwindows.dll'))) { throw 'Missing Windows platform plugin' }
if (!(Test-Path (Join-Path $stage 'sqldrivers/qsqlite.dll'))) { throw 'Missing SQLite plugin' }
Set-Content -LiteralPath (Join-Path $stage 'qt.conf') -Encoding ascii -Value "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml`nTranslations=translations"
$archive = Join-Path $dist ('MyQuant-' + $version + '-windows.zip')
if (Test-Path $archive) { throw "Archive already exists; move it before packaging again: $archive" }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive
Get-FileHash -Algorithm SHA256 -LiteralPath $archive
Write-Output "Staged application: $stage"
Write-Output "Release archive: $archive"
