[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GamePath,

    [switch]$InstallSilentPatch = $true,
    [switch]$DownloadMapContent,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$GamePath = [System.IO.Path]::GetFullPath($GamePath)
$GameExe = Join-Path $GamePath 'gta_sa.exe'
$ScriptsPath = Join-Path $GamePath 'scripts'
$DownloadPath = Join-Path $GamePath '_ModernSA_Downloads'

if (-not (Test-Path $GamePath -PathType Container)) {
    throw "Game directory does not exist: $GamePath"
}
if (-not (Test-Path $GameExe -PathType Leaf)) {
    throw "gta_sa.exe was not found in: $GamePath"
}

New-Item -ItemType Directory -Force -Path $ScriptsPath, $DownloadPath | Out-Null

function Get-VerifiedFile {
    param(
        [Parameter(Mandatory = $true)][string]$Uri,
        [Parameter(Mandatory = $true)][string]$OutFile,
        [Parameter(Mandatory = $true)][string]$Sha256
    )

    if ((Test-Path $OutFile) -and -not $Force) {
        $current = (Get-FileHash -Algorithm SHA256 $OutFile).Hash.ToLowerInvariant()
        if ($current -eq $Sha256.ToLowerInvariant()) {
            Write-Host "Using verified cached file: $OutFile"
            return
        }
    }

    Write-Host "Downloading: $Uri"
    Invoke-WebRequest -UseBasicParsing -Uri $Uri -OutFile $OutFile

    $actual = (Get-FileHash -Algorithm SHA256 $OutFile).Hash.ToLowerInvariant()
    $expected = $Sha256.ToLowerInvariant()
    if ($actual -ne $expected) {
        Remove-Item $OutFile -Force -ErrorAction SilentlyContinue
        throw "SHA-256 mismatch for $OutFile. Expected $expected, got $actual"
    }
}

if ($InstallSilentPatch) {
    $spUrl = 'https://github.com/CookiePLMonster/SilentPatch/releases/download/1.1-BUILD34.1-SA/SilentPatchSA.zip'
    $spHash = '0837bbdc0548a4a952f924adefd2bb6eacc866407f153844fe1f56995668c42c'
    $spZip = Join-Path $DownloadPath 'SilentPatchSA-Build34.1.zip'
    $spTemp = Join-Path $env:TEMP ('ModernSA-SilentPatch-' + [Guid]::NewGuid().ToString('N'))

    Get-VerifiedFile -Uri $spUrl -OutFile $spZip -Sha256 $spHash
    New-Item -ItemType Directory -Force -Path $spTemp | Out-Null
    try {
        Expand-Archive -Path $spZip -DestinationPath $spTemp -Force

        $asi = Get-ChildItem $spTemp -Recurse -File -Filter 'SilentPatchSA.asi' | Select-Object -First 1
        $ini = Get-ChildItem $spTemp -Recurse -File -Filter 'SilentPatchSA.ini' | Select-Object -First 1
        if (-not $asi) {
            throw 'SilentPatchSA.asi was not found in the official archive.'
        }

        Copy-Item $asi.FullName (Join-Path $ScriptsPath 'SilentPatchSA.asi') -Force
        if ($ini) {
            Copy-Item $ini.FullName (Join-Path $ScriptsPath 'SilentPatchSA.ini') -Force
        }
        Write-Host 'SilentPatch SA Build 34.1 installed.' -ForegroundColor Green
    }
    finally {
        Remove-Item $spTemp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($DownloadMapContent) {
    # These are map/model/IDE/IPL assets, not SilentPatch engine code. Keep them
    # sourced from the map project's own release instead of vendoring third-party
    # GTA assets into gta-reversed.
    $mapUrl = 'https://github.com/UnitedMel/SA-Definitive-Map-Content-Additions/releases/download/latest/SA.DefinitiveMapContentAdditions.7z'
    $mapHash = '96bd39d3644123270f496a645a68fe8ad629ea4dff468ac4ccec47e229357704'
    $mapArchive = Join-Path $DownloadPath 'SA.DefinitiveMapContentAdditions.7z'
    Get-VerifiedFile -Uri $mapUrl -OutFile $mapArchive -Sha256 $mapHash

    Write-Host ''
    Write-Host 'Verified PS2/map-content package downloaded:' -ForegroundColor Green
    Write-Host "  $mapArchive"
    Write-Host 'Map/model packs have overlapping files and extra dependencies, so they are intentionally not blindly merged into gta3.img.'
    Write-Host 'Install the release according to its project guide (normally through ModLoader / its supplied layout).'
}

Write-Host ''
Write-Host 'Modern SA compatibility installation step completed.' -ForegroundColor Green
Write-Host 'gta-reversed still requires the exact GTA SA 1.0 US Compact executable expected by the project.'
