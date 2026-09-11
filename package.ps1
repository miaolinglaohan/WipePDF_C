param(
    [string]$OutputDir = "release",
    [string]$ExeName = "WipePDF.exe"
)

$ErrorActionPreference = "Stop"

$rarExe = "C:\Program Files\WinRAR\Rar.exe"
$winrarExe = "C:\Program Files\WinRAR\WinRAR.exe"
$iconPath = "res\app.ico"

if (-not (Test-Path $rarExe) -or -not (Test-Path $winrarExe)) {
    Write-Error "WinRAR not found at $winrarExe"
    exit 1
}

Write-Host "1. Building latest WipePDF..." -ForegroundColor Cyan
& powershell -ExecutionPolicy Bypass -File .\build.ps1
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit $LASTEXITCODE
}

Write-Host "2. Syncing dist/ directory..." -ForegroundColor Cyan
if (-not (Test-Path "dist")) {
    New-Item -ItemType Directory -Force -Path "dist" | Out-Null
}
Copy-Item -Force "build\WipePDF.exe" "dist\WipePDF.exe"

Write-Host "3. Packaging single standalone executable..." -ForegroundColor Cyan
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$tempSfxConfig = Join-Path $env:TEMP "wipepdf_sfx_config.txt"
@"
TempMode
Setup=WipePDF.exe
Silent=1
Overwrite=1
Title=WipePDF - Stubborn Watermark & Link Remover
"@ | Out-File -Encoding utf8 -FilePath $tempSfxConfig

$targetExe = Join-Path $OutputDir $ExeName
if (Test-Path $targetExe) {
    Remove-Item -Force $targetExe
}

# Create SFX archive using Rar.exe
& $rarExe a -sfx -m5 -r -ep1 -z"$tempSfxConfig" "$targetExe" "dist\*"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Rar packaging failed!"
    exit $LASTEXITCODE
}

# Apply custom purple icon and SFX configuration via WinRAR.exe
Start-Process -FilePath $winrarExe -ArgumentList "c -z`"$tempSfxConfig`" -iicon`"$iconPath`" `"$targetExe`"" -Wait

Remove-Item -Force $tempSfxConfig

$sizeMB = [math]::Round(((Get-Item $targetExe).Length / 1MB), 2)
Write-Host "==================================================" -ForegroundColor Green
Write-Host " Single-file standalone executable created successfully!" -ForegroundColor Green
Write-Host " Path: $targetExe" -ForegroundColor Green
Write-Host " Size: $sizeMB MB" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Green
