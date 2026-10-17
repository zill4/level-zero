# Builds Level Zero Android APKs. Sets env vars for THIS process only (no system changes).
#   -Variant Store     arm64-v8a, PICO OpenXR plugin ON  (store / real PICO headset builds)
#   -Variant Emulator  x86_64,   PICO OpenXR plugin OFF (stock UE OpenXR; PICO's plugin ships an arm64-only loader)
param(
  [ValidateSet('Store','Emulator')][string]$Variant = 'Store',
  [string]$EngineRoot = 'E:\Epic\UE_5.8',
  [string]$AndroidSdk = "$env:LOCALAPPDATA\Android\Sdk",
  [string]$JavaRoot   = "$env:USERPROFILE\.jdks\jbr-21.0.11",
  [ValidateSet('Development','Shipping')][string]$Config = 'Development',
  [switch]$Install,
  [string]$Device = ''
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$project = Join-Path $root 'LevelZero.uproject'
$ndk = Get-ChildItem "$AndroidSdk\ndk" -Directory | Where-Object { $_.Name -match '^(27\.2|28\.|29\.)' } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $ndk) { throw 'Need an Android NDK r27c-r29 (UE 5.8). Expected 27.2.12479018 or 28.x under the SDK ndk folder.' }
if (-not (Test-Path "$JavaRoot\bin\java.exe")) { throw "JDK 17-23 required (UE 5.8 uses Gradle 8.13). Not found: $JavaRoot" }
$env:ANDROID_HOME = $AndroidSdk; $env:ANDROID_SDK_ROOT = $AndroidSdk
$env:NDKROOT = $ndk.FullName; $env:NDK_ROOT = $ndk.FullName; $env:ANDROID_NDK_ROOT = $ndk.FullName
$env:JAVA_HOME = $JavaRoot; $env:PATH = "$JavaRoot\bin;$AndroidSdk\platform-tools;$env:PATH"

$out = Join-Path $root "Packaged\$Variant"
$uatArgs = @('BuildCookRun', "-project=$project", '-platform=Android', '-cookflavor=ASTC', "-clientconfig=$Config",
             '-build', '-cook', '-stage', '-pak', '-package', '-archive', "-archivedirectory=$out", '-nop4', '-utf8output', '-unattended')
if ($Install) { $uatArgs += '-deploy'; if ($Device) { $uatArgs += "-device=Android@$Device" } }

# The launcher engine can't vary plugins/arches per build from the command line (UBT ignores -ini: arch
# overrides), so the emulator variant edits the .uproject (PICOOpenXR off) and DefaultEngine.ini
# (x86_64 instead of arm64) for the duration of the build and always restores both.
$engineIni = Join-Path $root 'Config\DefaultEngine.ini'
$backupProject = $null; $backupIni = $null
if ($Variant -eq 'Emulator') {
  $backupProject = Get-Content $project -Raw
  $backupIni = Get-Content $engineIni -Raw
  ($backupProject -replace '("Name": "PICOOpenXR", "Enabled": )true', '${1}false') | Set-Content $project -NoNewline
  ($backupIni -replace '(?m)^bBuildForArm64=True', 'bBuildForArm64=False' -replace '(?m)^bBuildForX8664=False', 'bBuildForX8664=True') | Set-Content $engineIni -NoNewline
}
try {
  & "$EngineRoot\Engine\Build\BatchFiles\RunUAT.bat" @uatArgs
  $code = $LASTEXITCODE
} finally {
  if ($backupProject) { Set-Content $project $backupProject -NoNewline }
  if ($backupIni) { Set-Content $engineIni $backupIni -NoNewline }
}
Get-ChildItem $out -Recurse -Filter *.apk -ErrorAction SilentlyContinue | ForEach-Object { "APK: $($_.FullName) ($([math]::Round($_.Length/1MB,1)) MB)" }
exit $code
