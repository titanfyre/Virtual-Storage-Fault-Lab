$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'lib/VirtualStorageFaultLabJsonSDKPowerShell.dll')
function Connect-VirtualStorageFaultLabSDK { <# .SYNOPSIS Opens an inert product SDK session. #> [CmdletBinding()]param() [TitanCSA.Products.VirtualStorageFaultLab.PowerShell.Client]::new() }
function Invoke-VirtualStorageFaultLabSDK { <# .SYNOPSIS Calls a documented product operation with an argument object. #> [CmdletBinding()]param([Parameter(Mandatory)]$Client,[Parameter(Mandatory)][string]$Operation,[hashtable]$Arguments=@{}) $Client.Call($Operation,($Arguments|ConvertTo-Json -Depth 100 -Compress)) | ConvertFrom-Json -Depth 100 }
function Disconnect-VirtualStorageFaultLabSDK { <# .SYNOPSIS Closes the SDK session and owned resources. #> [CmdletBinding()]param([Parameter(Mandatory)]$Client) $Client.Dispose() }
Export-ModuleMember -Function Connect-VirtualStorageFaultLabSDK,Invoke-VirtualStorageFaultLabSDK,Disconnect-VirtualStorageFaultLabSDK
