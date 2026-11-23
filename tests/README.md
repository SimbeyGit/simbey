# Quadoo Studio and QuadooForm tests

These folders retain the Subversion test layout. The solutions and projects target Visual Studio 2008 and support Debug and Release on Win32 and x64.

| Solution | Tests |
| --- | --- |
| `platform/QuadooForm/QuadooFormTests.sln` | Native internals, public runtime harness, extension DLL, and standalone designer |
| `desktop/QuadooStudio/QuadooStudioTests.sln` | Studio integration and application script bindings |

Open either solution, select the configuration and platform, and build. Set the desired test executable as the startup project. For `QuadooApplicationTests`, set Debugging > Command Arguments to the full path to `tests/desktop/QuadooStudio/ApplicationTests/Runtime.quadoo`. Other suites run without arguments. The application suite also supports the focused selectors used by the feature runners.

From the repository root, build and run all five suites with:

```bat
tests\desktop\QuadooStudio\build-studio.cmd Debug Win32 --test
tests\desktop\QuadooStudio\build-studio.cmd Release x64 --test
```

To build only the form and designer suites on Win32:

```bat
tests\platform\QuadooForm\build-designer.cmd Debug --test
```

The product solutions do not include tests. Test solutions build their required product projects. The mirrored projects link to the published SimbeyCore, JSON, and QuadooScript import libraries under `target/lib/<configuration>`, with an `x64` subfolder for 64-bit builds. Matching prebuilt runtime dependencies are included under `target/<configuration>` and `target/<configuration>/x64`; private dependency projects are not needed.

`StageRuntime.cmd` copies matching runtimes and designer examples next to test executables. Test executables, intermediates, scratch files, and logs belong under `tests/results`, which is ignored by Git. The main command-line runners use `RunOnTestDesktop.cmd` and the existing `TestDesktop.cpp` helper to run each suite on a separate Windows desktop, keeping test windows off the user's input desktop. Visual Studio launches tests on the current desktop; allow the suites to exercise focus and dialogs without interacting with their windows.

The application binding tests use a prebuilt `QuadooApplicationHostTests.lib` for the internal QuadooScript host classes they exercise. See [ApplicationTests/HostSupport.md](desktop/QuadooStudio/ApplicationTests/HostSupport.md) for details.

Sound filename validation and native default forwarding are fixed in the supplied application hosts, VM DLLs, and host test support libraries. The strengthened `--sound-headless` and `--async-only` tests pass in Debug and Release on both Win32 and x64. They check embedded-NUL filenames in wide and UTF-8 strings for both sound methods, and require native default invocation to succeed and execute exactly once.

Dependency provenance: QuadooParser DLLs and import libraries are now rebuilt from the current Subversion sources with the EOF regression fixed, in Debug and Release for Win32 and x64. The earlier compatible-parser workaround is no longer needed. SimbeyCore, JSON, and QuadooVM continue to use the matching published target configurations; the application hosts retain the StatusBar and sound fixes.

To run the focused EOF regression suite after building the application test project:

```bat
tests\RunOnTestDesktop.cmd tests\results\bin\Debug\x64\QuadooApplicationTests.exe --parser-eof-only
```

The suite covers initialized, typed, array, and scoped static globals; missing/LF/CRLF final newlines; file and text compilation; debug metadata; and rejection of truncated or malformed source.
