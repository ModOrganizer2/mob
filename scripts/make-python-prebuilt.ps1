param(
    [Parameter(Mandatory = $true)][String]$Path,
    [Parameter(Mandatory = $true)][String]$Output
)

7z a $Output (Join-Path -Path $Path -ChildPath "PCbuild/amd64") "-xr!pythoncore_temp"
7z rn $Output "amd64" "PCbuild/amd64"
7z a $Output (Join-Path -Path $Path -ChildPath "Include") "-xr!internal" "-xr!sip.h"
7z a $Output (Join-Path -Path $Path -ChildPath "Lib") "-xr!__pycache__" "-xr!site-packages"
7z a $Output (Join-Path -Path $Path -ChildPath "LICENSE")
