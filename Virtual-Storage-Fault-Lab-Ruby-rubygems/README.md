# Virtual-Storage-Fault-Lab Ruby SDK

`gem install virtual-storage-fault-lab`

Windows x64 Ruby3.1+.

```ruby
require "virtual_storage_fault_lab"
VirtualStorageFaultLabSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
