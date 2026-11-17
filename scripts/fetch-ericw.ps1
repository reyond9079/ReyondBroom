param([string]$Destination="$PSScriptRoot/../tools/ericw")
$ErrorActionPreference='Stop'
& python "$PSScriptRoot/fetch-ericw.py" --platform Windows-x64 --destination $Destination
if($LASTEXITCODE){exit $LASTEXITCODE}
