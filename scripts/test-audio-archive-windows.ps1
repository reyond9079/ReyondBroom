param([Parameter(Mandatory=$true)][string]$Runtime,
      [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$Runtime = (Resolve-Path $Runtime).Path
. "$PSScriptRoot/setup-ci-renderer.ps1" -Runtime $Runtime
python scripts/test-audio-archive.py $Runtime $Output
if ($LASTEXITCODE) { throw 'Native Windows audio archive/window regression failed' }
