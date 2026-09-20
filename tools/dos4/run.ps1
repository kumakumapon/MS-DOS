param(
    [string]$Work = '.work/build',
    [string]$Distro = 'Ubuntu-24.04',
    [string]$DosBox,
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    if (-not $DosBox) { $DosBox = & "$PSScriptRoot/get-dosbox.ps1" }
    $Work = [IO.Path]::GetFullPath($Work)
    if (-not $Resume) {
        python "$PSScriptRoot/build.py" prepare --work $Work
        if ($LASTEXITCODE -ne 0) { throw 'Source preparation failed' }
    }
    python "$PSScriptRoot/build.py" build --work $Work --dosbox $DosBox
    if ($LASTEXITCODE -ne 0) { throw "DOS build failed; inspect $Work/v4.0/src/BUILD.LOG" }
    python "$PSScriptRoot/image.py" --work $Work
    if ($LASTEXITCODE -ne 0) { throw 'Image creation failed' }
    $scriptPath = "$PSScriptRoot/verify.py".Replace('\', '/')
    $workPath = $Work.Replace('\', '/')
    $linuxScript = (& wsl -d $Distro --exec wslpath -a $scriptPath).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not resolve verifier path in WSL' }
    $linuxWork = (& wsl -d $Distro --exec wslpath -a $workPath).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not resolve build path in WSL' }
    & wsl -d $Distro --exec python3 $linuxScript --work $linuxWork
    if ($LASTEXITCODE -ne 0) { throw "QEMU verification failed; inspect $Work/qemu.log" }
    Write-Output "Build and QEMU verification passed: $Work/msdos4-boot.img"
} finally {
    Pop-Location
}
