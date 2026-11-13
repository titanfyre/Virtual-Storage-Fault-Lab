# Virtual-Storage-Fault-Lab PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module VirtualStorageFaultLab -Scope CurrentUser`.

```powershell
$client=Connect-VirtualStorageFaultLabSDK
try { Invoke-VirtualStorageFaultLabSDK -Client $client -Operation status } finally { Disconnect-VirtualStorageFaultLabSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
