#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The WebAssembly extension conformance suite.
#
# Every module here is hand-written .wat assembled by `water`, with no compiler anywhere in
# the chain. That is deliberate on two counts: a failure can only be the host, never a code
# generator; and it keeps the suite honest about the API being reachable from outside the
# toolchain that happens to be developed alongside it.
#
# Roughly half the cases are refusals -- wrong-kind handles, out-of-bounds spans, mismatched
# signatures, failed runs -- which a well-behaved generated module would never produce, and
# which are exactly where a plugin API earns its keep.
#
# It doubles as the engine-substitution suite. Swapping the WebAssembly engine for another
# one providing the §7.1 embedding interface means rebuilding with WASM_ROOT pointing at it
# and running exactly this: instantiation, import resolution by two-part name, export lookup
# by name, linear memory, traps and host calls are the whole surface an engine has to get
# right for extensions, and nothing here reaches past it.
#
# "§7.1", not "the community wasm-c-api header": that header renders the pre-3.0 appendix and
# omits operations the spec defines and this host uses, so an engine shipping it as-is needs
# a few declarations added first. The suite is what makes the cost measurable -- it fails on
# exactly what is missing, rather than leaving substitutability as an assertion. A suite built from a language binding could not serve that
# purpose -- it would test the engine and the toolchain together, and require building a
# compiler nobody evaluating an engine has any reason to build.
#
#
# NOT COVERED HERE, because it needs a window and someone to click: that a failure raises a
# dialog rather than only a console warning; cancelling a running plugin (the false answer is
# tested, the true one needs the Cancel button); getScreenCTM returning a transform that
# tracks zoom and rotation, rather than the absent it reports headless; a real selection and
# current layer; and single-step undo preserving object identity. Live preview is not
# implemented at all -- newDocCache is the hook and the backend does not use it yet.
#
# Usage: run-tests.sh [path-to-inkscape] [path-to-wat-assembler]

set -e

here=$(cd "$(dirname "$0")" && pwd)
inkscape=${1:-$here/../../build/bin/inkscape}
water=${2:-/home/dan/Source/javelina/wasm/build/water}

# Point the binary at the shipped data, the way testfiles/CMakeLists.txt does for the CTest
# suite. Without it Inkscape runs with no units table at all -- getUnit("mm") comes back with
# an empty abbreviation and the log fills with "Units file  is missing" -- so anything reading
# a unit silently answers nothing. Every case here ran that way until a metrics test asked for
# the document's display unit and got an empty string back.
: "${INKSCAPE_DATADIR:=$here/../../build/inkscape_datadir}"
export INKSCAPE_DATADIR
if [ ! -d "$INKSCAPE_DATADIR/inkscape" ]; then
    printf 'warning: %s/inkscape does not exist; unit-dependent cases will fail.\n' "$INKSCAPE_DATADIR"
    printf '         ln -s %s/share %s/inkscape\n' "$here/../.." "$INKSCAPE_DATADIR"
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# A throwaway profile per run, for two reasons.
#
# Preferences PERSIST, which is what they are for -- so a case that writes one and a case that
# asserts one is absent cannot both be right twice. The preference case passed on the first run
# and failed on the second, against a key the first run had left behind.
#
# And without this the suite writes into the user's own preferences.xml. A test that edits the
# configuration of the machine it runs on is a bad test however green it is.
export INKSCAPE_PROFILE_DIR="$work/profile"
mkdir -p "$INKSCAPE_PROFILE_DIR"

pass=0
fail=0

# run_case <present|absent> <name> <id> <pattern> [reason]
#
# `reason`, where given, is matched against the run's diagnostics. It matters for the
# refusal cases: those assert that something is NOT in the document, which would hold just
# as well if the extension had never run at all. Requiring the diagnostic too is what makes
# them test a refusal rather than an absence.
# The optional sixth argument is the input document; blank.svg unless a case needs the
# richer fixture to walk and measure.
run_case() {
    mode=$1 name=$2 id=$3 pattern=$4 reason=$5 input=${6:-blank.svg}
    "$water" "$here/$name.wat" -o "$work/$name.wasm" >/dev/null 2>&1 || {
        printf 'ASSEMBLE FAIL %s\n' "$name"; fail=$((fail + 1)); return
    }

    # The .inx is generated rather than checked in: every case needs the same one but for
    # two strings, and a suite where adding a test means writing a boilerplate XML file is a
    # suite people stop adding tests to.
    cat > "$work/$name.inx" <<INX
<?xml version="1.0" encoding="UTF-8"?>
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<inkscape-extension xmlns="http://www.inkscape.org/namespace/inkscape/extension">
    <name>$name</name>
    <id>$id</id>
    <!-- max is mandatory here: an int parameter defaults to a maximum of 10, so without it
         every case number above ten is silently clamped and each run tests the same thing. -->
    <param name="case" type="int" min="0" max="100000" gui-hidden="true">${case_number:-0}</param>
    <!-- One parameter of every type the .inx schema declares, so the accessors have something
         to read. color, optiongroup, path and notebook have all been writable in an .inx far
         longer than anything could read them back. The path is absolute deliberately: a
         relative one resolves against the extension's base directory, which is a fresh
         temporary directory on every run, so the expected value would never be stable. -->
    <param name="teststring" type="string" gui-hidden="true" gui-text="s">initial</param>
    <param name="testcolor" type="color" gui-hidden="true" gui-text="c">#11223344</param>
    <param name="testpath" type="path" mode="file" gui-hidden="true" gui-text="p">/tmp/wasm-suite-path</param>
    <param name="testchoice" type="optiongroup" appearance="combo" gui-hidden="true" gui-text="o">
        <option value="alpha">Alpha</option>
        <option value="beta">Beta</option>
    </param>
    <param name="testbook" type="notebook" gui-hidden="true">
        <page name="one" gui-text="One"><label>one</label></page>
        <page name="two" gui-text="Two"><label>two</label></page>
    </param>
    <effect>
        <object-type>all</object-type>
        <effects-menu hidden="true"/>
    </effect>
    <wasm>
        <module entry="effect" location="inx">$name.wasm</module>
    </wasm>
</inkscape-extension>
INX

    out=$work/$name.svg
    rm -f "$out"
    INKSCAPE_EXTENSIONS_DIR="$work" "$inkscape" \
        --actions="$id; export-filename:$out; export-do" \
        "$here/$input" >"$work/$name.log" 2>&1 || true

    if [ ! -f "$out" ]; then
        printf 'NO OUTPUT     %s\n' "$name"; fail=$((fail + 1)); return
    fi

    if grep -q "$pattern" "$out"; then found=yes; else found=no; fi

    if ! { { [ "$mode" = present ] && [ "$found" = yes ]; } ||
           { [ "$mode" = absent  ] && [ "$found" = no  ]; }; }; then
        printf 'FAIL          %s (expected %s: %s)\n' "$name" "$mode" "$pattern"
        grep -i wasm "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1))
        return
    fi

    if [ -n "$reason" ] && ! grep -q "$reason" "$work/$name.log"; then
        printf 'FAIL          %s (no diagnostic matching: %s)\n' "$name" "$reason"
        grep -i wasm "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1))
        return
    fi

    printf 'ok            %s\n' "$name"
    pass=$((pass + 1))
}

# As run_case, but silent and returning a status, for the matrix below: printing 168 lines of
# "ok" would bury the ones that matter.
run_case_quiet() {
    qname=$1 qid=$2 qreason=$3 qinput=${4:-blank.svg}
    "$water" "$here/$qname.wat" -o "$work/$qname.wasm" >/dev/null 2>&1 || return 1
    cat > "$work/$qname.inx" <<INX
<?xml version="1.0" encoding="UTF-8"?>
<inkscape-extension xmlns="http://www.inkscape.org/namespace/inkscape/extension">
    <name>$qname</name>
    <id>$qid</id>
    <!-- max is mandatory here: an int parameter defaults to a maximum of 10, so without it
         every case number above ten is silently clamped and each run tests the same thing. -->
    <param name="case" type="int" min="0" max="100000" gui-hidden="true">${case_number:-0}</param>
    <effect>
        <object-type>all</object-type>
        <effects-menu hidden="true"/>
    </effect>
    <wasm>
        <module entry="effect" location="inx">$qname.wasm</module>
    </wasm>
</inkscape-extension>
INX
    # Retried once if Inkscape died before it got as far as running anything. Registering the
    # application with D-Bus throws on failure and nothing catches it, so a few hundred
    # launches in a row will eventually abort one during startup -- a property of the harness
    # hammering the session bus, not of the module under test. Counting that as "the host
    # accepted a bad handle" would be a false report, and a suite that fails at random is a
    # suite people stop believing.
    attempt=0
    while [ "$attempt" -lt 2 ]; do
        INKSCAPE_EXTENSIONS_DIR="$work" "$inkscape" \
            --actions="$qid; export-filename:$work/$qname.svg; export-do" \
            "$here/$qinput" >"$work/$qname.log" 2>&1 || true
        # Anything from the backend means the run reached the extension; only a startup
        # failure leaves no trace of it at all.
        if grep -q "WasmBackend" "$work/$qname.log"; then
            break
        fi
        attempt=$((attempt + 1))
    done
    grep -q "$qreason" "$work/$qname.log"
}

echo "── WebAssembly extension conformance ─────────────────────────"

# The document really is reachable and mutable.
run_case present hello-plugin    org.inkscape.effect.wasm.hello       '<rect'

# Coverage of the published interface. Each module runs a numbered series of checks against
# fixture.svg and leaves a marker element only if every one of them held, so the assertion
# below cannot pass for a module that never ran. A failure reports its check number in the
# log, which is the whole point of numbering them.
run_case present cover-traversal org.inkscape.effect.wasm.cover.traversal 'ok-traversal' '' fixture.svg
run_case present cover-mutation  org.inkscape.effect.wasm.cover.mutation  'ok-mutation'  '' fixture.svg
run_case present cover-geometry  org.inkscape.effect.wasm.cover.geometry  'ok-geometry'  '' fixture.svg
run_case present cover-text      org.inkscape.effect.wasm.cover.text      'ok-text'      '' fixture.svg
run_case present cover-query     org.inkscape.effect.wasm.cover.query     'ok-query'     '' fixture.svg
run_case present cover-edges     org.inkscape.effect.wasm.cover.edges     'ok-edges'     '' fixture.svg
run_case present cover-lifetime  org.inkscape.effect.wasm.cover.lifetime  'ok-lifetime'  '' fixture.svg
run_case present cover-spaces    org.inkscape.effect.wasm.cover.spaces    'ok-spaces'    '' fixture.svg
run_case present cover-metrics   org.inkscape.effect.wasm.cover.metrics   'ok-metrics'   '' fixture.svg
run_case present cover-typed-attrs org.inkscape.effect.wasm.cover.typedattrs 'ok-typed-attrs' '' fixture.svg
run_case present cover-style-write org.inkscape.effect.wasm.cover.stylewrite 'ok-style-write' '' fixture.svg
run_case present cover-session   org.inkscape.effect.wasm.cover.session   'ok-session'   '' fixture.svg
run_case present cover-lookup    org.inkscape.effect.wasm.cover.lookup    'ok-lookup'    '' fixture.svg
run_case present cover-path      org.inkscape.effect.wasm.cover.path      'ok-path'      '' fixture.svg
run_case present cover-pages     org.inkscape.effect.wasm.cover.pages     'ok-pages'     '' fixture.svg
run_case present cover-layers    org.inkscape.effect.wasm.cover.layers    'ok-layers'    '' fixture.svg
run_case present cover-arrange   org.inkscape.effect.wasm.cover.arrange   'ok-arrange'   '' fixture.svg
run_case present cover-reshape   org.inkscape.effect.wasm.cover.reshape   'ok-reshape'   '' fixture.svg
run_case present cover-adorn     org.inkscape.effect.wasm.cover.adorn     'ok-adorn'     '' fixture.svg
run_case present cover-host      org.inkscape.effect.wasm.cover.host      'ok-host'      '' fixture.svg
run_case present cover-pathbuild org.inkscape.effect.wasm.cover.pathbuild 'ok-pathbuild' '' fixture.svg

# Strings: fits / absent / needs-N are three distinct answers, and the bytes arrive intact.
# `needs` and `fits` are both 10 by design -- the result is the answer's length either way --
# so `untouched` is what separates them: it is the short call's buffer, which must come back
# as the sentinel it went in as. `retry` then spends that length and gets the value.
run_case present string-protocol org.inkscape.effect.wasm.test.string 'fits="10"'
run_case present string-protocol org.inkscape.effect.wasm.test.string 'roundtrip="abcdefghij"'
run_case present string-protocol org.inkscape.effect.wasm.test.string 'absent="1"'
run_case present string-protocol org.inkscape.effect.wasm.test.string 'needs="10"'
run_case present string-protocol org.inkscape.effect.wasm.test.string 'untouched="ZZZZZZZZZZ"'
run_case present string-protocol org.inkscape.effect.wasm.test.string 'retry="abcdefghij"'

# Refusals. In each, the module modifies the document first, so "absent" also proves rollback;
# the trailing pattern pins down which refusal actually fired.
run_case absent refuse-wrong-kind    org.inkscape.effect.wasm.test.wrongkind '<g' \
         'appendChild: not a Node handle'
run_case absent refuse-bounds        org.inkscape.effect.wasm.test.bounds    '<g' \
         "createElement: name is outside the module's memory"
run_case absent refuse-status        org.inkscape.effect.wasm.test.status    '<g' \
         'reported failure (1)'
run_case absent refuse-bad-signature org.inkscape.effect.wasm.test.badsig    '<g' \
         "unresolved import 'org.inkscape.Node.appendChild'"
run_case absent refuse-string-bounds org.inkscape.effect.wasm.test.strbounds '<g' \
         "getAttribute: result is outside the module's memory"

# The staging memory is nominated by name. This module exports one under a different name,
# which a host selecting on kind would accept -- and which stops identifying anything at all
# once a plugin exports the two memories WebAssembly 3.0 permits it.
run_case absent refuse-wrong-memory-name org.inkscape.effect.wasm.test.memname '<g' \
         "exports no memory named 'memory'"

# Handle rules. Null is what every failed query returns, so passing one on is the easiest
# mistake to make; an unissued index must not address anything at all.
run_case absent refuse-null-handle  org.inkscape.effect.wasm.test.nullhandle  '<g' \
         'nodeType: not a Node handle'
run_case absent refuse-stale-handle org.inkscape.effect.wasm.test.stalehandle '<g' \
         'nodeType: not a Node handle'
run_case absent refuse-list-kind    org.inkscape.effect.wasm.test.listkind    '<g' \
         'NodeList.length: not a NodeList handle'

# Registered, and refuses: the Inkscape call underneath crashes. Kept as a name so a caller is
# told what is wrong rather than left reading "unresolved import" as "not written yet". If this
# case ever passes, sp-symbol.cpp has been fixed and the capability should be reclaimed.
run_case absent refuse-blocked      org.inkscape.effect.wasm.test.blocked     '<g' \
         'unSymbol: blocked upstream'

# The write side of the bounds check, and DOM's NotFoundError cases.
run_case absent refuse-write-bounds org.inkscape.effect.wasm.test.writebounds 'scratch-wb' \
         'getBBox: result is outside' fixture.svg
run_case absent refuse-not-a-child  org.inkscape.effect.wasm.test.notachild   'scratch-nac' \
         'reference is not a child' fixture.svg

# ── the other ends of the vtable ────────────────────────────────────────────────────────
#
# An effect is one of the things an extension can be. 17 of the 160 shipped .inx files are
# <output> and 8 are <input>, and a plugin system that cannot be either of those is a plugin
# system for half the extensions Inkscape ships.
#
# run_output_case <name> <id> <suffix> <pattern>
#
# Exports the fixture through the module and greps what the module wrote. The .inx declares
# <output> rather than <effect>, and the entry point is named "save"; everything else about the
# module -- the handle table, the document API, the memory contract -- is the same.
run_output_case() {
    name=$1 id=$2 suffix=$3 pattern=$4
    "$water" "$here/$name.wat" -o "$work/$name.wasm" >/dev/null 2>&1 || {
        printf 'ASSEMBLE FAIL %s\n' "$name"; fail=$((fail + 1)); return
    }
    cat > "$work/$name.inx" <<INX
<?xml version="1.0" encoding="UTF-8"?>
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<inkscape-extension xmlns="http://www.inkscape.org/namespace/inkscape/extension">
    <name>$name</name>
    <id>$id</id>
    <output>
        <extension>.$suffix</extension>
        <mimetype>application/x-wasm-suite-$suffix</mimetype>
        <filetypename>$name</filetypename>
        <filetypetooltip>$name</filetypetooltip>
    </output>
    <wasm>
        <module entry="save" location="inx">$name.wasm</module>
    </wasm>
</inkscape-extension>
INX

    out=$work/$name.$suffix
    rm -f "$out"
    INKSCAPE_EXTENSIONS_DIR="$work" "$inkscape" \
        --export-filename="$out" "$here/fixture.svg" >"$work/$name.log" 2>&1 || true

    if [ ! -f "$out" ]; then
        printf 'NO OUTPUT     %s\n' "$name"
        grep -i 'wasm\|save' "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1)); return
    fi
    if ! grep -q "$pattern" "$out"; then
        printf 'FAIL          %s (expected %s in its output)\n' "$name" "$pattern"
        grep -i 'wasm\|save' "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1)); return
    fi
    printf 'ok            %s\n' "$name"
    pass=$((pass + 1))
}

run_output_case cover-output org.inkscape.output.wasm.cover.output wasmout 'wasm-output-ok'

# run_input_case <name> <id> <suffix> <content> <pattern>
#
# Writes <content> to a file with that suffix, opens it through the module, exports the result
# as SVG and greps that. The module is handed the file's bytes and an empty document, so what
# comes out the far side is what it built.
run_input_case() {
    name=$1 id=$2 suffix=$3 content=$4 pattern=$5
    "$water" "$here/$name.wat" -o "$work/$name.wasm" >/dev/null 2>&1 || {
        printf 'ASSEMBLE FAIL %s\n' "$name"; fail=$((fail + 1)); return
    }
    cat > "$work/$name.inx" <<INX
<?xml version="1.0" encoding="UTF-8"?>
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<inkscape-extension xmlns="http://www.inkscape.org/namespace/inkscape/extension">
    <name>$name</name>
    <id>$id</id>
    <input>
        <extension>.$suffix</extension>
        <mimetype>application/x-wasm-suite-$suffix</mimetype>
        <filetypename>$name</filetypename>
        <filetypetooltip>$name</filetypetooltip>
    </input>
    <wasm>
        <module entry="open" location="inx">$name.wasm</module>
    </wasm>
</inkscape-extension>
INX

    printf '%s' "$content" > "$work/$name-source.$suffix"
    out=$work/$name.svg
    rm -f "$out"
    INKSCAPE_EXTENSIONS_DIR="$work" "$inkscape" \
        --export-filename="$out" "$work/$name-source.$suffix" >"$work/$name.log" 2>&1 || true

    if [ ! -f "$out" ]; then
        printf 'NO OUTPUT     %s\n' "$name"
        grep -i 'wasm\|open' "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1)); return
    fi
    if ! grep -q "$pattern" "$out"; then
        printf 'FAIL          %s (expected %s in the opened document)\n' "$name" "$pattern"
        grep -i 'wasm\|open' "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1)); return
    fi
    printf 'ok            %s\n' "$name"
    pass=$((pass + 1))
}

run_input_case cover-input org.inkscape.input.wasm.cover.input wasmin 'wasm-input-source' 'ok-input'

# ── ported extensions ───────────────────────────────────────────────────────────────────
#
# A shipped Python extension rewritten against this API and run for real. Testing operations one
# at a time says each one exists; a port says they compose into something someone would actually
# ship, which is where the friction in an interface shows up.
#
# The ports are chosen for where the Python system is structurally stuck rather than for being
# easy: frame.py spawns an Inkscape process per text element to get a bounding box, because
# inkex cannot measure text at all.
run_demo_case() {
    name=$1 id=$2 pattern=$3 subject=${4:-multiline}
    "$water" "$here/$name.wat" -o "$work/$name.wasm" >/dev/null 2>&1 || {
        printf 'ASSEMBLE FAIL %s\n' "$name"; fail=$((fail + 1)); return
    }
    cat > "$work/$name.inx" <<INX
<?xml version="1.0" encoding="UTF-8"?>
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<inkscape-extension xmlns="http://www.inkscape.org/namespace/inkscape/extension">
    <name>$name</name>
    <id>$id</id>
    <param name="segments" type="int" min="2" max="1000" gui-hidden="true" gui-text="n">8</param>
    <param name="max" type="float" min="0.1" max="1000" gui-hidden="true" gui-text="m">10</param>
    <param name="method" type="optiongroup" appearance="combo" gui-hidden="true" gui-text="d">
        <option value="bynum">By number</option>
        <option value="bymax">By max length</option>
    </param>
    <param name="corner_radius" type="int" min="0" max="1000" gui-hidden="true" gui-text="r">4</param>
    <param name="offset_absolute" type="float" min="-1000" max="1000" gui-hidden="true" gui-text="a">5</param>
    <param name="offset_relative" type="float" min="-50" max="1000" gui-hidden="true" gui-text="r">0</param>
    <param name="width" type="float" min="0" max="100" gui-hidden="true" gui-text="w">2</param>
    <param name="stroke_color" type="color" gui-hidden="true" gui-text="s">#3366ccff</param>
    <param name="fill_color" type="color" gui-hidden="true" gui-text="f">#00000000</param>
    <param name="z_position" type="optiongroup" appearance="combo" gui-hidden="true" gui-text="z">
        <option value="bottom">Bottom</option>
        <option value="top">Top</option>
    </param>
    <effect>
        <object-type>all</object-type>
        <effects-menu hidden="true"/>
    </effect>
    <wasm>
        <module entry="effect" location="inx">$name.wasm</module>
    </wasm>
</inkscape-extension>
INX

    out=$work/$name.svg
    rm -f "$out"
    # Something has to be selected for a frame to go round, and the command line has no
    # selection of its own -- so the run selects first, exactly as a user would.
    INKSCAPE_EXTENSIONS_DIR="$work" "$inkscape" \
        --actions="select-by-id:$subject; $id; export-filename:$out; export-do" \
        "$here/fixture.svg" >"$work/$name.log" 2>&1 || true

    if [ ! -f "$out" ]; then
        printf 'NO OUTPUT     %s\n' "$name"
        grep -i wasm "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1)); return
    fi
    if ! grep -q "$pattern" "$out"; then
        printf 'FAIL          %s (expected %s)\n' "$name" "$pattern"
        grep -i wasm "$work/$name.log" | sed 's/^/                /' | head -3 || true
        fail=$((fail + 1)); return
    fi
    printf 'ok            %s\n' "$name"
    pass=$((pass + 1))
}

# Framed around the fixture's TEXT, which is the case Python has to spawn a subprocess for.
run_demo_case demo-frame org.inkscape.effect.wasm.demo.frame 'id="frame"'

# Subdividing a RECT, which addnodes.py filters out of its own selection and never touches.
run_demo_case demo-addnodes org.inkscape.effect.wasm.demo.addnodes 'id="subdivided"' box

# ── the refusal matrix ──────────────────────────────────────────────────────────────────
#
# Every handle-taking operation, against every way a handle can be wrong. One module, driven
# by its `case` parameter, invoked once per combination -- a trap ends the run, so these
# cannot share one. Testing a single representative per class would leave the other forty
# operations resting on the assumption that they were written the same way, which is exactly
# the assumption a test suite exists to stop making.
#
#   0xx null handle    1xx handle never issued    2xx real handle of the wrong kind
#
# The expected diagnostic is only "not a ... handle": which operation refused is already
# established by the case number, and pinning the exact wording of fifty messages would make
# the suite fail on a reworded string rather than on a behaviour change.
OPERATIONS=56

echo
echo "── refusal matrix: $((OPERATIONS * 3)) combinations ───────────────────────"
matrix_pass=0
matrix_fail=0
for bucket in 0 100 200; do
    op=0
    while [ "$op" -lt "$OPERATIONS" ]; do
        case_number=$((bucket + op))
        export case_number
        # Tested inside `if` rather than through $? because `set -e` makes a bare failing
        # call fatal -- which silently truncated this loop after one iteration and left the
        # summary unprinted, the sort of thing that reads as "the suite passed".
        if run_case_quiet trap-matrix "org.inkscape.effect.wasm.trapmatrix" 'not a' fixture.svg; then
            matrix_pass=$((matrix_pass + 1))
        else
            printf 'FAIL          trap-matrix case %d\n' "$case_number"
            matrix_fail=$((matrix_fail + 1))
        fi
        op=$((op + 1))
    done
done
printf '%d refused correctly, %d accepted a bad handle\n' "$matrix_pass" "$matrix_fail"
pass=$((pass + matrix_pass))
fail=$((fail + matrix_fail))

echo "──────────────────────────────────────────────────────────────"
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
