# Vibescape

Vibescape is a personal development fork of [Inkscape](https://inkscape.org/), hosted on GitHub under [nuxboxen](https://github.com/nuxboxen).

The initial import preserves upstream Git history, branches, tags, licenses, and credits. All 13 recursive source submodules are hosted in companion `nuxboxen/Vibescape-*` repositories.

## Get the complete source

```sh
git clone --recurse-submodules https://github.com/nuxboxen/Vibescape.git
cd Vibescape
```

For an existing checkout:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

See [Vibescape setup and dependency provenance](VIBESCAPE.md) for the exact source snapshot, dependency inventory, platform build requirements, and upstream update workflow.

## Development status

This is a source fork of Inkscape's development branch. Application names, artwork, and executable names still follow upstream Inkscape. No Vibescape binary release or build validation is implied by this import.

## Inkscape and licensing

Inkscape is a free and open source SVG vector graphics editor for artistic and technical illustrations, including logos, typography, diagrams, and drawings.

Original authorship and licensing are retained in [AUTHORS](AUTHORS), [COPYING](COPYING), and [LICENSES](LICENSES/). Consult the licenses of individual files and dependency repositories.

- [Upstream source](https://gitlab.com/inkscape/inkscape)
- [User documentation](https://inkscape.org/learn/)
- [Developer documentation](doc/readme.md)
- [Vibescape issues](https://github.com/nuxboxen/Vibescape/issues)
