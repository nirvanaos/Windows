$vsWherePath = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
& "$vsWherePath" -latest -format value -property installationPath | Tee-Object -Variable Global:visualStudioPath | out-null
Join-Path "$visualStudioPath" "\Common7\Tools\Microsoft.VisualStudio.DevShell.dll" | Import-Module
Enter-VsDevShell -VsInstallPath:"$visualStudioPath" -SkipAutomaticLocation -HostArch amd64 -Arch amd64 | out-null
$EnvData = @{
    WIN_SDK_INC_DIR = "${env:WindowsSdkDir}Include\${env:WindowsSDKVersion}"
    MSVC_INC_DIR = "${env:VCToolsInstallDir}include"
}
$EnvData | ConvertTo-Json -Compress
