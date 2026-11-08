# Legacy invocation still downloads and waits for the editor. Check mode never
# downloads; prepare reports verified progress and waits for ParentId. Relaunch
# requires restart.request containing the same Session, written by Restart.
param([Parameter(Mandatory=$true)][string]$Root,[Parameter(Mandatory=$true)][string]$CurrentVersion,[int]$ParentId=0,[string]$LocalPackage='',[ValidateSet('auto','check','prepare')][string]$Mode='auto',[string]$Session='',[string]$TargetVersion='')
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath($Root)
if(!(Test-Path -LiteralPath "$Root/fteqw64.exe") -and !(Test-Path -LiteralPath "$Root/bin/fteqw64.exe")){throw 'Invalid editor installation'}
$updates=Join-Path $Root 'updates'
New-Item -ItemType Directory -Force -Path $updates | Out-Null
if($Session -and $Session -notmatch '^[a-zA-Z0-9_-]{1,80}$'){throw 'Invalid update session'}
$state=@{session=$Session;current_version=$CurrentVersion;available_version='';stage='checking';received=0L;total=0L}
function Write-UpdateStatusFile([string]$name,[string]$text){
 $temporary=Join-Path $updates ($name+'.tmp');$target=Join-Path $updates $name
 [IO.File]::WriteAllText($temporary,$text,[Text.UTF8Encoding]::new($false))
 for($attempt=0;$attempt -lt 5;$attempt++){
  try{Move-Item -LiteralPath $temporary -Destination $target -Force;return}catch{
   $cause=$_.Exception.GetBaseException()
   if($cause -isnot [IO.IOException] -and $cause -isnot [UnauthorizedAccessException]){throw}
   # Native readers can briefly hold a Windows file handle without delete
   # sharing. A telemetry collision must not abort a verified update.
   if($attempt -eq 4){return};Start-Sleep -Milliseconds 20
  }
 }
}
function Status([string]$text,[string]$stage='', [long]$received=-1,[long]$total=-1){
 if($stage){$state.stage=$stage};if($received -ge 0){$state.received=$received};if($total -ge 0){$state.total=$total};$state.message=$text
 Write-UpdateStatusFile 'status.txt' $text
 Write-UpdateStatusFile 'status.json' ($state | ConvertTo-Json -Compress)
}
function Under([string]$base,[string]$relative){$p=[IO.Path]::GetFullPath((Join-Path $base $relative));if(!$p.StartsWith($base.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe archive path'};return $p}
function FileHash([string]$path){$stream=[IO.File]::OpenRead($path);$sha=[Security.Cryptography.SHA256]::Create();try{return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$sha.Dispose()}}
function CleanupUpdates([string]$keep){
 $base=[IO.Path]::GetFullPath($updates)
 if((Get-Item -LiteralPath $base).Attributes -band [IO.FileAttributes]::ReparsePoint){return}
 foreach($item in Get-ChildItem -LiteralPath $base){
  $target=Under $base $item.Name
  if($target -eq $keep -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){continue}
  if($item.PSIsContainer -and $item.Name -match '^(stage|backup)-[a-zA-Z0-9_-]+$'){
   # Avoid traversing junctions, including those inside an old staging folder.
   $pending=[Collections.Generic.Queue[string]]::new();$pending.Enqueue($target);$linked=$false
   while($pending.Count -and !$linked){foreach($child in Get-ChildItem -LiteralPath $pending.Dequeue() -Force){
    if($child.Attributes -band [IO.FileAttributes]::ReparsePoint){$linked=$true;break}
    if($child.PSIsContainer){$pending.Enqueue($child.FullName)}
   }}
   if(!$linked){Remove-Item -LiteralPath $target -Recurse -Force}
  }elseif(!$item.PSIsContainer -and $item.Name -match '^(?:ReyondBroom|FTEBroom)-[0-9][a-zA-Z0-9.\-]*-(Windows-x64\.zip|Linux-x64\.tar\.gz)(\.part|\.sha256)?$'){
   Remove-Item -LiteralPath $target -Force
  }
 }
}
function VersionKey([string]$tag){if($tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:-alpha\.(\d+))?$'){return $null};$alpha=if($Matches[4]){[int]$Matches[4]}else{999999};return [version]("{0}.{1}.{2}.{3}" -f $Matches[1],$Matches[2],$Matches[3],$alpha)}
function Open-UpdateResponse([string]$url){
 $fresh=$url+$(if($url.Contains('?')){'&'}else{'?'})+'_rb_update='+[DateTime]::UtcNow.Ticks
 $request=[Net.HttpWebRequest]::Create($fresh)
 $request.UserAgent='ReyondBroom-Updater';$request.Timeout=60000;$request.ReadWriteTimeout=60000
 $request.Headers['Cache-Control']='no-cache';$request.Headers['X-GitHub-Api-Version']='2022-11-28'
 $request.Accept=if($url.StartsWith('https://api.github.com/repos/reyond9079/ReyondBroom/releases/assets/')){'application/octet-stream'}else{'application/vnd.github+json'}
 return $request.GetResponse()
}
function Get-UpdateAssetUrls($asset){
 if(!$asset.browser_download_url -or !$asset.browser_download_url.StartsWith('https://github.com/reyond9079/ReyondBroom/releases/download/')){throw 'Unexpected download URL'}
 if($asset.url){
  if($asset.url -notmatch '^https://api\.github\.com/repos/reyond9079/ReyondBroom/releases/assets/[1-9][0-9]*$'){throw 'Unexpected asset API URL'}
  [string]$asset.url
 }
 [string]$asset.browser_download_url
}
function Read-UpdateText($response,[int]$limit=4194304){
 $stream=$response.GetResponseStream();$memory=[IO.MemoryStream]::new()
 try{
  $buffer=New-Object byte[] 4096
  while(($n=$stream.Read($buffer,0,$buffer.Length)) -gt 0){
   if($memory.Length+$n -gt $limit){throw 'Unexpected response size'}
   $memory.Write($buffer,0,$n)
  }
  return [Text.Encoding]::UTF8.GetString($memory.ToArray())
 }finally{$stream.Dispose();$memory.Dispose()}
}
function Fetch-Update([string[]]$urls,[scriptblock]$consume){
 $last=$null
 for($route=0;$route -lt $urls.Count;$route++){
  for($attempt=0;$attempt -lt 3;$attempt++){
   try{
    $response=Open-UpdateResponse $urls[$route]
    try{return (& $consume $response)}finally{$response.Dispose()}
   }catch{
    $last=$_;$errorObject=$_.Exception.GetBaseException()
    if($errorObject -is [Net.WebException] -and $errorObject.Response){
     $code=[int]$errorObject.Response.StatusCode;$errorObject.Response.Close()
     if($code -notin @(408,429,500,502,503,504)){
      if($code -in @(403,404) -and $route+1 -lt $urls.Count){break}
      throw
     }
    }elseif($errorObject -isnot [Net.WebException] -and $errorObject -isnot [TimeoutException] -and $errorObject -isnot [IO.EndOfStreamException] -and $errorObject -isnot [Net.Sockets.SocketException]){throw}
    if($attempt -lt 2){Status 'Temporary download error. Retrying...';Start-Sleep -Seconds ($attempt+1)}
   }
  }
  if($route+1 -lt $urls.Count){Status 'Retrying through the alternative GitHub download route...'}
 }
 throw $last
}
function Download-Update($asset,$sum,[string]$version){
 $urls=@(Get-UpdateAssetUrls $asset);$sumUrls=@(Get-UpdateAssetUrls $sum)
 $name=[string]$asset.name
 if([IO.Path]::GetFileName($name) -ne $name -or $name.Contains('/') -or $name.Contains('\')){throw 'Unexpected asset name'}
 $checksum=Fetch-Update $sumUrls {
  param($response)
  $fields=(Read-UpdateText $response 4096).Trim() -split '\s+'
  if($fields.Count -ne 2 -or $fields[0] -notmatch '^[a-fA-F0-9]{64}$' -or $fields[1] -ne $name){throw 'Invalid release checksum'}
  $fields[0].ToLowerInvariant()
 }
 if($asset.digest -and $asset.digest -ne ('sha256:'+$checksum)){throw 'Release checksum disagrees with GitHub asset digest'}
 $archive=Under $updates $name;$partial=Under $updates ($name+'.part')
 try{
  $null=Fetch-Update $urls {
   param($response)
   $inputStream=$response.GetResponseStream();$output=[IO.File]::Create($partial)
   try{
    $buffer=New-Object byte[] 65536;$received=0L;$last=[DateTime]::MinValue
    Status 'Downloading update...' 'downloading' 0 $asset.size
    for(;;){
     try{$n=$inputStream.Read($buffer,0,$buffer.Length)}catch{
      if($_.Exception.GetBaseException() -is [IO.IOException]){throw [IO.EndOfStreamException]::new('Interrupted update download')}
      throw
     }
     if($n -le 0){break}
     $received+=$n;if($received -gt 536870912){throw 'Update exceeds 512 MiB'}
     $output.Write($buffer,0,$n)
     if(([DateTime]::Now-$last).TotalMilliseconds -gt 200){Status ("Downloading {0}: {1:N1} / {2:N1} MiB" -f $version,($received/1MB),($asset.size/1MB)) 'downloading' $received $asset.size;$last=[DateTime]::Now}
    }
    if($received -ne $asset.size){throw [IO.EndOfStreamException]::new('Incomplete update download')}
    Status 'Download complete. Verifying...' 'verifying' $received $asset.size
   }finally{$output.Dispose();$inputStream.Dispose()}
  }
  if((FileHash $partial) -ne $checksum){throw 'Update checksum mismatch'}
  Move-Item -LiteralPath $partial -Destination $archive -Force
 }finally{if(Test-Path -LiteralPath $partial){Remove-Item -LiteralPath $partial -Force}}
 return $archive
}
$oldManifest=@{}
if(Test-Path -LiteralPath "$Root/distribution-manifest.json"){try{$manifest=Get-Content -LiteralPath "$Root/distribution-manifest.json" -Raw | ConvertFrom-Json;foreach($property in $manifest.PSObject.Properties){$oldManifest[$property.Name]=$property.Value}}catch{}}
function Preserve([string]$name){
 $p=$name.Replace('\','/');$leaf=[IO.Path]::GetFileName($p)
 $existing=Under $Root $p
 # Saved game folders/definitions and per-game compiler workspaces are user
 # settings, including copies that still match an earlier package manifest.
 if($p -in @('reyondbroom-legacy-games.txt','ftebroom/reyondbroom-legacy-games.txt','ftebroom/reyondbroom-quake2-build-options.txt','ftebroom/reyondbroom-halflife-build-options.txt','ftebroom/compilation-profiles.json')){return (Test-Path -LiteralPath $existing)}
 if((Test-Path -LiteralPath $existing) -and $p -match '^ftebroom/.*\.(cfg|ini|map|wad|def|ent|fgd)$'){
  if(!$oldManifest.ContainsKey($p)){return $true}
  if((FileHash $existing) -ne $oldManifest[$p]){return $true}
 }
 return ($p -match '^(downloads|updates|ftebroom/(autosave|cache))/') -or ($leaf -match '^(ftebroom-(project|game-profile|quake1|build|preferences|run|plugins|camera).*\.(txt|cfg)|appearance\.cfg|workspace\.ini|config\.cfg|fte\.cfg|installed\.lst)$')
}
$mutex=$null;$stage=$null;$backup=$null;$changed=@();$process=$null
try {
 $hash=[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($Root.ToLowerInvariant()))).Replace('-','')
 $mutex=New-Object Threading.Mutex($false,('Local\FTEBroomUpdate'+$hash))
 if(!$mutex.WaitOne(0)){exit 0}
 if($ParentId){try{$process=[Diagnostics.Process]::GetProcessById($ParentId)}catch [ArgumentException]{}} # Closing the editor before this line is allowed.
 Status 'Checking GitHub releases...'
 if(!$LocalPackage){
  [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
  $releases=Fetch-Update @('https://api.github.com/repos/reyond9079/ReyondBroom/releases?per_page=100') {param($response);Read-UpdateText $response | ConvertFrom-Json}
  $current=VersionKey $CurrentVersion
  $release=$releases | Where-Object {
   $candidate="ReyondBroom-$($_.tag_name.TrimStart('v'))-Windows-x64.zip"
   !$_.draft -and (!$TargetVersion -or $_.tag_name.TrimStart('v') -eq $TargetVersion.TrimStart('v')) -and (VersionKey $_.tag_name) -and (VersionKey $_.tag_name) -gt $current -and ($_.assets.name -contains $candidate) -and ($_.assets.name -contains "$candidate.sha256")
  } | Sort-Object @{Expression={VersionKey $_.tag_name};Descending=$true} | Select-Object -First 1
  if(!$release){if($TargetVersion){throw 'The selected release is no longer available'};Status 'ReyondBroom is up to date.' 'current';exit 0}
  $version=$release.tag_name.TrimStart('v');$name="ReyondBroom-$version-Windows-x64.zip"
  $asset=$release.assets | Where-Object name -eq $name | Select-Object -First 1
  $sum=$release.assets | Where-Object name -eq "$name.sha256" | Select-Object -First 1
  if(!$asset -or !$sum){throw 'The newer release has no complete Windows package yet.'}
  $state.available_version=$version
  if($Mode -eq 'check'){Status 'A new version of ReyondBroom is available.' 'available' 0 $asset.size;exit 0}
  Status 'Downloading update...' 'downloading'
  $LocalPackage=Download-Update $asset $sum $version
 }elseif($Mode -eq 'check'){
  throw 'Check-only mode cannot install a local package'
 }
 Status 'Verifying and preparing the update...' 'verifying'
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $stage=Join-Path $updates ('stage-'+[guid]::NewGuid());New-Item -ItemType Directory -Path $stage | Out-Null
 $zip=[IO.Compression.ZipFile]::OpenRead($LocalPackage)
 try {$total=0L;foreach($entry in $zip.Entries){$name=$entry.FullName.Replace('\','/');if(!$name.StartsWith('ReyondBroom-Windows-x64/')){throw 'Unexpected package layout'};$relative=$name.Substring('ReyondBroom-Windows-x64/'.Length);if(!$relative){continue};$target=Under $stage $relative;$total+=$entry.Length;if($total -gt 2147483648){throw 'Expanded update exceeds 2 GiB'};if($name.EndsWith('/')){New-Item -ItemType Directory -Force $target | Out-Null}else{New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($target)) | Out-Null;[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target)}}}finally{$zip.Dispose()}
 if((!(Test-Path "$stage/fteqw64.exe") -and !(Test-Path "$stage/bin/fteqw64.exe")) -or !(Test-Path "$stage/fteplug_ftebroom_x64.dll")){throw 'Incomplete update'}
 if($Mode -eq 'prepare'){Status 'Update verified. Restart the editor to finish updating.' 'ready'}else{Status 'Update downloaded. It will be installed after you close the editor.' 'ready'}
 if($process){$process.WaitForExit()}
 Status 'Installing update...' 'installing'
 $backup=Join-Path $updates ('backup-'+[guid]::NewGuid());New-Item -ItemType Directory -Path $backup | Out-Null
 foreach($file in Get-ChildItem -LiteralPath $stage -File -Recurse){
  $relative=$file.FullName.Substring($stage.Length+1);if(Preserve $relative){continue};$target=Under $Root $relative;$saved=Under $backup $relative
  $existed=Test-Path -LiteralPath $target;if($existed){New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($saved)) | Out-Null;Copy-Item -LiteralPath $target -Destination $saved}
  $changed+=,@($target,$saved,$existed);New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($target)) | Out-Null
  $temporary=$target+'.ftebroom-update.tmp';Copy-Item -LiteralPath $file.FullName -Destination $temporary -Force;Move-Item -LiteralPath $temporary -Destination $target -Force
 }
 if((Test-Path "$stage/bin/fteqw64.exe") -and (Test-Path "$Root/fteqw64.exe")){
  $target=Under $Root 'fteqw64.exe';$saved=Under $backup 'fteqw64.exe'
  Copy-Item -LiteralPath $target -Destination $saved -Force
  $changed+=,@($target,$saved,$true);Remove-Item -LiteralPath $target
 }
 try{CleanupUpdates $backup}catch{} # Cache cleanup must not roll back a successful install.
 Status 'Update installed. Your settings and documents were preserved. Start ReyondBroom normally.' 'installed'
 $restart=Join-Path $updates 'restart.request'
 if($Mode -eq 'prepare' -and $Session -and (Test-Path -LiteralPath $restart) -and ([IO.File]::ReadAllText($restart).Trim() -eq $Session)){
  Remove-Item -LiteralPath $restart -Force
  $launcher=Join-Path $Root 'ReyondBroom.exe'
  try{Start-Process -FilePath $launcher -WorkingDirectory $Root | Out-Null}catch{Status 'Update installed. Start ReyondBroom normally to continue.' 'installed'}
 }
} catch {
 foreach($item in $changed){try{if($item[2]){Copy-Item -LiteralPath $item[1] -Destination $item[0] -Force}else{Remove-Item -LiteralPath $item[0] -Force}}catch{}}
 Status ('Update failed: '+$_.Exception.Message) 'error';exit 1
} finally {
 if($process){$process.Dispose()}
 if($mutex){try{$mutex.ReleaseMutex()}catch{};$mutex.Dispose()}
 # Failed updates retain staging and rollback files for diagnosis.
}
