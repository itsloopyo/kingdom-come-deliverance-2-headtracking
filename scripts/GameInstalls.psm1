#!/usr/bin/env pwsh
#Requires -Version 5.1
Set-StrictMode -Version Latest

<#
.SYNOPSIS
    Every installed copy of Kingdom Come: Deliverance II on this machine, with
    the directory the mod deploys into for each.
.DESCRIPTION
    The stores do not agree on layout. Steam and GOG install
    Bin\Win64MasterMasterSteamPGO\KingdomCome.exe under the game root; the Game
    Pass package installs KingdomCome.exe flat in
    <XboxGames>\Kingdom Come- Deliverance II\Content. Ultimate ASI Loader only
    scans the directory the executable is in, so that directory - not the game
    root - is what every dev task writes to, and it is derived from the
    executable's own relative path rather than assumed.

    A machine can hold more than one of these at once, and they are separate
    links of the game needing separate build profiles, so the tasks act on all
    of them rather than on whichever detection returned first.
#>

$Script:CoreModule = Join-Path (Split-Path -Parent $PSScriptRoot) 'cameraunlock-core/powershell/GamePathDetection.psm1'
Import-Module $Script:CoreModule -Force

$Script:GameId = 'kingdom-come-deliverance-2'

<#
.SYNOPSIS
    Describe one install: where it is, which store it came from, and where the
    ASI and loader go.
.PARAMETER Path
    The game root, as a store's detection reports it.
#>
function New-Kcd2Install {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)

    $config = (Get-GameConfigs)[$Script:GameId]

    # The Xbox build's executable relpath is its own field, because the GDK
    # package flattens the Bin\Win64MasterMasterSteamPGO the other stores keep.
    $identity = Get-XboxPackageIdentity -ContentDir $Path
    if ($identity -and $identity -ne $config.XboxIdentityName) {
        throw "This is not a Kingdom Come: Deliverance II package: $Path ($identity)"
    }
    $isXbox = $identity -eq $config.XboxIdentityName
    $exeRelPath = if ($isXbox) {
        $config.XboxExecutable
    } else {
        $config.Executable
    }

    $store = if ($isXbox) {
        'Game Pass'
    } elseif (@(Find-SteamLibraries) | Where-Object { $Path -like (Join-Path $_ 'steamapps\common\*') }) {
        'Steam'
    } else {
        'install'
    }

    $exePath = Join-Path $Path $exeRelPath
    if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
        throw "Game executable not found: $exePath"
    }
    return [pscustomobject]@{
        Store        = $store
        Path         = $Path
        ExeRelPath   = $exeRelPath
        ExePath      = $exePath
        ExeDirectory = (Split-Path -Parent $exePath)
    }
}

<#
.SYNOPSIS
    Every install on this machine, or just the one at -GamePath when given.
.PARAMETER GamePath
    An explicit game root, validated before any task writes to it.
.OUTPUTS
    The objects New-Kcd2Install returns, in detection order.
#>
function Get-Kcd2Installs {
    [CmdletBinding()]
    param([string]$GamePath = '')

    if ($GamePath) {
        if (-not (Test-Path -LiteralPath $GamePath -PathType Container)) {
            throw "Game path does not exist or is not a directory: $GamePath"
        }
        return @(New-Kcd2Install -Path ((Resolve-Path -LiteralPath $GamePath).Path))
    }

    return @(Find-AllGamePaths -GameId $Script:GameId | ForEach-Object { New-Kcd2Install -Path $_ })
}

Export-ModuleMember -Function @('Get-Kcd2Installs', 'New-Kcd2Install')
