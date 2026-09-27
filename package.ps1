param(
    [string]$OutputDir = "release",
    [string]$ExeName = "WipePDF_C.exe"
)

$ErrorActionPreference = "Stop"

Write-Host "1. Building latest WipePDF..." -ForegroundColor Cyan
& powershell -ExecutionPolicy Bypass -File .\build.ps1 -Clean
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit $LASTEXITCODE
}

Write-Host "2. Syncing dist/ directory..." -ForegroundColor Cyan
if (-not (Test-Path "dist")) {
    New-Item -ItemType Directory -Force -Path "dist" | Out-Null
}
Copy-Item -Force "build\$ExeName" "dist\$ExeName"

Write-Host "3. Packaging portable zip..." -ForegroundColor Cyan
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$targetZip = Join-Path $OutputDir "WipePDF_C_Portable.zip"
if (Test-Path $targetZip) {
    Remove-Item -Force $targetZip
}

Compress-Archive -Path "dist\*" -DestinationPath $targetZip -Force

$sizeMB = [math]::Round(((Get-Item $targetZip).Length / 1MB), 2)
Write-Host "==================================================" -ForegroundColor Green
Write-Host " Portable ZIP package created successfully!" -ForegroundColor Green
Write-Host " Path: $targetZip" -ForegroundColor Green
Write-Host " Size: $sizeMB MB" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Green
