param(
    [Parameter(Mandatory=$true)][string]$Archive,
    [Parameter(Mandatory=$true)][string]$InstallRoot
)
$ErrorActionPreference = 'Stop'
$zip = (Resolve-Path -LiteralPath $Archive).Path
$root = [IO.Path]::GetFullPath($InstallRoot)
foreach ($dataRoot in @((Join-Path $env:LOCALAPPDATA 'MyQuant'), $env:MYQUANT_HOME)) {
    if ([string]::IsNullOrWhiteSpace($dataRoot)) { continue }
    $protected = [IO.Path]::GetFullPath($dataRoot).TrimEnd('\','/')
    if ($root.TrimEnd('\','/').Equals($protected, [StringComparison]::OrdinalIgnoreCase) -or
        $root.StartsWith($protected + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'InstallRoot must be outside the runtime data directory'
    }
}
if ([IO.Path]::GetExtension($zip) -ne '.zip') { throw 'Expected a MyQuant release ZIP' }
New-Item -ItemType Directory -Force -Path $root | Out-Null
$destination = Join-Path $root ('MyQuant-' + [guid]::NewGuid().ToString('N'))
# Install side by side; never replace a running binary or touch the data folder.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$reader = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    foreach ($entry in $reader.Entries) {
        $target = [IO.Path]::GetFullPath((Join-Path $destination $entry.FullName))
        if (!$target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Archive contains an unsafe path'
        }
        if ($entry.FullName -match '(^|[/\\])(data|etf|cache|exports|backups|logs)([/\\]|$)|\.(db|sqlite|sqlite3)$') {
            throw 'Archive contains runtime data'
        }
    }
} finally { $reader.Dispose() }
Expand-Archive -LiteralPath $zip -DestinationPath $destination
$exe = Join-Path $destination 'MyQuant.exe'
if (!(Test-Path $exe)) { throw 'Archive does not contain MyQuant.exe' }
Write-Output "Installed: $exe"
Write-Output 'Close MyQuant before opening the new executable. Keep the previous directory for rollback.'
Write-Output 'Runtime data remains in %LOCALAPPDATA%\MyQuant.'
