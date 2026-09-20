$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$work = Join-Path $root '.work'
$name = 'dosbox-x-vsbuild-win64-2026.08.31-portable.zip'
$url = "https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v2026.08.31/$name"
$expected = '4A22D27FD49B81E1321494311F9CCCB2B617A9BE35367B60D83E0CF2BCA39C8B'
New-Item -ItemType Directory -Force $work | Out-Null
$archive = Join-Path $work $name
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri $url -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
    throw "DOSBox-X archive checksum mismatch: $archive"
}
$destination = Join-Path $work 'dosbox'
if (-not (Test-Path -LiteralPath $destination)) {
    Expand-Archive -LiteralPath $archive -DestinationPath $destination
}
$exe = Join-Path $destination 'bin/x64/Release/dosbox-x.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "DOSBox-X executable missing: $exe" }
Write-Output $exe
