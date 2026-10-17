# Builds the Level Zero PICO APK. Sets env vars for THIS process only (no system changes).
param(
  [string]$EngineRoot = 'E:\Epic\UE_5.8',
  [string]$AndroidSdk = "$env:LOCALAPPDATA\Android\Sdk",
  [string]$JavaRoot   = "$env:USERPROFILE\.jdks\jbr-21.0.11",
  [ValidateSet('Development','Shipping')][string]$Config = 'Development',
  [switch]$Install
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path "$PSScriptRoot\..\LevelZero.uproject").Path
$ndk = Get-ChildItem "$AndroidSdk\ndk" -Directory | Where-Object { $_.Name -match '^(27\.2|28\.|29\.)' } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $ndk) { throw 'Need an Android NDK r27c-r29 (UE 5.8). Expected 27.2.12479018 or 28.x under the SDK ndk folder.' }
if (-not (Test-Path "$JavaRoot\bin\java.exe")) { throw "JDK 17-23 required (UE 5.8 uses Gradle 8.13). Not found: $JavaRoot" }
$env:ANDROID_HOME = $AndroidSdk; $env:ANDROID_SDK_ROOT = $AndroidSdk
$env:NDKROOT = $ndk.FullName; $env:NDK_ROOT = $ndk.FullName; $env:ANDROID_NDK_ROOT = $ndk.FullName
$env:JAVA_HOME = $JavaRoot; $env:PATH = "$JavaRoot\bin;$AndroidSdk\platform-tools;$env:PATH"
$out = Join-Path (Split-Path $project) 'Packaged'
$uatArgs = @('BuildCookRun', "-project=$project", '-platform=Android', '-cookflavor=ASTC', "-clientconfig=$Config",
          '-build', '-cook', '-stage', '-pak', '-package', '-archive', "-archivedirectory=$out", '-nop4', '-utf8output')
if ($Install) { $uatArgs += '-deploy' }
& "$EngineRoot\Engine\Build\BatchFiles\RunUAT.bat" @args
exit $LASTEXITCODE
