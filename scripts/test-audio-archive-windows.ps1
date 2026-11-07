param([Parameter(Mandatory=$true)][string]$Runtime,
      [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$Runtime = (Resolve-Path $Runtime).Path
$mesa = Join-Path $env:RUNNER_TEMP 'reyond-audio-mesa'
New-Item -ItemType Directory -Force $mesa | Out-Null
gh release download 25.3.3 --repo pal1000/mesa-dist-win --dir $mesa --pattern mesa3d-25.3.3-release-msvc.7z --clobber
if ($LASTEXITCODE) { throw 'Mesa test renderer download failed' }
$archive = Join-Path $mesa 'mesa3d-25.3.3-release-msvc.7z'
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLower() -ne '66b79057ba273c08daa03551a231995d48b8623bb1efa9f3a7c3e09792d8f1ba') { throw 'Mesa checksum mismatch' }
& 'C:/Program Files/7-Zip/7z.exe' x $archive "-o$mesa/extracted" -y | Out-Null
if ($LASTEXITCODE) { throw 'Mesa extraction failed' }
$gl = Get-ChildItem "$mesa/extracted" -Recurse -Filter opengl32.dll | Where-Object { $_.DirectoryName -match 'x64|amd64' } | Select-Object -First 1
if (!$gl) { throw '64-bit Mesa DLL missing' }
# Test only the extracted runtime; distribution archives/checksums stay untouched.
Copy-Item (Join-Path $gl.DirectoryName '*.dll') "$Runtime/bin" -Force
Copy-Item $gl.FullName "$Runtime/bin/rb_qa_opengl.dll" -Force
if (Get-Command Set-DisplayResolution -ErrorAction SilentlyContinue) {
  Set-DisplayResolution -Width 1920 -Height 1080 -Force
}
$env:RB_QA_RENDERER = 'gl rb_qa_opengl.dll'
python scripts/test-audio-archive.py $Runtime $Output
if ($LASTEXITCODE) { throw 'Native Windows audio archive/window regression failed' }
