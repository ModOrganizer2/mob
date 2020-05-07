@echo off
Setlocal EnableDelayedExpansion

set "vswhere_cmd=third-party\bin\vswhere.exe -nologo -prerelease -latest -property installationPath"

for /F "tokens=* USEBACKQ" %%A in (`%vswhere_cmd%`) do (
	set ret=%errorlevel%
	if %errorlevel% neq 0 (
		echo %%F
		echo vswhere returned %ret%
		exit /b 1
	)

	set "installation_path=%%A"

	if "%installation_path%" == "" (
		echo empty installation path
		exit /b 1
	)
)

set "opts="
set "opts=%opts% vs/mob.sln"
set "opts=%opts% -m "
set "opts=%opts% -p:Configuration=Release"
set "opts=%opts% -noLogo "
set "opts=%opts% -p:UseMultiToolTask=true"
set "opts=%opts% -p:EnforceProcessCountAcrossBuilds=true"
set "opts=%opts% -clp:ErrorsOnly;Verbosity=minimal"

set "vcvars=%installation_path%\VC\Auxiliary\Build\vcvarsall.bat"
cmd /c ""%vcvars%" amd64 > NUL && msbuild %opts%"
echo run `mob -d path` to start building
