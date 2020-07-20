$installationPath = & $PSScriptRoot\third-party\bin\vswhere.exe -products * -nologo -prerelease -latest -property installationPath
if (! $?) {
    Write-Error "vswhere returned $LastExitCode"
    exit $LastExitCode
}

if (! $installationPath) {
    Write-Error "Empty installation path"
    exit 1
}

$opts = ""
$opts += " $PSScriptRoot\vs\mob.sln"
$opts += " -m"
$opts += " -p:Configuration=Release"
$opts += " -noLogo"
$opts += " -p:UseMultiToolTask=true"
$opts += " -p:EnforceProcessCountAcrossBuilds=true"
$opts += " -clp:ErrorsOnly;Verbosity=minimal"

$vsDevCmd = "$installationPath\Common7\Tools\VsDevCmd.bat"
if (!(Test-Path "$vsDevCmd")) {
    Write-Error "VdDevCmd.bat not found at $vsDevCmd"
    exit 1
}

& "${env:COMSPEC}" /c "`"$vsDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && msbuild $opts"

if (! $?) {
    Write-Error "Build failed"
    exit $LastExitCode
}

Copy-Item "$PSScriptRoot\build\Release\x64\mob.exe" "$PSScriptRoot\mob.exe"
echo "run ``.\mob -d prefix/path build`` to start building"