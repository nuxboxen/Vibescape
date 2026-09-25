# Vibescape source and dependencies

## Scope and safety

This is a personal learning fork. We do not plan to distribute prebuilt Vibescape
binaries, installers, or binary release archives. Read the
[project scope and security guidance](README.md#security-and-running-code) before
using any checkout, setup, or build commands. We have not audited this fork and
cannot establish that it is safe to build or run.

## Initial import

- Date: 2026-09-24 (UTC)
- Official upstream: https://gitlab.com/inkscape/inkscape.git
- Starting Inkscape commit: `02fb211a6c4065c770c7bd01e58c15cfaf69f4f8`
- GitHub home: https://github.com/nuxboxen/Vibescape
- Machine-readable inventory: [vibescape-dependencies.json](vibescape-dependencies.json)

The repositories retain upstream commit history, branches, tags, licenses, and attribution. This is a cross-host development fork, so GitHub does not display a native fork relationship to GitLab.

## Source dependency inventory

All eight direct submodules and all five nested extension submodules are mirrored under the same GitHub account. The current Vibescape development branch resolves every source submodule from those GitHub repositories.

| Checkout path | GitHub repository | Original pinned commit |
| --- | --- | --- |
| `share/extensions` | [Vibescape-extensions ](https://github.com/nuxboxen/Vibescape-extensions) | `87ece23a2fac1a8174d93e35354b84e9465f57e1` |
| `src/3rdparty/2geom` | [Vibescape-lib2geom ](https://github.com/nuxboxen/Vibescape-lib2geom) | `c7d8378ccc109bdb2ed30f0c8407725e93ffdd9b` |
| `share/themes` | [Vibescape-themes ](https://github.com/nuxboxen/Vibescape-themes) | `a8a8375fbe8672bb1a273be7aa32c06564936e29` |
| `src/3rdparty/libcroco` | [Vibescape-libcroco ](https://github.com/nuxboxen/Vibescape-libcroco) | `d851af79660943cb8b6feaa9c7cc9d39eb3b4686` |
| `po` | [Vibescape-translations ](https://github.com/nuxboxen/Vibescape-translations) | `01264328bacc922634ce790a4a2a6bf3608ca45c` |
| `src/3rdparty/libdepixelize` | [Vibescape-libdepixelize ](https://github.com/nuxboxen/Vibescape-libdepixelize) | `73b07a45f01cf7c4b179d1e65cef22609e000419` |
| `src/3rdparty/libuemf` | [Vibescape-libuemf ](https://github.com/nuxboxen/Vibescape-libuemf) | `71aba3710b4548bd6df7b7bddcd17096a813efa5` |
| `src/3rdparty/capypdf` | [Vibescape-capypdf ](https://github.com/nuxboxen/Vibescape-capypdf) | `d789f48b43518bcd190e9d845c943e3978014f61` |
| `share/extensions/other/gcodetools` | [Vibescape-extensions-gcodetools ](https://github.com/nuxboxen/Vibescape-extensions-gcodetools) | `f799a9ac8707cb08f2b04602c22a12c6d7acb9c7` |
| `share/extensions/other/inkman` | [Vibescape-extension-manager ](https://github.com/nuxboxen/Vibescape-extension-manager) | `267734e94609fe9041a3c69da6430e7a90b8cd6f` |
| `share/extensions/other/clipart` | [Vibescape-inkscape-import-clipart ](https://github.com/nuxboxen/Vibescape-inkscape-import-clipart) | `8d2948ce2fe82b195fec99f909049a8fdd64298e` |
| `share/extensions/other/extension-xaml` | [Vibescape-extension-xaml ](https://github.com/nuxboxen/Vibescape-extension-xaml) | `3a16aa7372c96559a6af918f54e096dc5de16081` |
| `share/extensions/other/extension-afdesign` | [Vibescape-extension-afdesign ](https://github.com/nuxboxen/Vibescape-extension-afdesign) | `b2288b5f371685161ff87b70788a5cddccf7f4a8` |

The extensions checkout uses commit `c6442355c0fa92ea6ccf1701b3e5b86278296725`, whose parent is the original pinned commit. Its only change is replacing the five nested submodule URLs in `.gitmodules`. That commit is retained on `codex/vibescape-dependencies` in the extensions repository. Every other source dependency retains its original pinned commit.

Historical branches and tags remain as imported, including their historical upstream URLs. The GitHub-only submodule wiring applies to the current Vibescape branch.

## Clone and update the complete source

The integrated development branch is `master`. The explicit branch below avoids selecting an imported historical branch through the repository default.

```sh
git clone --branch master --recurse-submodules https://github.com/nuxboxen/Vibescape.git
cd Vibescape
git submodule status --recursive
```

If the repository was cloned without submodules:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

Use the pinned commits recorded by the main repository. Avoid `git submodule update --remote` unless intentionally upgrading dependencies.

## Platform build requirements

The source dependencies are included through the mirrored submodules. Compilers, GTK/GLib, Cairo, Boost, Python packages, and other platform libraries still come from their normal package managers. The repository retains upstream dependency declarations, package lock files where present, and build documentation; it is not an offline operating-system package bundle.

Start with [the build guide](doc/building/readme.md), [Windows/MSYS2](doc/building/windows.md), [Linux](doc/building/linux.md), or [macOS](doc/building/mac.md). CMake requirements are defined in [CMakeScripts/DefineDependsandFlags.cmake](CMakeScripts/DefineDependsandFlags.cmake).

When an upstream guide gives the Inkscape GitLab clone command, substitute the Vibescape recursive clone command above. Existing GitLab CI configuration and installer definitions are retained as upstream material. They do not imply planned Vibescape binary distribution; no such distribution is planned.

The initial import verified repository and submodule integrity. The first local Windows development build completed on 2026-09-24 and opened the editor. See [the Vibescape Windows build notes](doc/building/vibescape-windows.md) for the reproducible setup, validation results, and remaining metafile test failures. Use `Build-Windows.ps1` for incremental builds, `Build-Windows.ps1 -Action Install` to fully refresh the local runnable folder, and `Run-Vibescape.cmd` to launch with separate Vibescape preferences.

## Bringing in upstream improvements

Keep GitHub as `origin` and add Inkscape as `upstream`:

```sh
git remote add upstream https://gitlab.com/inkscape/inkscape.git
git fetch upstream --tags
```

Review and merge selected upstream changes on a development branch. If upstream changes any submodule pin, first ensure that exact commit is present in the corresponding GitHub mirror, preserve the GitHub URLs, update [vibescape-dependencies.json](vibescape-dependencies.json), and repeat the recursive clone check. If the extensions pin changes, apply its nested URL changes on top of that new pinned version.

Do not mirror-push upstream over the modified Vibescape branch: doing so would discard fork-specific commits.

## Attribution

Vibescape is an independent personal fork, not an official Inkscape release. All original copyright notices and license files remain in place. See [COPYING](COPYING), [AUTHORS](AUTHORS), and the license files inside each dependency.
