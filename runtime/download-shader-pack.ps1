param([Parameter(Mandatory=$true)][string]$Destination,[Parameter(Mandatory=$true)][string]$Manifest)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $root | Out-Null
try {
 [IO.File]::Delete((Join-Path $root 'error.txt'))
 $spec=Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
 if(!$spec.url.StartsWith('https://github.com/reyond9079/Reyond-FTE-Assets/releases/download/') -or $spec.size -le 0 -or $spec.size -gt 67108864){throw 'Invalid shader download manifest.'}
 $archive=Join-Path $root 'pack.part'
 $request=[Net.HttpWebRequest]::Create($spec.url);$request.Timeout=45000
 $response=$request.GetResponse();$inputStream=$response.GetResponseStream();$output=[IO.File]::Create($archive)
 try {$buffer=New-Object byte[] 65536;$received=0
  while(($count=$inputStream.Read($buffer,0,$buffer.Length)) -gt 0){$received+=$count;if($received -gt $spec.size){throw 'Download exceeds pinned size.'};$output.Write($buffer,0,$count);[IO.File]::WriteAllText((Join-Path $root 'progress.txt'),"$received $($spec.size) Downloading")}
 }finally{$output.Dispose();$inputStream.Dispose();$response.Dispose()}
 $sha=[Security.Cryptography.SHA256]::Create();$hashStream=[IO.File]::OpenRead($archive)
 try{$archiveHash=([BitConverter]::ToString($sha.ComputeHash($hashStream))).Replace('-','').ToLowerInvariant()}finally{$hashStream.Dispose();$sha.Dispose()}
 if($received -ne $spec.size -or $archiveHash -ne $spec.sha256){throw 'Shader archive checksum mismatch.'}
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $zip=[IO.Compression.ZipFile]::OpenRead($archive)
 $ready=Join-Path $root 'ready.txt';if(Test-Path -LiteralPath $ready){Remove-Item -LiteralPath $ready}
 try {
  $expected=@{};foreach($property in $spec.files.PSObject.Properties){$expected[$property.Name]=$property.Value}
  if($zip.Entries.Count -ne $expected.Count){throw 'Unexpected archive entries.'}
  $seen=@{};$total=0;$content=Join-Path $root 'content';New-Item -ItemType Directory -Force -Path $content | Out-Null
  foreach($entry in $zip.Entries){
   $name=$entry.FullName;$total+=$entry.Length
   if($seen.ContainsKey($name) -or !$expected.ContainsKey($name) -or $name -match '(^/|\\|:|[\r\n]|(^|/)\.\.?(/|$))' -or $total -gt 67108864 -or $entry.Length -gt 16777216){throw 'Unsafe shader archive entry.'}
   $seen[$name]=$true;$fileSpec=$expected[$name]
   $target=[IO.Path]::GetFullPath((Join-Path $content $name))
   if(!$target.StartsWith($content+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $entry.Length -ne $fileSpec.size){throw 'Invalid shader file path or size.'}
   New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($target)) | Out-Null
   $stream=$entry.Open();$memory=New-Object IO.MemoryStream
   try{$stream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
   $sha=[Security.Cryptography.SHA256]::Create();try{$hash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
   if($hash -ne $fileSpec.sha256){throw 'Shader file checksum mismatch.'}
   [IO.File]::WriteAllBytes($target,$bytes)
  }
 }finally{$zip.Dispose()}
 [IO.File]::WriteAllLines((Join-Path $root 'files.txt'),[string[]]($expected.Keys | Sort-Object),(New-Object Text.UTF8Encoding($false)))
 [IO.File]::WriteAllText((Join-Path $root 'ready.txt'),"$($spec.version) $($spec.sha256)")
 [IO.File]::WriteAllText((Join-Path $root 'progress.txt'),"$received $received Ready")
 Remove-Item -LiteralPath $archive
}catch{[IO.File]::WriteAllText((Join-Path $root 'error.txt'),$_.Exception.Message);exit 1}
