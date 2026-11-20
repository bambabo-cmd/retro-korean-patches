# Gens KO Windows UI smoke/capture harness. GPL-2.0, 2026-11-20.
#requires -Version 5.1
# Runs ONLY a generated original homebrew cartridge. No commercial ROM needed.
# Examples (run from an interactive Windows desktop with Python installed):
#   powershell -File tests/windows_smoke.ps1 -ExeDirectory build/bin -OutputDirectory smoke-output
#   powershell -File tests/windows_smoke.ps1 -Archive release.zip -ArchiveSha256 <64 hex digits> -OutputDirectory smoke-output
#   powershell -File tests/windows_smoke.ps1 -SettingsProbe -ExeDirectory build/settings-probe -OutputDirectory probe-captures
#   powershell -File tests/windows_smoke.ps1 -SettingsProbe -IncludeShaderSettings -ExeDirectory build/shader_probe -OutputDirectory shader-captures
#   powershell -File tests/windows_smoke.ps1 -SettingsProbe -RealCrt -ExeDirectory temporary-mesa-probe -OutputDirectory real-crt-proof
# Exit codes: 0 PASS (this limited smoke test); 1 FAIL; 2 UNSUPPORTED desktop.
# Output JSON explicitly distinguishes screenshots/GUI startup from game QA.
[CmdletBinding(DefaultParameterSetName = 'Directory')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Directory')]
    [string]$ExeDirectory,
    [Parameter(Mandatory = $true, ParameterSetName = 'Archive')]
    [string]$Archive,
    [Parameter(Mandatory = $true, ParameterSetName = 'Archive')]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ArchiveSha256,
    [string]$OutputDirectory = 'windows-smoke-output',
    [string]$PythonExecutable = 'python',
    [switch]$SettingsProbe,
    [switch]$IncludeShaderSettings,
    [switch]$RealCrt,
    [ValidateRange(5, 60)] [int]$WindowTimeoutSeconds = 30
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$smokeExit = 1
$smokeProcess = $null
$smokeWork = $null
$savedGalliumDriver = $env:GALLIUM_DRIVER
$savedSoftwareGl = $env:LIBGL_ALWAYS_SOFTWARE
if ($RealCrt) { $IncludeShaderSettings = $true }
$smokeOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[void][System.IO.Directory]::CreateDirectory($smokeOutput)
$smokeReport = [ordered]@{
    status = 'NOT_RUN'
    scope = 'Windows executable startup, generated homebrew display, two real settings dialogs, clean exit'
    game = 'Original Gens KO homebrew UI smoke card; NOT Pier Solar'
    commercial_rom_included = $false
    full_gameplay_verified = $false
    audio_verified = $false
    physical_gamepads_verified = $false
    multi_monitor_verified = $false
    screenshot_method = 'Actual foreground window, System.Drawing.Graphics.CopyFromScreen'
    shader_settings_requested = [bool]$IncludeShaderSettings
    real_crt_requested = [bool]$RealCrt
    screenshots = @()
    started_utc = [DateTime]::UtcNow.ToString('o')
}
if ($SettingsProbe) {
    $smokeReport['scope'] = 'Production settings dialogs in an isolated UI probe; NOT the full emulator'
    $smokeReport['game'] = 'None; settings probe contains no game or emulation core'
    $smokeReport['emulator_startup_verified'] = $false
}
if ($IncludeShaderSettings) {
    $smokeReport['scope'] += '; includes the actual shader settings dialog'
    $smokeReport['shader_settings_captured'] = $false
    $smokeReport['shader_gameplay_verified'] = $false
}
if ($RealCrt) {
    $smokeReport['scope'] += '; verifies actual Real CRT 14-pass OpenGL output and parameter dialog on an owned pattern'
    $smokeReport['actual_glsl_pipeline_verified'] = $false
    $smokeReport['shader_parameters_captured'] = $false
    $smokeReport['software_opengl_verified'] = $false
}

function Write-SmokeReport {
    $smokeReport['finished_utc'] = [DateTime]::UtcNow.ToString('o')
    $smokeReport | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $smokeOutput 'windows-smoke.json') -Encoding UTF8
}
function Unsupported([string]$Reason) { throw ('UNSUPPORTED: ' + $Reason) }

try {
    if ($RealCrt -and -not $SettingsProbe) { throw '-RealCrt requires -SettingsProbe; it never claims a full game test.' }
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        Unsupported 'This test requires Windows; cross-compilation is not UI execution.'
    }
    if (-not [Environment]::UserInteractive) {
        Unsupported 'No interactive Windows user desktop is available.'
    }
    Add-Type -AssemblyName System.Drawing
    Add-Type -ReferencedAssemblies @('System.Drawing', 'System.Core') -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
public static class GensSmokeWin32 {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
    public sealed class WindowInfo { public long Handle; public string ClassName; public string Title; }
    public sealed class CaptureInfo {
        public string Path; public int Width, Height, SampledColors; public bool ClientOnly;
    }
    public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref Point point);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr window, uint flags);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string className, string title);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
    [DllImport("user32.dll")] public static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    public static void PrepareDpi() {
        try { SetThreadDpiAwarenessContext(new IntPtr(-4)); }
        catch(EntryPointNotFoundException) { SetProcessDPIAware(); }
    }
    public static bool HasInputDesktop() {
        IntPtr desktop=OpenInputDesktop(0,false,1);
        if(desktop==IntPtr.Zero) return false;
        CloseDesktop(desktop);return true;
    }
    public static WindowInfo[] Windows(int processId) {
        var windows=new List<WindowInfo>();
        EnumWindows(delegate(IntPtr handle,IntPtr parameter) {
            uint owner;GetWindowThreadProcessId(handle,out owner);
            if(owner==(uint)processId && IsWindowVisible(handle)) {
                var klass=new StringBuilder(256);var title=new StringBuilder(1024);
                GetClassName(handle,klass,klass.Capacity);GetWindowText(handle,title,title.Capacity);
                windows.Add(new WindowInfo {Handle=handle.ToInt64(),ClassName=klass.ToString(),Title=title.ToString()});
            }
            return true;
        },IntPtr.Zero);
        return windows.ToArray();
    }
    public static WindowInfo Preview(IntPtr parent) {
        IntPtr window=FindWindowEx(parent,IntPtr.Zero,"GensKoShaderPreview",null);
        if(window==IntPtr.Zero||!IsWindowVisible(window)) throw new InvalidOperationException("The actual renderer preview child window is unavailable.");
        var title=new StringBuilder(1024);GetWindowText(window,title,title.Capacity);
        return new WindowInfo {Handle=window.ToInt64(),ClassName="GensKoShaderPreview",Title=title.ToString()};
    }
    public static CaptureInfo Capture(IntPtr window,string path,bool clientOnly) {
        if(!IsWindow(window)||!IsWindowVisible(window)||IsIconic(window)) throw new InvalidOperationException("Target window is not visible.");
        if(GetForegroundWindow()!=GetAncestor(window,2)) throw new InvalidOperationException("UNSUPPORTED: Cannot capture the target as the foreground window.");
        Rect rect;
        if(clientOnly) {
            if(!GetClientRect(window,out rect)) throw new InvalidOperationException("GetClientRect failed.");
            Point origin=new Point();if(!ClientToScreen(window,ref origin)) throw new InvalidOperationException("ClientToScreen failed.");
            rect.right+=origin.x;rect.bottom+=origin.y;rect.left=origin.x;rect.top=origin.y;
        } else if(!GetWindowRect(window,out rect)) throw new InvalidOperationException("GetWindowRect failed.");
        int width=rect.right-rect.left,height=rect.bottom-rect.top;
        Rectangle desktop=new Rectangle(GetSystemMetrics(76),GetSystemMetrics(77),GetSystemMetrics(78),GetSystemMetrics(79));
        if(width<32||height<32||width>16384||height>16384||rect.left<desktop.Left||rect.top<desktop.Top||rect.right>desktop.Right||rect.bottom>desktop.Bottom)
            throw new InvalidOperationException("UNSUPPORTED: Window does not fit the visible desktop for an exact capture.");
        using(var bitmap=new Bitmap(width,height,PixelFormat.Format32bppArgb)) {
            using(var graphics=Graphics.FromImage(bitmap)) graphics.CopyFromScreen(rect.left,rect.top,0,0,new Size(width,height),CopyPixelOperation.SourceCopy);
            var colors=new HashSet<int>();
            for(int y=0;y<height;y+=Math.Max(1,height/180)) for(int x=0;x<width;x+=Math.Max(1,width/240)) colors.Add(bitmap.GetPixel(x,y).ToArgb());
            if(colors.Count<2) throw new InvalidOperationException("UNSUPPORTED: Screen capture is blank or desktop rendering is unavailable.");
            bitmap.Save(path,ImageFormat.Png);
            return new CaptureInfo {Path=path,Width=width,Height=height,SampledColors=colors.Count,ClientOnly=clientOnly};
        }
    }
}
'@
    [GensSmokeWin32]::PrepareDpi()
    if (-not [GensSmokeWin32]::HasInputDesktop()) {
        Unsupported 'The input desktop cannot be accessed (service, locked, or disconnected desktop).'
    }
    $smokeWork = Join-Path ([System.IO.Path]::GetTempPath()) ('gens-ko-smoke-' + [Guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($smokeWork)
    $smokeRun = Join-Path $smokeWork 'run'
    [void][System.IO.Directory]::CreateDirectory($smokeRun)
    $exeName = if ($SettingsProbe) { 'Gens_KO_Settings_Probe.exe' } else { 'Gens_KO.exe' }
    if ($PSCmdlet.ParameterSetName -eq 'Archive') {
        $archivePath = (Resolve-Path -LiteralPath $Archive).Path
        $actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $ArchiveSha256.ToLowerInvariant()) { throw 'Release archive SHA-256 does not match the expected value.' }
        $smokeReport['archive_sha256'] = $actualHash
        $unpacked = Join-Path $smokeWork 'unpacked'
        Expand-Archive -LiteralPath $archivePath -DestinationPath $unpacked
        $candidates = @(Get-ChildItem -LiteralPath $unpacked -Recurse -File -Filter $exeName)
        if ($candidates.Count -ne 1) { throw ('Expected exactly one ' + $exeName + ' in the verified Windows archive.') }
        $sourceDirectory = $candidates[0].DirectoryName
    } else {
        $sourceDirectory = (Resolve-Path -LiteralPath $ExeDirectory).Path
        if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory $exeName) -PathType Leaf)) { throw ('ExeDirectory does not contain ' + $exeName + '.') }
    }
    # The running copy has its own config/saves; never change the supplied release.
    Get-ChildItem -LiteralPath $sourceDirectory -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $smokeRun -Recurse -Force
    }
    $exe = Join-Path $smokeRun $exeName
    $smokeReport['exe_sha256'] = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    @'
[General]
Intro Style=0
Priority=1
[Window KO]
X=40
Y=40
Width=760
Height=600
[Graphics]
Full Screen=0
Frame skip=-1
Windows VSync=0
[Options]
Auto Pause=0
Message=1
[Sound]
State=0
'@ | Set-Content -LiteralPath (Join-Path $smokeRun 'Gens.cfg') -Encoding Unicode
    if ($RealCrt) {
        # A copied user's shader or GDI preference cannot silently change the
        # pinned fixture. Only this private running copy is reset.
        @'
[Video]
KeepAspect=1
IntegerScale=0
GDI=0
Shader=
'@ | Set-Content -LiteralPath (Join-Path $smokeRun 'Gens-KO-video.ini') -Encoding Unicode
        foreach ($stale in @('Gens-KO-shaders.ini', 'real-crt-probe.json', 'real-crt-probe.json.tmp', 'Gens-KO-renderer.log')) {
            $staleFile = Join-Path $smokeRun $stale
            if (Test-Path -LiteralPath $staleFile) { Remove-Item -LiteralPath $staleFile -Force }
        }
        $mesaMarker = Join-Path $smokeRun 'CI_MESA_RUNTIME.json'
        if (Test-Path -LiteralPath $mesaMarker) {
            $mesaRuntime = Get-Content -LiteralPath $mesaMarker -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($mesaRuntime.status -ne 'PREPARED') { throw 'The supplied Mesa runtime was not prepared successfully.' }
            foreach ($runtimeFile in $mesaRuntime.runtime_files) {
                $runtimePath = Join-Path $smokeRun $runtimeFile.file
                if ((Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $runtimeFile.sha256) {
                    throw ('Mesa runtime DLL changed after preparation: ' + $runtimeFile.file)
                }
            }
            $env:GALLIUM_DRIVER = 'llvmpipe'
            $env:LIBGL_ALWAYS_SOFTWARE = '1'
            $smokeReport['mesa_runtime'] = $mesaRuntime
        }
    }
    if ($SettingsProbe) {
        if ($RealCrt) {
            $smokeProcess = Start-Process -FilePath $exe -WorkingDirectory $smokeRun -ArgumentList '--real-crt' -PassThru
        } else {
            $smokeProcess = Start-Process -FilePath $exe -WorkingDirectory $smokeRun -PassThru
        }
    } else {
        $rom = Join-Path $smokeWork 'Gens_KO_original_smoke.bin'
        $generator = Join-Path $PSScriptRoot 'make_smoke_rom.py'
        $generation = (& $PythonExecutable $generator $rom | Out-String)
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $rom)) { throw 'Could not generate the original smoke-test ROM.' }
        $smokeReport['homebrew'] = $generation | ConvertFrom-Json
        $smokeProcess = Start-Process -FilePath $exe -WorkingDirectory $smokeRun -ArgumentList @('--windowed', '--hide-menu', ('"' + $rom + '"')) -PassThru
    }
    $smokeReport['process_id'] = $smokeProcess.Id

    function Assert-Alive {
        $smokeProcess.Refresh()
        if ($smokeProcess.HasExited) { throw ('Emulator exited unexpectedly: ' + $smokeProcess.ExitCode) }
    }
    function Wait-SmokeWindow([string]$ClassName, [long]$DifferentFrom = 0, [string]$TitleContains = '') {
        $deadline = [DateTime]::UtcNow.AddSeconds($WindowTimeoutSeconds)
        do {
            Assert-Alive
            $matches = @([GensSmokeWin32]::Windows($smokeProcess.Id) | Where-Object {
                $_.ClassName -eq $ClassName -and $_.Handle -ne $DifferentFrom -and $_.Title.Contains($TitleContains)
            })
            if ($matches.Count -eq 1) { return $matches[0] }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        $visible = [GensSmokeWin32]::Windows($smokeProcess.Id) | ConvertTo-Json -Depth 4 -Compress
        throw ('Timed out waiting for window class ' + $ClassName + '; visible windows: ' + $visible)
    }
    function Capture-SmokeWindow($Window, [string]$Name, [bool]$ClientOnly = $false) {
        $handle = [IntPtr]$Window.Handle
        $owner = [GensSmokeWin32]::GetAncestor($handle, 2)
        [void][GensSmokeWin32]::ShowWindow($owner, 9)
        [void][GensSmokeWin32]::BringWindowToTop($owner)
        [void][GensSmokeWin32]::SetForegroundWindow($owner)
        $deadline = [DateTime]::UtcNow.AddSeconds(3)
        while ([GensSmokeWin32]::GetForegroundWindow() -ne $owner -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 100
        }
        Start-Sleep -Milliseconds 500
        Assert-Alive
        $file = Join-Path $smokeOutput $Name
        $capture = [GensSmokeWin32]::Capture($handle, $file, $ClientOnly)
        $smokeReport['screenshots'] += [ordered]@{
            file = $Name; title = $Window.Title; class_name = $Window.ClassName
            sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
            width = $capture.Width; height = $capture.Height; sampled_colors = $capture.SampledColors
            client_only = $ClientOnly; actual_window_capture = $true
        }
    }
    function Close-SmokeDialog($Window) {
        $handle = [IntPtr]$Window.Handle
        if (-not [GensSmokeWin32]::PostMessage($handle, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)) { throw 'WM_CLOSE could not be posted to the settings dialog.' }
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        while ([GensSmokeWin32]::IsWindow($handle) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100; Assert-Alive }
        if ([GensSmokeWin32]::IsWindow($handle)) { throw 'Settings dialog did not close within five seconds.' }
    }
    function Assert-RealCrtProof($Proof, [bool]$RequireClosed = $false) {
        if ($Proof.status -ne 'PASS') { throw ('Real CRT renderer proof is not PASS: ' + $Proof.status + '; ' + $Proof.diagnostic) }
        if ($Proof.process_id -ne $smokeProcess.Id) { throw 'Renderer proof belongs to a different or stale process.' }
        if ($Proof.requested_passes -ne 14 -or $Proof.parsed_passes -ne 14 -or $Proof.active_passes -ne 14 -or
            $Proof.parsed_feedback_pass -ne 6 -or $Proof.active_feedback_pass -ne 6 -or
            $Proof.parsed_float_passes -le 0 -or $Proof.parameter_count -le 0) {
            throw 'The actual renderer is not running the required 14-pass, float-framebuffer, feedback Real CRT preset.'
        }
        if (-not $Proof.preset_load_succeeded -or $Proof.verified_presentations -lt 3 -or
            $Proof.gl_error -ne 0 -or $Proof.framebuffer_sampled_colors -lt 8 -or
            -not $Proof.shader_output_differs_from_baseline -or
            -not $Proof.gl_vendor -or -not $Proof.gl_renderer -or -not $Proof.gl_version -or
            $Proof.renderer_status -notlike 'OpenGL *GLSL 14*') {
            throw 'Actual OpenGL presentation/readback did not pass; compilation or GDI fallback alone is insufficient.'
        }
        if ($RequireClosed -and -not $Proof.closed_normally) { throw 'Renderer proof does not confirm a normal close.' }
        if ($smokeReport.Contains('mesa_runtime') -and $Proof.gl_renderer -notmatch '(?i)llvmpipe') {
            throw 'Mesa was prepared, but the active renderer is not the requested llvmpipe software OpenGL device.'
        }
    }
    function Wait-RealCrtProof {
        # Software shader compilation can take longer than window creation.
        # The bounded poll retains startup failures instead of timing out silently.
        $proofPath = Join-Path $smokeRun 'real-crt-probe.json'
        $deadline = [DateTime]::UtcNow.AddSeconds(120)
        do {
            Assert-Alive
            if (Test-Path -LiteralPath $proofPath) {
                $proof = Get-Content -LiteralPath $proofPath -Raw -Encoding UTF8 | ConvertFrom-Json
                if ($proof.status -eq 'FAIL') { throw ('Real CRT renderer failed: ' + $proof.diagnostic) }
                if ($proof.status -eq 'PASS') { Assert-RealCrtProof $proof; return $proof }
                if ($proof.status -ne 'PENDING') { throw 'Renderer proof contains an unknown status.' }
            }
            Start-Sleep -Milliseconds 150
        } while ([DateTime]::UtcNow -lt $deadline)
        throw 'Real CRT did not complete three verified OpenGL presentations within 120 seconds.'
    }
    $mainClass = if ($SettingsProbe) { 'GensKoSettingsProbe' } else { 'Gens KO' }
    $main = Wait-SmokeWindow $mainClass
    if ($RealCrt) {
        [void][GensSmokeWin32]::BringWindowToTop([IntPtr]$main.Handle)
        [void][GensSmokeWin32]::SetForegroundWindow([IntPtr]$main.Handle)
        $smokeReport['renderer_initial_proof'] = Wait-RealCrtProof
    }
    Start-Sleep -Seconds 2
    Assert-Alive
    $main = Wait-SmokeWindow $mainClass
    $unexpectedDialogs = @([GensSmokeWin32]::Windows($smokeProcess.Id) | Where-Object { $_.ClassName -eq '#32770' })
    if ($unexpectedDialogs.Count -gt 0) { throw ('Unexpected startup dialog: ' + ($unexpectedDialogs | ConvertTo-Json -Compress)) }
    if ($SettingsProbe) {
        Capture-SmokeWindow $main '01-settings-probe.png'
        if ($RealCrt) {
            $preview = [GensSmokeWin32]::Preview([IntPtr]$main.Handle)
            Capture-SmokeWindow $preview '02-real-crt-preview.png' $true
        }
    } else {
        Capture-SmokeWindow $main '01-main-homebrew.png'
        Capture-SmokeWindow $main '02-homebrew-client.png' $true
        $smokeReport['emulator_startup_verified'] = $true
    }
    if (-not [GensSmokeWin32]::PostMessage([IntPtr]$main.Handle, 0x111, [IntPtr]40183, [IntPtr]::Zero)) { throw 'Could not request controller settings.' }
    $inputTitle = -join @([char]0xD0A4, [char]0xBCF4, [char]0xB4DC)
    $inputDialog = Wait-SmokeWindow '#32770' $main.Handle $inputTitle
    Capture-SmokeWindow $inputDialog '03-keyboard-gamepad-settings.png'
    Close-SmokeDialog $inputDialog
    if (-not [GensSmokeWin32]::PostMessage([IntPtr]$main.Handle, 0x111, [IntPtr]57005, [IntPtr]::Zero)) { throw 'Could not request transport hotkey settings.' }
    $hotkeyDialog = Wait-SmokeWindow 'GensKOPlaybackSettings'
    Capture-SmokeWindow $hotkeyDialog '04-hotkey-speed-settings.png'
    Close-SmokeDialog $hotkeyDialog
    if ($IncludeShaderSettings) {
        if (-not [GensSmokeWin32]::PostMessage([IntPtr]$main.Handle, 0x111, [IntPtr]57001, [IntPtr]::Zero)) { throw 'Could not request shader settings.' }
        $shaderDialog = Wait-SmokeWindow 'GensKoVideoSettings'
        Capture-SmokeWindow $shaderDialog '05-shader-settings.png'
        if ($RealCrt) {
            if (-not [GensSmokeWin32]::PostMessage([IntPtr]$shaderDialog.Handle, 0x111, [IntPtr]5110, [IntPtr]::Zero)) {
                throw 'Could not request actual loaded shader parameters.'
            }
            $parametersDialog = Wait-SmokeWindow 'GensKoShaderParameters'
            Capture-SmokeWindow $parametersDialog '06-real-crt-parameters.png'
            Close-SmokeDialog $parametersDialog
            $smokeReport['shader_parameters_captured'] = $true
        }
        Close-SmokeDialog $shaderDialog
        $smokeReport['shader_settings_captured'] = $true
    }
    Assert-Alive
    [void][GensSmokeWin32]::PostMessage([IntPtr]$main.Handle, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $smokeProcess.WaitForExit(10000)) { throw 'Emulator did not exit normally within ten seconds.' }
    $smokeReport['process_exit_code'] = $smokeProcess.ExitCode
    if ($smokeProcess.ExitCode -ne 0) { throw ('Emulator exited with nonzero code: ' + $smokeProcess.ExitCode) }
    if ($RealCrt) {
        $finalProof = Get-Content -LiteralPath (Join-Path $smokeRun 'real-crt-probe.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        Assert-RealCrtProof $finalProof $true
        $smokeReport['renderer_final_proof'] = $finalProof
        $smokeReport['actual_glsl_pipeline_verified'] = $true
        $smokeReport['software_opengl_verified'] = $finalProof.gl_renderer -match '(?i)llvmpipe'
    }
    $smokeReport['status'] = 'PASS'
    $smokeReport['clean_exit'] = $true
    $smokeExit = 0
} catch {
    $reason = $_.Exception.Message
    $smokeReport['reason'] = $reason
    if ($reason -like '*UNSUPPORTED:*') { $smokeReport['status'] = 'UNSUPPORTED'; $smokeExit = 2 }
    else { $smokeReport['status'] = 'FAIL'; $smokeExit = 1 }
} finally {
    if ($null -ne $smokeProcess) {
        $smokeProcess.Refresh()
        if (-not $smokeProcess.HasExited) {
            # Only terminate the child process created by this harness.
            Stop-Process -Id $smokeProcess.Id -Force -ErrorAction SilentlyContinue
            [void]$smokeProcess.WaitForExit(3000)
            $smokeReport['forced_cleanup'] = $true
        }
    }
    if ($null -ne $smokeWork -and (Test-Path -LiteralPath $smokeWork)) {
        foreach ($evidenceName in @('Gens-KO-startup.log', 'Gens-KO-renderer.log', 'real-crt-probe.json', 'CI_MESA_RUNTIME.json')) {
            $evidence = Join-Path (Join-Path $smokeWork 'run') $evidenceName
            if (Test-Path -LiteralPath $evidence) { Copy-Item -LiteralPath $evidence -Destination (Join-Path $smokeOutput $evidenceName) -Force }
        }
        # All copied program/config/homebrew files are private temporary inputs;
        # public output contains only the exact screenshots, log and report.
        Remove-Item -LiteralPath $smokeWork -Recurse -Force -ErrorAction SilentlyContinue
    }
    $env:GALLIUM_DRIVER = $savedGalliumDriver
    $env:LIBGL_ALWAYS_SOFTWARE = $savedSoftwareGl
    Write-SmokeReport
}
Write-Output ('Windows smoke result: ' + $smokeReport['status'])
Write-Output (Join-Path $smokeOutput 'windows-smoke.json')
exit $smokeExit
