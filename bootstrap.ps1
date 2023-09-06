$root = $PSScriptRoot
if (!$root) {
    $root = "."
}

cmake -B $root/build $root | Out-Null
cmake --build $root/build --config Release -- `
    "-p:UseMultiToolTask=true" `
    "-noLogo" `
    "-p:EnforceProcessCountAcrossBuilds=true" `
    "-clp:ErrorsOnly;Verbosity=minimal"

if (! $?) {
    Write-Error "Build failed"
    exit $LastExitCode
}

Copy-Item "$root\build\src\Release\mob.exe" "$root\mob.exe"
Write-Output "run ``.\mob -d prefix/path build`` to start building"
