@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul

rem Project48 local dedicated-server launcher.
rem Optional overrides:
rem   set P48_UNREAL_EDITOR=D:\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe
rem   set P48_EXPECTED_PLAYERS=2

set "PROJECT_FILE=%~dp0Project48.uproject"
set "GAME_MAP_FILE=%~dp0Content\MSJ\Maps\P48_FlyingIslandMap.umap"
set "DRY_RUN=0"

if /I "%~1"=="--dry-run" set "DRY_RUN=1"
if not defined P48_EXPECTED_PLAYERS set "P48_EXPECTED_PLAYERS=2"

if not exist "%PROJECT_FILE%" (
    echo [ERROR] Project48.uproject를 찾을 수 없습니다.
    echo         이 배치 파일을 프로젝트 루트 폴더에 두고 실행해 주세요.
    pause
    exit /b 1
)

if not exist "%GAME_MAP_FILE%" (
    echo [ERROR] Flying Island 맵을 찾을 수 없습니다.
    echo         %GAME_MAP_FILE%
    pause
    exit /b 1
)

for %%F in ("%GAME_MAP_FILE%") do if %%~zF LSS 1024 call :HandleInvalidMap
if errorlevel 1 exit /b 1

set "UE_EDITOR="
for /f "usebackq delims=" %%I in (`powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='SilentlyContinue'; $project=$env:PROJECT_FILE; $association=(ConvertFrom-Json (Get-Content -LiteralPath $project -Raw)).EngineAssociation; $candidates=New-Object System.Collections.Generic.List[string]; if($env:P48_UNREAL_EDITOR){$candidates.Add($env:P48_UNREAL_EDITOR)}; $launcher=Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'; if(Test-Path -LiteralPath $launcher){$data=ConvertFrom-Json (Get-Content -LiteralPath $launcher -Raw); foreach($item in $data.InstallationList){if($item.AppName -eq ('UE_'+$association) -or $item.AppVersion -like ($association+'*')){$candidates.Add((Join-Path $item.InstallLocation 'Engine\Binaries\Win64\UnrealEditor.exe'))}}}; $builds=Get-ItemProperty -LiteralPath 'HKCU:\Software\Epic Games\Unreal Engine\Builds'; if($builds -and $association){$property=$builds.PSObject.Properties[$association]; if($property){$candidates.Add((Join-Path $property.Value 'Engine\Binaries\Win64\UnrealEditor.exe'))}}; $installed=Get-ItemProperty -LiteralPath ('HKLM:\SOFTWARE\EpicGames\Unreal Engine\'+$association); if($installed.InstalledDirectory){$candidates.Add((Join-Path $installed.InstalledDirectory 'Engine\Binaries\Win64\UnrealEditor.exe'))}; if($env:ProgramFiles){$candidates.Add((Join-Path $env:ProgramFiles ('Epic Games\UE_'+$association+'\Engine\Binaries\Win64\UnrealEditor.exe')))}; foreach($drive in [System.IO.DriveInfo]::GetDrives()){if($drive.IsReady){$root=$drive.RootDirectory.FullName; $candidates.Add((Join-Path $root ('Epic Games\UE_'+$association+'\Engine\Binaries\Win64\UnrealEditor.exe'))); $candidates.Add((Join-Path $root ('UnrealEngine\UE_'+$association+'\Engine\Binaries\Win64\UnrealEditor.exe')))}}; $command=Get-Command UnrealEditor.exe; if($command){$candidates.Add($command.Source)}; foreach($candidate in $candidates){if($candidate -and (Test-Path -LiteralPath $candidate)){[Console]::WriteLine([System.IO.Path]::GetFullPath($candidate)); break}}"`) do set "UE_EDITOR=%%I"

if not defined UE_EDITOR call :PromptForUnrealEditor

if not defined UE_EDITOR (
    echo [ERROR] UnrealEditor.exe 경로를 확인할 수 없습니다.
    pause
    exit /b 1
)

set "GAME_URL=/Game/MSJ/Maps/P48_FlyingIslandMap?game=/Game/KSH/Game/BP_P48SurvivalGameMode.BP_P48SurvivalGameMode_C?ExpectedPlayers=%P48_EXPECTED_PLAYERS%"
set "LOBBY_URL=/Game/WJS/Lobby/L_Lobby"
set "GAME_SERVER_ADDRESSES=127.0.0.1:17778,127.0.0.1:17779,127.0.0.1:17780"

echo.
echo ========================================
echo Project48 로컬 서버 실행
echo ========================================
echo Unreal:  %UE_EDITOR%
echo Project: %PROJECT_FILE%
echo Players: %P48_EXPECTED_PLAYERS%
echo.

if "%DRY_RUN%"=="1" (
    echo [DRY RUN] 실제 서버는 실행하지 않습니다.
    echo.
    echo Game Server 1: 127.0.0.1:17778
    echo Game Server 2: 127.0.0.1:17779
    echo Game Server 3: 127.0.0.1:17780
    echo Lobby Server:  127.0.0.1:17777
    exit /b 0
)

for %%P in (17778 17779 17780) do (
    echo 게임 서버를 실행합니다: 127.0.0.1:%%P
    start "Project48 Game Server %%P" "%UE_EDITOR%" "%PROJECT_FILE%" "%GAME_URL%" -server -log -port=%%P
    timeout /t 1 /nobreak >nul
)

echo 로비 서버를 실행합니다: 127.0.0.1:17777
start "Project48 Lobby Server 17777" "%UE_EDITOR%" "%PROJECT_FILE%" "%LOBBY_URL%" -server -log -port=17777 "-LobbyGameServerAddresses=%GAME_SERVER_ADDRESSES%" "-LobbyReturnAddress=127.0.0.1:17777"

echo.
echo 서버 실행 요청을 완료했습니다.
echo 게임 서버 3개와 로비 서버 1개의 로그 창을 확인해 주세요.
timeout /t 5 /nobreak >nul
exit /b 0

:HandleInvalidMap
if "%DRY_RUN%"=="1" (
    echo [WARNING] Flying Island 맵이 정상 에셋이 아닌 Git LFS 포인터 상태입니다.
    echo           실제 실행 전 최신 dev를 받은 뒤 git lfs pull을 실행해 주세요.
    exit /b 0
)
echo [ERROR] Flying Island 맵이 정상 에셋이 아닌 Git LFS 포인터 상태입니다.
echo         최신 dev를 받은 뒤 git lfs pull을 실행해 주세요.
pause
exit /b 1

:PromptForUnrealEditor
echo [WARNING] UnrealEditor.exe를 자동으로 찾지 못했습니다.
echo 프로젝트에 연결할 Unreal Engine 설치 폴더 또는 UnrealEditor.exe 경로를 입력해 주세요.
set /p "UE_INPUT=경로: "
set "UE_INPUT=%UE_INPUT:"=%"
if exist "%UE_INPUT%\Engine\Binaries\Win64\UnrealEditor.exe" set "UE_EDITOR=%UE_INPUT%\Engine\Binaries\Win64\UnrealEditor.exe"
for %%F in ("%UE_INPUT%") do if exist "%%~fF" if /I "%%~xF"==".exe" set "UE_EDITOR=%%~fF"
exit /b 0
