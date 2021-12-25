$src = 'https://chrisguzak.pkgs.visualstudio.com/_packaging/ChrisGuzak/nuget/v3/index.json'
$name = (Split-Path -Leaf $PSCommandPath).Replace('Package-', '').Replace('.ps1', '')
Write-Host $name

nuget.exe pack "$name.nuspec"

$latest = Get-ChildItem -Filter "$name.*.nupkg" |
    Sort-Object -Descending {[version]$(if ($_.Name -match '\d+(\.\d+)+') { $matches[0] } else { '0.0.0'}) } |
    Select-Object -First 1

nuget.exe push -Source $src -ApiKey VSTS $latest.FullName

$PSScriptRoot
