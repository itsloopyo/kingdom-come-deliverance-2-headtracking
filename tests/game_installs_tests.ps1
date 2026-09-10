#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '../scripts/GameInstalls.psm1') -Force
$core = & (Get-Module GameInstalls) { Get-Module GamePathDetection }
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('kcd2-installs-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $fixture | Out-Null

try {
    $steam = Join-Path $fixture 'Steam/steamapps/common/KingdomComeDeliverance2'
    $gog = Join-Path $fixture 'GOG/KingdomComeDeliverance2'
    $xbox = Join-Path $fixture 'XboxGames/KCD2/Content'
    $kcd1 = Join-Path $fixture 'XboxGames/KCD1/Content'
    $stale = Join-Path $fixture 'StaleSteam/steamapps/common/KingdomComeDeliverance2'
    foreach ($path in @($steam, $gog, $stale)) {
        New-Item -ItemType Directory -Path (Join-Path $path 'Bin/Win64MasterMasterSteamPGO') -Force | Out-Null
    }
    foreach ($path in @($steam, $gog)) {
        [IO.File]::WriteAllText((Join-Path $path 'Bin/Win64MasterMasterSteamPGO/KingdomCome.exe'), '')
    }
    foreach ($path in @($xbox, $kcd1)) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $path 'KingdomCome.exe'), '')
        [IO.File]::WriteAllText((Join-Path $path 'gamelaunchhelper.exe'), '')
    }
    [IO.File]::WriteAllText((Join-Path $xbox 'MicrosoftGame.Config'), '<Game><Identity Name="DeepSilver.77536C3FE941" /></Game>')
    [IO.File]::WriteAllText((Join-Path $kcd1 'MicrosoftGame.Config'), '<Game><Identity Name="another-game" /></Game>')

    & $core {
        param($FixtureRoot, $GogRoot)
        $script:FixtureRoot = $FixtureRoot
        $script:GogRoot = $GogRoot
        $script:EnabledStores = @('Steam', 'GOG', 'Xbox')
        function script:Find-SteamLibraries {
            if ($script:EnabledStores -contains 'Steam') {
                @((Join-Path $script:FixtureRoot 'Steam'), (Join-Path $script:FixtureRoot 'StaleSteam'))
            }
        }
        function script:Find-SteamGameByAppId {
            if ($script:EnabledStores -contains 'Steam') { Join-Path $script:FixtureRoot 'Steam/steamapps/common/KingdomComeDeliverance2' }
        }
        function script:Find-GogGamePath {
            if ($script:EnabledStores -contains 'GOG') { $script:GogRoot }
        }
        function script:Get-XboxGameRoots {
            if ($script:EnabledStores -contains 'Xbox') { Join-Path $script:FixtureRoot 'XboxGames' }
        }
        $config = (Get-GameConfigs)['kingdom-come-deliverance-2']
        $config.EnvVar = 'KCD2_TEST_UNUSED_PATH'
    } $fixture $gog

    $installs = @(Get-Kcd2Installs)
    if ($installs.Count -ne 3) { throw "Expected three installs, got $($installs.Count)" }
    foreach ($path in @($steam, $gog, $xbox)) {
        $matches = @($installs | Where-Object Path -eq $path)
        if ($matches.Count -ne 1) { throw "Expected exactly one install at $path" }
        $expected = if ($path -eq $xbox) { $path } else { Join-Path $path 'Bin/Win64MasterMasterSteamPGO' }
        if ($matches[0].ExeDirectory -ne $expected) { throw "Wrong deployment directory for $path" }
    }
    $explicit = @(Get-Kcd2Installs -GamePath $xbox)
    if ($explicit.Count -ne 1 -or $explicit[0].Store -ne 'Game Pass') { throw 'Explicit Game Pass path failed' }
    foreach ($invalid in @($kcd1, $stale)) {
        $rejected = $false
        try { Get-Kcd2Installs -GamePath $invalid | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw "Accepted invalid install: $invalid" }
    }
    foreach ($stores in @(@('Steam'), @('GOG'), @('Xbox'), @('Steam', 'Xbox'), @())) {
        & $core { param($Stores) $script:EnabledStores = $Stores } $stores
        $found = @(Get-Kcd2Installs)
        if ($found.Count -ne $stores.Count) {
            throw "Expected $($stores.Count) installs for [$($stores -join ', ')], got $($found.Count)"
        }
    }
    Write-Host 'Install discovery passed: Steam + GOG + Game Pass, duplicate and stale paths, KCD1 exclusion, explicit paths.'
} finally {
    $resolved = (Resolve-Path -LiteralPath $fixture).Path
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Fixture escaped temporary directory: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
