# Inkscape MCP Server

Control **Inkscape** from any MCP-compatible AI agent (opencode, Claude Desktop, etc.).

## How it works

```
AI Agent ──MCP──▶ inkscape-mcp ──stdin/stdout──▶ inkscape --shell
```

The server spawns `inkscape --shell` as a persistent subprocess and drives it
by piping Inkscape **action strings** (`action:arg; action:arg; ...`) to stdin.
This is the simplest, most robust integration — no DBus dependency, works
headless or with a visible GUI.

## Install

```bash
pip install -e .
```

## Configure in opencode

Add to `opencode.json` (or `~/.config/opencode/opencode.json`):

```json
{
  "mcp": {
    "inkscape": {
      "type": "local",
      "command": ["python3", "-m", "inkmcp.server"],
      "environment": {
        "INKSCAPE_BIN": "/usr/bin/inkscape"
      }
    }
  }
}
```

Or with GUI visible:

```json
{
  "mcp": {
    "inkscape": {
      "type": "local",
      "command": ["python3", "-m", "inkmcp.server", "--with-gui"]
    }
  }
}
```

## Tools (21)

| Tool | Description |
|------|-------------|
| `inkscape_status` | Check shell subprocess status |
| `inkscape_action_list` | List all available Inkscape actions |
| `inkscape_run_actions` | Run raw semicolon-separated action string |
| `inkscape_file_open` | Open an SVG/PDF/PNG file |
| `inkscape_file_new` | Create a new blank document |
| `inkscape_file_save` | Save current document to SVG |
| `inkscape_export` | Export to PNG/PDF/PS/EPS/SVG |
| `inkscape_select` | Select all / none / invert / by-id |
| `inkscape_query` | Query geometry (x/y/w/h/all) |
| `inkscape_transform` | Translate / rotate / scale / flip |
| `inkscape_object_ops` | Duplicate, delete, group, to-path, etc. |
| `inkscape_create_shape` | Create rect/ellipse/path/text/use |
| `inkscape_path_ops` | Boolean ops (union, difference, ...) |
| `inkscape_align` | Align objects horizontally/vertically |
| `inkscape_list_objects` | List all object ids |
| `inkscape_get_svg` | Read the current SVG DOM as text |
| `inkscape_set_svg` | Replace document with raw SVG XML |
| `inkscape_undo` | Undo / redo |
| `inkscape_vacuum_defs` | Clean up unused defs |
| `inkscape_fit_canvas` | Fit canvas to drawing / set page size |

## Architecture

See `architecture.md` for the full diagram and `Inkscape MCP.md` (Obsidian)
for the design notes.