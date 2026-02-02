param(
    [switch]
    $Verbose,
    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]
    $Config = "Release"
)

$root = $PSScriptRoot
if (!$root) {
    $root = "."
}

$logLevel = if ($Verbose) { "STATUS" } else { "ERROR" }

cmake --preset vcpkg --log-level=$logLevel

if ($Verbose) {
    cmake --build --preset $Config --verbose
} else {
    cmake --build --preset $Config
}

if (! $?) {
    Write-Error "Build failed"
    exit $LastExitCode
}

Copy-Item "$root\build\src\${Config}\mob.exe" "$root\mob.exe"
Write-Output "run ``.\mob -d prefix/path build`` to start building"
