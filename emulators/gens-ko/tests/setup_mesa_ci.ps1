#requires -Version 5.1
# Test-only Mesa deployment. Never installs a system driver or modifies the
# release probe directory. RuntimeDirectory is a disposable copy for CI use.
# Required upstream MSVC x86 files: opengl32.dll and libgallium_wgl.dll.
# Do not include RuntimeDirectory or its DLLs in a release archive.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$ProbeDirectory,
    [Parameter(Mandatory = $true)] [string]$RuntimeDirectory,
    [string]$ReportPath = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$mesaUrl = 'https://github.com/pal1000/mesa-dist-win/releases/download/26.3.0/mesa3d-26.3.0-release-msvc.7z'
$mesaExpected = 'c505fcc77e8f63a09d7ddac086591d2f542c0698d37fc9e3094685a34d0d9991'
$mesaDownload = $null
$mesaReport = [ordered]@{
    status = 'NOT_RUN'; version = '26.3.0'; package = 'MSVC x86'
    source_url = $mesaUrl; expected_archive_sha256 = $mesaExpected
    purpose = 'Temporary Windows CI software-OpenGL validation only; not shipped'
    system_driver_installed = $false; release_directory_modified = $false
    runtime_files = @(); started_utc = [DateTime]::UtcNow.ToString('o')
}
function Get-PEMachine([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) { return 0 }
        $stream.Position = 0x3c
        $offset = $reader.ReadUInt32()
        if ($offset + 6 -gt $stream.Length) { return 0 }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x00004550) { return 0 }
        return $reader.ReadUInt16()
    } finally { $reader.Dispose(); $stream.Dispose() }
}
try {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Mesa CI setup requires Windows.' }
    $sourceDirectory = (Resolve-Path -LiteralPath $ProbeDirectory).Path.TrimEnd('\', '/')
    $runtimeDirectoryFull = [System.IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd('\', '/')
    if ($runtimeDirectoryFull.Equals($sourceDirectory, [StringComparison]::OrdinalIgnoreCase) -or
        $runtimeDirectoryFull.StartsWith($sourceDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'RuntimeDirectory must be separate from the release probe folder.'
    }
    $sourceExe = Join-Path $sourceDirectory 'Gens_KO_Settings_Probe.exe'
    if (-not (Test-Path -LiteralPath $sourceExe -PathType Leaf) -or (Get-PEMachine $sourceExe) -ne 0x14c) {
        throw 'ProbeDirectory must contain the 32-bit Gens_KO_Settings_Probe.exe; no emulator release is modified.'
    }
    if ((Test-Path -LiteralPath $runtimeDirectoryFull) -and @(Get-ChildItem -LiteralPath $runtimeDirectoryFull -Force).Count -gt 0) {
        throw 'RuntimeDirectory must be empty so stale OpenGL DLLs cannot affect the result.'
    }
    $sevenZip = Get-Command '7z.exe' -ErrorAction SilentlyContinue
    if ($null -eq $sevenZip) {
        $candidate = Join-Path ${env:ProgramFiles} '7-Zip/7z.exe'
        if (-not (Test-Path -LiteralPath $candidate)) { throw '7z.exe is required (included on Windows 2022 GitHub runners).' }
        $sevenZipPath = $candidate
    } else { $sevenZipPath = $sevenZip.Source }
    $mesaDownload = Join-Path ([System.IO.Path]::GetTempPath()) ('gens-mesa-download-' + [Guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($mesaDownload)
    $archive = Join-Path $mesaDownload 'mesa3d-26.3.0-release-msvc.7z'
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri $mesaUrl -OutFile $archive -TimeoutSec 180
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    $mesaReport['archive_sha256'] = $actual
    if ($actual -ne $mesaExpected) { throw 'Mesa archive SHA-256 mismatch. No DLL was installed.' }
    $unpacked = Join-Path $mesaDownload 'unpacked'
    $unpackLog = Join-Path $mesaDownload '7zip.log'
    & $sevenZipPath x '-y' ('-o' + $unpacked) $archive > $unpackLog 2>&1
    if ($LASTEXITCODE -ne 0) { throw ('Mesa extraction failed with code ' + $LASTEXITCODE) }
    $openglCandidates = @(Get-ChildItem -LiteralPath $unpacked -Recurse -File -Filter 'opengl32.dll' |
        Where-Object { (Get-PEMachine $_.FullName) -eq 0x14c })
    if ($openglCandidates.Count -ne 1) { throw 'The verified archive does not contain exactly one x86 opengl32.dll runtime.' }
    $mesaFolder = $openglCandidates[0].DirectoryName
    $gallium = Join-Path $mesaFolder 'libgallium_wgl.dll'
    if (-not (Test-Path -LiteralPath $gallium -PathType Leaf) -or (Get-PEMachine $gallium) -ne 0x14c) {
        throw 'Matching x86 libgallium_wgl.dll is missing from the discovered Mesa runtime folder.'
    }
    $mesaReport['archive_runtime_folder'] = $mesaFolder.Substring($unpacked.Length).TrimStart('\', '/')
    [void][System.IO.Directory]::CreateDirectory($runtimeDirectoryFull)
    Get-ChildItem -LiteralPath $sourceDirectory -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $runtimeDirectoryFull -Recurse -Force
    }
    foreach ($file in @($openglCandidates[0].FullName, $gallium)) {
        $name = [System.IO.Path]::GetFileName($file)
        Copy-Item -LiteralPath $file -Destination (Join-Path $runtimeDirectoryFull $name) -Force
        $mesaReport['runtime_files'] += [ordered]@{
            file = $name; machine = 'IMAGE_FILE_MACHINE_I386'
            sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
            bytes = (Get-Item -LiteralPath $file).Length
        }
    }
    $env:GALLIUM_DRIVER = 'llvmpipe'
    $env:LIBGL_ALWAYS_SOFTWARE = '1'
    $mesaReport['environment'] = @{ GALLIUM_DRIVER = 'llvmpipe'; LIBGL_ALWAYS_SOFTWARE = '1' }
    $mesaReport['runtime_probe_directory'] = $runtimeDirectoryFull
    $mesaReport['probe_exe_sha256'] = (Get-FileHash -LiteralPath $sourceExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $mesaReport['status'] = 'PREPARED'
    $mesaReport['gpu_execution_verified'] = $false
    $mesaReport['finished_utc'] = [DateTime]::UtcNow.ToString('o')
    $json = $mesaReport | ConvertTo-Json -Depth 8
    $json | Set-Content -LiteralPath (Join-Path $runtimeDirectoryFull 'CI_MESA_RUNTIME.json') -Encoding UTF8
    if ($ReportPath) {
        $fullReport = [System.IO.Path]::GetFullPath($ReportPath)
        [void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($fullReport))
        $json | Set-Content -LiteralPath $fullReport -Encoding UTF8
    }
    Write-Output $json
} catch {
    $mesaReport['status'] = 'FAIL'; $mesaReport['reason'] = $_.Exception.Message
    if ($ReportPath) {
        $fullReport = [System.IO.Path]::GetFullPath($ReportPath)
        [void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($fullReport))
        $mesaReport | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $fullReport -Encoding UTF8
    }
    throw
} finally {
    if ($null -ne $mesaDownload -and (Test-Path -LiteralPath $mesaDownload)) {
        Remove-Item -LiteralPath $mesaDownload -Recurse -Force -ErrorAction SilentlyContinue
    }
}
