# Prebuilt application host test support

The Subversion application binding tests compile private AQVMT, QVM, and WQVM host implementation files directly. This Git mirror supplies those same host classes through `target/lib/<configuration>/QuadooApplicationHostTests.lib` (and `x64/QuadooApplicationHostTests.lib`) so the tests can build without the private QuadooScript source tree.

The libraries were built from the local Subversion working tree on November 22, 2026, using Visual C++ 2008 in all four Debug/Release and Win32/x64 configurations. They contain only the QuadooScript host implementation files referenced by the original `QuadooApplicationTests.vcproj`. They use that project's compiler settings and defines, including `QUADOO_ASYNC_TESTS`, with compiler debug information disabled so the published libraries do not require a compiler PDB. The script adapter and test code still compile from the mirrored sources.

Required host class headers are under `platform/QuadooScript/Published/TestSupport`, preserving AQVMT/QVM/WQVM subfolders. Their relative includes are adjusted to the mirrored published interfaces and QuadooForm sources. These declarations and libraries are test support; application code should use the regular published QuadooScript interfaces.

When updating these classes, refresh the headers and all four host libraries together from the corresponding Subversion revision. The regular SimbeyCore, JSON, QuadooParser, and QuadooVM libraries and runtime binaries must also match their published headers. Release runtime copies in `bin` have been refreshed, while the configuration-specific copies in `target` allow Debug and Release to run with their matching dependencies.

The mirrored script adapter corrects the StatusBar Panels[index] dispatch condition found by the imported application tests. The published AQVMT.bin and AQVMDbg.exe were rebuilt for all four configurations against that corrected adapter using temporary copies of their private project files; no private project or implementation source is required to build this mirror.

The application hosts and host test support libraries also include the Subversion sound filename validation fix. QuadooVM DLLs and import libraries are refreshed from the Subversion build containing the native default-forwarding stack fix. Both regression suites pass in all four configurations.
