# Virtual-Storage-Fault-Lab Dart / Flutter Windows SDK

`dart pub add virtual_storage_fault_lab` or `flutter pub add virtual_storage_fault_lab`.

```dart
final client=await Client.open(); // import package:virtual_storage_fault_lab/virtual_storage_fault_lab.dart
try { print(client.call("status")); } finally { client.close(); }
```

Windows x64 only. Calls are synchronous; use a worker isolate for UI responsiveness. For compiled Dart or Flutter Windows deployment, explicitly copy `lib/native/VirtualStorageFaultLabJsonSDK.dll` beside the executable or pass its absolute libraryPath. This is an FFI package; automatic Flutter DLL bundling is not provided. A full Flutter application has not been compiled in this validation. [API](doc/API.md). Matching driver/license required separately.
