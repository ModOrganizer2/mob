param(
    [Parameter(Mandatory = $true)][String]$PyQtPath,
    [Parameter(Mandatory = $true)][String]$PythonPath,
    [Parameter(Mandatory = $true)][String]$Output
)

$Output = [IO.Path]::GetFullPath($Output)

7z a $Output (Join-Path -Path $PyQtPath -ChildPath "LICENSE")

Push-Location $PythonPath
7z a $Output "Lib/site-packages/PyQt6" "-xr!__pycache__"
Pop-Location
