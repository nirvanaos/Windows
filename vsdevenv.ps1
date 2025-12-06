$vsWherePath = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
& "$vsWherePath" -latest -format value -property installationPath | Tee-Object -Variable Global:visualStudioPath | out-null
Join-Path "$visualStudioPath" "\Common7\Tools\Microsoft.VisualStudio.DevShell.dll" | Import-Module
Enter-VsDevShell -VsInstallPath:"$visualStudioPath" -SkipAutomaticLocation | out-null
$EnvData = @{
    WindowsSdkDir = ${env:WindowsSdkDir}
		WindowsSDKVersion = ${env:WindowsSDKVersion}
    VCToolsInstallDir = ${env:VCToolsInstallDir}
}
$EnvData | ConvertTo-Json
