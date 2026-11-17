param(
    [Parameter(Mandatory=$true)][string]$Bundle,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$src = (Resolve-Path "$PSScriptRoot/..").Path
$test = Join-Path ([IO.Path]::GetTempPath()) ('ReyondBroom-editing-' + [guid]::NewGuid())
Copy-Item -LiteralPath (Resolve-Path $Bundle).Path -Destination $test -Recurse
Get-ChildItem "$test/ftebroom/maps/edit_*.map" -ErrorAction SilentlyContinue | Remove-Item
foreach ($name in @('materials04.png','clip04.png','reload04.png','qconsole.log')) {
    $artifact = Join-Path "$test/ftebroom" $name
    if (Test-Path -LiteralPath $artifact) { Remove-Item -LiteralPath $artifact }
}
& $Python -X utf8 "$src/tests/prepare-project-fixture.py" $test
if ($LASTEXITCODE -ne 0) { throw 'Fixture creation failed' }
Copy-Item "$src/tests/editing.cfg" "$test/ftebroom/editing-steps.cfg"
$fixture = ($test + '/fixture assets/baseq3').Replace('\','/')
@"
exec ftebroom.cfg
set ftebroom_clip_keep 1
set ftebroom_skip_setup 1
in 12 ftebroom_project "$fixture"
in 16 exec editing-steps.cfg
"@ | Set-Content "$test/ftebroom/editing.cfg"
$engine = Join-Path $test 'bin/fteqw64.exe'
if (!(Test-Path -LiteralPath $engine)) { $engine = Join-Path $test 'fteqw64.exe' }
$process = Start-Process $engine -ArgumentList @('-nohome','-noupdates','-basedir',('"'+$test+'"'),'-window','-width','1280','-height','800','-condebug','+exec','editing.cfg') -WorkingDirectory $test -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(115000)) {
    Stop-Process -Id $process.Id
    throw "Editing test timed out; logs: $test"
}
if ($process.ExitCode -ne 0) { throw "Engine exited with code $($process.ExitCode); logs: $test" }
& $Python -X utf8 "$src/tests/check-editing.py" "$test/ftebroom/maps"
if ($LASTEXITCODE -ne 0) { throw "Editing assertions failed; logs: $test" }
foreach ($image in @('materials04.png','clip04.png','reload04.png')) {
    if (!(Test-Path "$test/ftebroom/$image")) { throw "Missing engine screenshot: $image" }
}
$log = Get-Content "$test/ftebroom/qconsole.log" -Raw
if ([regex]::Matches($log,'permanent workspace restored for map').Count -lt 2) {
    throw 'Workspace was not restored after map reload'
}
Write-Host "PASS: editing regression. Inspect engine screenshots at $test/ftebroom"
