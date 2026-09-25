# Vibescape on Windows

The local checkout is `C:\Users\winbo\Projects\Vibescape`.
Double-click `Run-Vibescape.cmd` in the repository root to open our build.
It launches `build/windows/install/bin/inkscape.exe` with a separate
application identity and preferences in `%APPDATA%\Vibescape`.
You can also run `Run-Vibescape.ps1`, optionally passing one SVG file.
The application still uses Inkscape's original name and branding.

Use the launcher for normal work: opening `inkscape.exe` directly still uses
Inkscape's default profile. The other executable under `build/windows/bin`
is the compiler output and does not have the complete packaged runtime beside it.

## Rebuild this configured checkout

From PowerShell in the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build-Windows.ps1
```

The default action, `Build`, reuses the CMake configuration and compiles only
changed inputs. It then updates the runnable folder when application binaries,
resources under `share/`, translations, or CMake configuration change. An
unchanged build skips installation entirely. Existing launchers keep pointing
at `build/windows/install/bin/inkscape.exe`.

| Action | Command | Purpose |
| --- | --- | --- |
| Build | `.\Build-Windows.ps1` | Incrementally build and refresh the runnable folder when needed. |
| Install | `.\Build-Windows.ps1 -Action Install` | Reconfigure, build, and force a full refresh of the local runnable folder. |
| Configure | `.\Build-Windows.ps1 -Action Configure` | Run configuration without compiling or installing. |

The default is 12 parallel jobs. Use `-Jobs 4` to reduce CPU use.
Use `-Fresh` after changing dependency locations or versions; this refreshes
the CMake cache and then performs the selected action. Dependencies are not
monitored automatically. Use `-Action Install` to repair missing runtime files;
the fast path checks the main executable's presence, not every installed file.

The script uses MSYS2 UCRT64, RelWithDebInfo, static internal Inkscape libraries,
and the pinned internal lib2geom. All installation stays inside the local build
folder. It does not install system-wide or change file associations. Close
Vibescape before rebuilding changed application code so Windows can replace
its executable and libraries. Release archives and installers remain separate
future work.

Logs are in `build/windows/logs/`. The package inventory is
`build/windows/logs/msys2-packages.txt`, refreshed by explicit configuration.

## Initial environment

Install official [MSYS2](https://www.msys2.org/) at `C:\msys64`.
Update it with `pacman -Syu`, restarting the terminal and repeating when requested.
Then run the following from an MSYS2 **UCRT64** terminal:

```bash
cd /c/Users/winbo/Projects/Vibescape
bash buildtools/msys2installdeps.sh
```

The first setup on 2026-09-24 used GCC 16.2.0, CMake 4.4.3, Ninja 1.13.2,
GTK 4.24.0 and GLib 2.90.0. The MSYS2 repository supplied gtkmm 4.20.0 and
glibmm 2.86.0, which were incompatible with those newer C libraries.
The first compile failed on the conflicting GdkCursorClass declaration.

The matching official GNOME releases were built privately under
`build/windows/deps`: gtkmm 4.24.0 and glibmm 2.90.0. Their installed DLLs,
headers and pkg-config files take precedence only for this build.
The MSYS2 package files are not patched or replaced.

### Build matching C++ bindings

These commands reproduce the compatibility setup for the versions above.
Future MSYS2 versions may supply matching packages directly.

From the repository root in UCRT64:

```bash
set -euo pipefail
mkdir -p build/windows/vendor
cd build/windows/vendor
for spec in glibmm/2.90/glibmm-2.90.0 gtkmm/4.24/gtkmm-4.24.0; do
    name="${spec##*/}"
    curl -fL --retry 3 -o "$name.tar.xz" "https://download.gnome.org/sources/$spec.tar.xz"
    curl -fL --retry 3 -o "$name.sha256sum" "https://download.gnome.org/sources/$spec.sha256sum"
    sha256sum --ignore-missing -c "$name.sha256sum"
    tar -xf "$name.tar.xz"
done
cd ../../..

prefix="$(cygpath -m "$PWD/build/windows/deps")"
meson setup build/windows/glibmm-build build/windows/vendor/glibmm-2.90.0 \
    --prefix "$prefix" --libdir lib --buildtype release --wrap-mode=nofallback \
    -Dmaintainer-mode=false -Dbuild-documentation=false -Dbuild-examples=false
meson compile -C build/windows/glibmm-build -j 12
meson install -C build/windows/glibmm-build

export PKG_CONFIG_PATH="$PWD/build/windows/deps/lib/pkgconfig"
export PATH="$PWD/build/windows/deps/bin:$PATH"
meson setup build/windows/gtkmm-build build/windows/vendor/gtkmm-4.24.0 \
    --prefix "$prefix" --libdir lib --buildtype release --wrap-mode=nofallback \
    -Dmaintainer-mode=false -Dbuild-documentation=false \
    -Dbuild-demos=false -Dbuild-tests=false
meson compile -C build/windows/gtkmm-build -j 12
meson install -C build/windows/gtkmm-build
```

Then run `Build-Windows.ps1 -Fresh` from PowerShell.

If Git for Windows created the checkout with `core.autocrlf=true`, set that
same value in the repository-local configuration when using MSYS2 Git.
Use repository-local settings for submodules too. This avoids reporting
line-ending-only changes without rewriting the source files.

## Packaging adjustments

`CMakeScripts/InstallMSYS2.cmake` uses the detected GraphicsMagick version
instead of a fixed 1.3.45 directory, and installs the C++ binding DLLs selected
by pkg-config. This keeps the runtime libraries consistent with compilation.

For the existing test suite, Windows may refuse CMake's symbolic link.
A directory junction at `build/windows/inkscape_datadir/inkscape` pointing to
the source `share` folder provides the same test-resource path without
changing Windows security settings.


The package-list helper also places its temporary file in the build
directory, where its existing cleanup removes it. This prevents a
`list_files_pacman_temp.txt` file being left in the source root.

## Fast development builds

`Build-Windows.ps1` records its configuration arguments and reuses the cache
on ordinary builds. Ninja still reconfigures automatically when a CMake input
changes. `CMakeScripts/VibescapeWindows.cmake` adds a `vibescape-stage` target
that runs the existing complete install rules only when their tracked inputs
change. Adding or removing resources also refreshes the CMake file lists.

Installation still performs its normal DLL/resource copying and Python
precompilation when needed. Selected C++ binding DLLs are excluded from the
generic MSYS2 DLL list so the installer does not repeatedly replace them with
the distribution's versions before installing the selected versions.

The measured unchanged build took **2.22 seconds inside the helper**
(**2.63 seconds including PowerShell startup**), compared with the previous
38.42-second full configure/build/install repeat. It skipped configuration,
C/C++ compilation, linking, and installation, preserving the executable and
configuration/install stamp timestamps. This measures an already configured
checkout, not a clean build.

Additional checks passed for adding and editing a temporary palette: the
installed bytes matched the source, and editing an existing palette took
6.14 seconds without reconfiguration or recompilation. The fixture was
removed from source and installation afterward. The full refresh action
also completed successfully; the following unchanged build took 1.90 seconds and skipped staging, confirming the forced refresh left Ninja up to date.

Packaged version and PNG export checks ran without MSYS2 on PATH and with
private preferences. The PNG dimensions and color matched, bundled Python
imports passed with the installed extension directory on `PYTHONPATH`, and
selected binding DLL hashes matched the local dependencies. The generated
CapyPDF header hash was unchanged. CLI export returned zero but emitted locale
and GTK shutdown warnings, retained in `fast-build/export.log`.

Evidence for this path is in `build/windows/logs/fast-build/`. The earlier
CapyPDF-only measurement below remains the baseline.

## Incremental build fixes

The build helper writes its Bash commands to the ignored
`build/windows/run-build.sh` before running them. This preserves quoting in
Windows PowerShell 5.1 and PowerShell 7. Passing those commands directly on
Bash's command line caused the normal Windows PowerShell rebuild to fail
with `fresh: unbound variable`.

The CapyPDF rule in `src/3rdparty/CMakeLists.txt` now declares `capypdf.h`
as a generated output and tracks its template and CMake file as inputs.
An unchanged build therefore keeps the existing header instead of rewriting
it and recompiling its consumers. Changes to the template or version
definitions still regenerate the header.

Verification on 2026-09-24:

- The corrected helper built and installed successfully under Windows PowerShell 5.1.
- An unchanged repeat under PowerShell 7 passed in 38.42 seconds, including
  configuration and packaging. It performed zero C/C++ compilations or links.
- The regenerated header was byte-for-byte identical to the original.
  The repeat preserved both its timestamp and the executable's timestamp.
- Ninja's dependency graph includes both the template and the CMake file.
- The earlier build log had 25 steps, including unnecessary compilation and
  linking following header generation.

Evidence is in `build/windows/logs/incremental-build/`: `before-build.log`,
`first-fixed-build.log`, `repeat-build.log`, and `result.json`.
This measures a repeat build of the configured checkout, not a clean build.

## Validation

The first Windows x64 build completed on 2026-09-24 from commit
`6df7f7647e14e3f3003607fae434a3e425498a86`, with the local build/packaging
changes described here. No application source files or dependency pins changed.

- Full compile and installation into `build/windows/install`: passed.
- Reconfiguration and build-temporary-file cleanup: passed.
- Main executable's packaged DLL dependency check: passed.
- Standalone version check and PNG/PDF exports with MSYS2 removed from PATH: passed.
- Bundled Python imports, including inkex, NumPy, Pillow, PyGObject, Cairo,
  requests and BeautifulSoup: passed. The installer now includes soupsieve,
  typing_extensions and charset_normalizer.
- Installed PNG matches the reference in alpha and all visible pixels.
  Only hidden RGB values under fully transparent pixels differ.
- Exported PDF rendered with Poppler matches the reference exactly (RMSE 0).
- Selected upstream CTest run: 19 passed, 2 failed, 1 skipped (22 total).
  The EMF image comparison had 6.405% error against a 0% tolerance; WMF had
  63.764% against 0.5%. The ImageMagick-rendered WMF output appeared black.
  These remain unresolved export/reference-comparison limitations.
- The upstream PDF comparison skipped because ImageMagick attempted to invoke
  missing `gswin32c`. The separate Poppler check above verified PDF output.
- The graphical editor opened `shapes.svg` with the expected build version.
  Automatic visual inspection was unavailable: the desktop tool reported a
  window-owner mismatch even though its expected and current owner strings
  were identical. Hands-on GUI acceptance is still pending.
- Windows locale warnings appeared; the tested commands returned successfully.

The image-comparison tests additionally used MSYS2 packages `bc` and
`mingw-w64-ucrt-x86_64-imagemagick`. This was focused build validation,
not the entire Inkscape test suite or a release certification.

Evidence is under `build/windows/logs`: `build-artifact.json`,
`cli-tests.log`, `runtime-dll-check.log`, `python-smoke.log`,
`installed-smoke.log`, `png-comparison.json`, and `pdf-comparison.json`.
Failed metafile images remain under `build/windows/testfiles/cli_tests`.
The earlier GUI smoke check used the private profile `build/windows/smoke/gui-profile`.

The executable retains debug information and is about 1.48 GB. Keep the
whole installation folder together; the executable needs its adjacent
libraries and resource folders. No MSI/EXE installer was created. The local
build output and validation logs are ignored by Git; the build scripts and
instructions are maintained in this repository.
