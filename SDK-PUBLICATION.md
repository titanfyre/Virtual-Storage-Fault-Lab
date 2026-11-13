# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | virtual-storage-fault-lab |
| CPP/conan | published_custom_source_index | [virtual-storage-fault-lab](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | paused_pending_owner_email_confirmation | Virtual-Storage-Fault-Lab.Native |
| NET/nuget | built_verified_requires_trusted_publishing_policy | VirtualStorageFaultLab.SDK [Source](Virtual-Storage-Fault-Lab-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | virtual-storage-fault-lab [Source](Virtual-Storage-Fault-Lab-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [virtual-storage-fault-lab](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [virtual-storage-fault-lab](https://crates.io/crates/virtual-storage-fault-lab) [Source](Virtual-Storage-Fault-Lab-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [virtual-storage-fault-lab](https://www.npmjs.com/package/virtual-storage-fault-lab) [Source](Virtual-Storage-Fault-Lab-JavaScript-npm/) |
| Java/maven | published_install_verified | [virtual-storage-fault-lab](https://central.sonatype.com/artifact/io.github.titancsa/virtual-storage-fault-lab/1.0.0) [Source](Virtual-Storage-Fault-Lab-Java-maven/) |
| Java/jitpack | published_install_verified | [virtual-storage-fault-lab](https://jitpack.io/#titanfyre/Virtual-Storage-Fault-Lab/java-sdk-v1.0.0) [Source](Virtual-Storage-Fault-Lab-Java-maven/) |
| Go/modules | published_install_verified | [virtual-storage-fault-lab](https://pkg.go.dev/github.com/titanfyre/Virtual-Storage-Fault-Lab/Virtual-Storage-Fault-Lab-Go-modules@v1.0.0) [Source](Virtual-Storage-Fault-Lab-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Virtual-Storage-Fault-Lab/Virtual-Storage-Fault-Lab-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [VirtualStorageFaultLab](https://www.powershellgallery.com/packages/VirtualStorageFaultLab/1.0.0) [Source](Virtual-Storage-Fault-Lab-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | virtual_storage_fault_lab [Source](Virtual-Storage-Fault-Lab-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [virtual-storage-fault-lab](https://rubygems.org/gems/virtual-storage-fault-lab) [Source](Virtual-Storage-Fault-Lab-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/virtual-storage-fault-lab](https://packagist.org/packages/titancsa/virtual-storage-fault-lab) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
