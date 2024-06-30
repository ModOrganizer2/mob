param(
    [Parameter(Mandatory = $true)][String]$Path,
    [Parameter(Mandatory = $true)][String]$Output
)

7z a $Output (Join-Path -Path $Path -ChildPath "boost")
7z a $Output (Join-Path -Path $Path -ChildPath "lib32-msvc-14.3")
7z a $Output (Join-Path -Path $Path -ChildPath "lib64-msvc-14.3")
7z a $Output (Join-Path -Path $Path -ChildPath "LICENSE_1_0.txt")
