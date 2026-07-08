"""Inkscape MCP server.

Exposes Inkscape actions as MCP tools so any MCP-compatible AI agent
(opencode, Claude, etc.) can drive Inkscape: open files, create shapes,
transform objects, query geometry, export to PNG/PDF, run extensions,
and more.

Integration path: spawns ``inkscape --shell`` as a persistent subprocess
and pipes action strings in.  Works headless (default) or with a visible
GUI (``--with-gui``).
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import shlex
import shutil
import tempfile
import uuid
from pathlib import Path
from typing import Any, Optional

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import (
    GetPromptResult,
    ImageContent,
    TextContent,
    Tool,
    EmbeddedResource,
)

from .controller import InkscapeShell, ShellResult, quote_arg

logger = logging.getLogger("inkmcp.server")


# ---------------------------------------------------------------------- #
# helpers
# ---------------------------------------------------------------------- #
def _result_text(r: ShellResult) -> str:
    parts = []
    if r.stdout:
        parts.append(r.stdout)
    if r.stderr:
        parts.append(f"[stderr]\n{r.stderr}")
    if not parts:
        parts.append("ok")
    return "\n".join(parts)


def _temp_svg_path(prefix: str = "inkmcp_") -> str:
    """Create a temp file path that we clean up later."""
    fd, path = tempfile.mkstemp(prefix=prefix, suffix=".svg")
    os.close(fd)
    return path


def _ok(text: str, data: Any = None) -> list[TextContent]:
    payload = {"ok": True, "result": text}
    if data is not None:
        payload["data"] = data
    return [TextContent(type="text", text=json.dumps(payload, indent=2, default=str))]


def _err(msg: str, detail: Any = None) -> list[TextContent]:
    payload = {"ok": False, "error": msg}
    if detail is not None:
        payload["detail"] = detail
    return [TextContent(type="text", text=json.dumps(payload, indent=2, default=str))]


# ---------------------------------------------------------------------- #
# server
# ---------------------------------------------------------------------- #
class InkscapeMCPServer:
    def __init__(
        self,
        inkscape_bin: str = "",
        with_gui: bool = False,
    ) -> None:
        self.shell = InkscapeShell(
            inkscape_bin=inkscape_bin or os.environ.get("INKSCAPE_BIN", ""),
            with_gui=with_gui,
        )
        self.server: Server = Server("inkscape-mcp")
        self._register_handlers()

    # ------------------------------------------------------------------ #
    # tool table
    # ------------------------------------------------------------------ #
    TOOLS: list[dict] = [
        {
            "name": "inkscape_status",
            "description": "Check whether the Inkscape shell subprocess is alive and report its binary path.",
            "schema": {"type": "object", "properties": {}},
        },
        {
            "name": "inkscape_action_list",
            "description": "List every available Inkscape action (command) with tooltips. Returns the full action vocabulary the AI can use.",
            "schema": {"type": "object", "properties": {}},
        },
        {
            "name": "inkscape_run_actions",
            "description": "Run one or more raw Inkscape actions (semicolon-separated action:arg tokens). Power-user escape hatch for any action not covered by a dedicated tool. Example: 'file-open:circle.svg; select-all; export-filename:out.png; export-type:png; export-do'.",
            "schema": {
                "type": "object",
                "properties": {
                    "actions": {
                        "type": "string",
                        "description": "Semicolon-separated action string, e.g. 'select-all; transform-rotate:45'",
                    },
                    "timeout": {
                        "type": "number",
                        "description": "Seconds to wait (default 60).",
                        "default": 60,
                    },
                },
                "required": ["actions"],
            },
        },
        {
            "name": "inkscape_file_open",
            "description": "Open an SVG (or importable) file in the Inkscape shell.",
            "schema": {
                "type": "object",
                "properties": {
                    "path": {"type": "string", "description": "Absolute or relative path to the SVG/PDF/PNG file."},
                },
                "required": ["path"],
            },
        },
        {
            "name": "inkscape_file_new",
            "description": "Create a new blank SVG document.",
            "schema": {"type": "object", "properties": {}},
        },
        {
            "name": "inkscape_file_save",
            "description": "Save the current document to a path (SVG by default).",
            "schema": {
                "type": "object",
                "properties": {
                    "path": {"type": "string", "description": "Destination path."},
                },
                "required": ["path"],
            },
        },
        {
            "name": "inkscape_export",
            "description": "Export the current document (or a selected object) to PNG/PDF/PS/EPS/SVG. Configures all export options then fires export-do.",
            "schema": {
                "type": "object",
                "properties": {
                    "filename": {"type": "string", "description": "Output file path."},
                    "type": {"type": "string", "description": "export type: png, pdf, ps, eps, svg", "default": "png"},
                    "area": {"type": "string", "description": "'page', 'drawing', or 'x0,y0,x1,y1'"},
                    "dpi": {"type": "number", "description": "Raster DPI (png).", "default": 96},
                    "width": {"type": "number", "description": "Output width px (png)."},
                    "height": {"type": "number", "description": "Output height px (png)."},
                    "id": {"type": "string", "description": "Export only this object id."},
                    "id_only": {"type": "boolean", "description": "Export only the selected id area.", "default": False},
                    "text_to_path": {"type": "boolean", "description": "Convert text to paths (pdf/ps).", "default": False},
                    "background": {"type": "string", "description": "Background color, e.g. '#ffffff'."},
                    "background_opacity": {"type": "number", "description": "0.0–1.0"},
                },
                "required": ["filename"],
            },
        },
        {
            "name": "inkscape_select",
            "description": "Select objects by id, or all / none / invert.",
            "schema": {
                "type": "object",
                "properties": {
                    "mode": {"type": "string", "description": "'all', 'none', 'invert', 'by-id'", "default": "all"},
                    "ids": {"type": "string", "description": "Comma-separated object ids (mode='by-id')."},
                },
            },
        },
        {
            "name": "inkscape_query",
            "description": "Query geometry / properties of objects. Without an id, queries all objects (select-list style).",
            "schema": {
                "type": "object",
                "properties": {
                    "what": {"type": "string", "description": "'all', 'x', 'y', 'width', 'height', 'id'", "default": "all"},
                    "id": {"type": "string", "description": "Object id to query (optional)."},
                },
            },
        },
        {
            "name": "inkscape_transform",
            "description": "Apply a transform to the current selection: translate, rotate, scale, flip, or rotate-90.",
            "schema": {
                "type": "object",
                "properties": {
                    "op": {"type": "string", "description": "'translate', 'rotate', 'scale', 'flip-h', 'flip-v', 'rotate-90-cw', 'rotate-90-ccw'"},
                    "dx": {"type": "number", "description": "translate dx (px)."},
                    "dy": {"type": "number", "description": "translate dy (px)."},
                    "angle": {"type": "number", "description": "rotate angle (deg)."},
                    "factor": {"type": "number", "description": "scale factor."},
                },
                "required": ["op"],
            },
        },
        {
            "name": "inkscape_object_ops",
            "description": "Object-level operations: duplicate, delete, group, ungroup, raise/lower z-order, set attribute, to-path, trace bitmap, etc.",
            "schema": {
                "type": "object",
                "properties": {
                    "op": {"type": "string", "description": "One of: duplicate, delete, group, ungroup, raise, lower, raise-to-top, lower-to-bottom, to-path, stroke-to-path, rotate-90-cw, rotate-90-ccw, flip-h, flip-v, set-attribute, get-attribute, trace, set-clip, unset-clip, set-mask, unset-mask"},
                    "attribute": {"type": "string", "description": "attribute name (set/get-attribute)."},
                    "value": {"type": "string", "description": "attribute value (set-attribute)."},
                },
                "required": ["op"],
            },
        },
        {
            "name": "inkscape_create_shape",
            "description": "Create a basic shape by injecting SVG XML into the document via a temporary extension-free approach. Supports rect, ellipse, circle, line, path, text, use (clone).",
            "schema": {
                "type": "object",
                "properties": {
                    "shape": {"type": "string", "description": "rect, ellipse, circle, line, path, text, use"},
                    "attrs": {"type": "object", "description": "SVG attributes dict, e.g. {x:10,y:10,width:100,height:50,fill:'#ff0000'}"},
                    "text": {"type": "string", "description": "Text content (shape='text')."},
                },
                "required": ["shape"],
            },
        },
        {
            "name": "inkscape_path_ops",
            "description": "Boolean path operations on the selection: union, difference, intersection, exclusion, division, cut, combine, break apart, reverse.",
            "schema": {
                "type": "object",
                "properties": {
                    "op": {"type": "string", "description": "union, difference, intersection, exclusion, division, cut, combine, break-apart, reverse"},
                },
                "required": ["op"],
            },
        },
        {
            "name": "inkscape_align",
            "description": "Align selected objects relative to each other or the page.",
            "schema": {
                "type": "object",
                "properties": {
                    "horizontal": {"type": "string", "description": "left, center, right, or omit"},
                    "vertical": {"type": "string", "description": "top, middle, bottom, or omit"},
                    "relative_to": {"type": "string", "description": "'selection' or 'page'", "default": "selection"},
                },
            },
        },
        {
            "name": "inkscape_list_objects",
            "description": "List every object id + tag in the current document (uses query-all). Useful for the AI to discover what's in the file.",
            "schema": {"type": "object", "properties": {}},
        },
        {
            "name": "inkscape_get_svg",
            "description": "Save the current document to a temp file and read it back, returning the raw SVG XML. Lets the AI inspect the full DOM.",
            "schema": {"type": "object", "properties": {}},
        },
        {
            "name": "inkscape_set_svg",
            "description": "Replace the current document content by writing SVG XML to a temp file and opening it. Lets the AI author arbitrary SVG.",
            "schema": {
                "type": "object",
                "properties": {
                    "svg": {"type": "string", "description": "Raw SVG XML content."},
                },
                "required": ["svg"],
            },
        },
        {
            "name": "inkscape_undo",
            "description": "Undo or redo the last operation.",
            "schema": {
                "type": "object",
                "properties": {
                    "redo": {"type": "boolean", "description": "If true, redo instead of undo.", "default": False},
                },
            },
        },
        {
            "name": "inkscape_vacuum_defs",
            "description": "Remove unused definitions from the SVG <defs> section (cleanup).",
            "schema": {"type": "object", "properties": {}},
        },
        {
            "name": "inkscape_fit_canvas",
            "description": "Fit the canvas/page to the drawing bounding box or set a custom page size.",
            "schema": {
                "type": "object",
                "properties": {
                    "mode": {"type": "string", "description": "'fit-drawing' or 'set-size'", "default": "fit-drawing"},
                    "width": {"type": "number", "description": "page width (mode='set-size')."},
                    "height": {"type": "number", "description": "page height (mode='set-size')."},
                },
            },
        },
    ]

    # ------------------------------------------------------------------ #
    # handler registration
    # ------------------------------------------------------------------ #
    def _register_handlers(self) -> None:
        srv = self.server

        @srv.list_tools()
        async def list_tools() -> list[Tool]:
            return [
                Tool(
                    name=t["name"],
                    description=t["description"],
                    inputSchema=t["schema"],
                )
                for t in self.TOOLS
            ]

        @srv.call_tool()
        async def call_tool(name: str, arguments: dict) -> list[TextContent]:
            try:
                handler = getattr(self, f"_tool_{name}")
            except AttributeError:
                return _err(f"unknown tool: {name}")
            try:
                return await handler(arguments or {})
            except Exception as exc:  # surface to the agent
                return _err(str(exc), repr(exc))

        @srv.get_prompt()
        async def get_prompt(name: str, args: dict) -> GetPromptResult:
            if name == "inkscape_help":
                return GetPromptResult(
                    description="Inkscape MCP help",
                    messages=[
                        {
                            "role": "user",
                            "content": {
                                "type": "text",
                                "text": (
                                    "You are controlling Inkscape via an MCP server. "
                                    "Available tools let you open/save/export SVG files, "
                                    "select & transform objects, create shapes, run path "
                                    "booleans, align, query geometry, and run raw actions. "
                                    "Call inkscape_action_list to discover every action."
                                ),
                            },
                        }
                    ],
                )
            return GetPromptResult(description="unknown", messages=[])

    # ------------------------------------------------------------------ #
    # tool implementations
    # ------------------------------------------------------------------ #
    async def _tool_inkscape_status(self, a: dict) -> list[TextContent]:
        info = {
            "binary": self.shell.inkscape_bin,
            "running": self.shell.is_running,
            "with_gui": self.shell.with_gui,
        }
        return _ok(json.dumps(info), info)

    async def _tool_inkscape_action_list(self, a: dict) -> list[TextContent]:
        r = await self.shell.run_actions(["action-list"], timeout=30)
        return _ok(_result_text(r))

    async def _tool_inkscape_run_actions(self, a: dict) -> list[TextContent]:
        timeout = float(a.get("timeout", 60))
        r = await self.shell.run_action_string(a["actions"], timeout=timeout)
        return _ok(_result_text(r))

    async def _tool_inkscape_file_open(self, a: dict) -> list[TextContent]:
        path = str(Path(a["path"]).expanduser().resolve())
        r = await self.shell.run_actions([f"file-open:{quote_arg(path)}"])
        return _ok(_result_text(r))

    async def _tool_inkscape_file_new(self, a: dict) -> list[TextContent]:
        r = await self.shell.run_actions(["file-new"])
        return _ok(_result_text(r))

    async def _tool_inkscape_file_save(self, a: dict) -> list[TextContent]:
        path = str(Path(a["path"]).expanduser().resolve())
        # save-as via export-type svg + export-filename + export-do
        actions = [
            f"export-filename:{quote_arg(path)}",
            "export-type:svg",
            "export-do",
        ]
        r = await self.shell.run_actions(actions)
        return _ok(_result_text(r))

    async def _tool_inkscape_export(self, a: dict) -> list[TextContent]:
        filename = str(Path(a["filename"]).expanduser().resolve())
        etype = a.get("type", "png")
        actions: list[str] = [f"export-filename:{quote_arg(filename)}", f"export-type:{etype}"]
        area = a.get("area")
        if area == "page":
            actions.append("export-area-page")
        elif area == "drawing":
            actions.append("export-area-drawing")
        elif area:
            actions.append(f"export-area:{area}")
        if a.get("dpi"):
            actions.append(f"export-dpi:{a['dpi']}")
        if a.get("width"):
            actions.append(f"export-width:{a['width']}")
        if a.get("height"):
            actions.append(f"export-height:{a['height']}")
        if a.get("id"):
            actions.append(f"export-id:{quote_arg(a['id'])}")
            if a.get("id_only"):
                actions.append("export-id-only")
        if a.get("text_to_path"):
            actions.append("export-text-to-path")
        if a.get("background"):
            actions.append(f"export-background:{a['background']}")
        if a.get("background_opacity") is not None:
            actions.append(f"export-background-opacity:{a['background_opacity']}")
        actions.append("export-do")
        r = await self.shell.run_actions(actions, timeout=120)
        return _ok(_result_text(r))

    async def _tool_inkscape_select(self, a: dict) -> list[TextContent]:
        mode = a.get("mode", "all")
        if mode == "all":
            r = await self.shell.run_actions(["select-all"])
        elif mode == "none":
            r = await self.shell.run_actions(["select-clear"])
        elif mode == "invert":
            r = await self.shell.run_actions(["select-invert"])
        elif mode == "by-id":
            ids = a.get("ids", "")
            r = await self.shell.run_actions([f"select-by-id:{quote_arg(ids)}"])
        else:
            return _err(f"unknown select mode: {mode}")
        return _ok(_result_text(r))

    async def _tool_inkscape_query(self, a: dict) -> list[TextContent]:
        what = a.get("what", "all")
        actions: list[str] = []
        mapping = {
            "all": "query-all",
            "x": "query-x",
            "y": "query-y",
            "width": "query-width",
            "height": "query-height",
            "id": "query-id",
        }
        action = mapping.get(what, "query-all")
        if a.get("id"):
            actions.append(f"query-id:{quote_arg(a['id'])}")
        actions.append(action)
        r = await self.shell.run_actions(actions)
        return _ok(_result_text(r))

    async def _tool_inkscape_transform(self, a: dict) -> list[TextContent]:
        op = a["op"]
        if op == "translate":
            actions = [f"transform-translate:{a.get('dx', 0)},{a.get('dy', 0)}"]
        elif op == "rotate":
            actions = [f"transform-rotate:{a.get('angle', 0)}"]
        elif op == "scale":
            actions = [f"transform-scale:{a.get('factor', 1)}"]
        elif op == "flip-h":
            actions = ["object-flip-horizontal"]
        elif op == "flip-v":
            actions = ["object-flip-vertical"]
        elif op == "rotate-90-cw":
            actions = ["object-rotate-90-cw"]
        elif op == "rotate-90-ccw":
            actions = ["object-rotate-90-ccw"]
        else:
            return _err(f"unknown transform op: {op}")
        r = await self.shell.run_actions(actions)
        return _ok(_result_text(r))

    async def _tool_inkscape_object_ops(self, a: dict) -> list[TextContent]:
        op = a["op"]
        mapping = {
            "duplicate": "duplicate",
            "delete": "delete",
            "group": "group",
            "ungroup": "ungroup",
            "raise": "raise",
            "lower": "lower",
            "raise-to-top": "raise-to-top",
            "lower-to-bottom": "lower-to-bottom",
            "to-path": "object-to-path",
            "stroke-to-path": "object-stroke-to-path",
            "rotate-90-cw": "object-rotate-90-cw",
            "rotate-90-ccw": "object-rotate-90-ccw",
            "flip-h": "object-flip-horizontal",
            "flip-v": "object-flip-vertical",
            "trace": "object-trace",
            "set-clip": "object-set-clip",
            "unset-clip": "object-unset-clip",
            "set-mask": "object-set-mask",
            "unset-mask": "object-unset-mask",
        }
        if op == "set-attribute":
            attr = a.get("attribute", "")
            val = a.get("value", "")
            r = await self.shell.run_actions(
                [f"object-set-attribute:{quote_arg(attr)},{quote_arg(val)}"]
            )
        elif op == "get-attribute":
            attr = a.get("attribute", "")
            r = await self.shell.run_actions([f"object-get-attribute:{quote_arg(attr)}"])
        elif op in mapping:
            r = await self.shell.run_actions([mapping[op]])
        else:
            return _err(f"unknown object op: {op}")
        return _ok(_result_text(r))

    async def _tool_inkscape_create_shape(self, a: dict) -> list[TextContent]:
        shape = a["shape"]
        attrs = a.get("attrs", {})
        text = a.get("text", "")

        # Generate a unique id for the new element if none provided
        if "id" not in attrs:
            attrs = {**attrs, "id": f"inkmcp_{shape}_{uuid.uuid4().hex[:8]}"}

        # Build SVG element string
        attr_str = " ".join(f'{k}="{v}"' for k, v in attrs.items())
        if shape == "text" and text:
            svg_el = f"<text {attr_str}>{text}</text>"
        elif shape == "path":
            d = attrs.get("d", "")
            extra = " ".join(f'{k}="{v}"' for k, v in attrs.items() if k != "d")
            svg_el = f'<path d="{d}" {extra}/>'
        elif shape == "use":
            svg_el = f"<use {attr_str}/>"
        else:
            svg_el = f"<{shape} {attr_str}/>"
        logger.info("create_shape: %s id=%s", shape, attrs.get("id"))

        # Strategy: save current doc → inject element into SVG XML → reopen.
        # This MERGES into the existing document rather than replacing.
        # If no document is open, start with a fresh blank one.
        new_id = attrs.get("id", "")

        # Step 1: get current SVG (if any)
        tmp_get = _temp_svg_path("inkmcp_get_")
        try:
            await self.shell.run_actions(
                [f"export-filename:{quote_arg(tmp_get)}", "export-type:svg", "export-do"]
            )
            current_svg = Path(tmp_get).read_text(encoding="utf-8") if Path(tmp_get).exists() else ""
        finally:
            Path(tmp_get).unlink(missing_ok=True)

        # Step 2: inject element before </svg>
        if current_svg and "</svg>" in current_svg:
            injected = current_svg.replace("</svg>", f"  {svg_el}\n</svg>")
        else:
            # no document open — create a fresh one
            injected = (
                '<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg xmlns="http://www.w3.org/2000/svg" '
                'width="800" height="600" viewBox="0 0 800 600">\n'
                f"  {svg_el}\n</svg>"
            )

        # Step 3: write back and open
        tmp_put = _temp_svg_path("inkmcp_put_")
        try:
            Path(tmp_put).write_text(injected, encoding="utf-8")
            r = await self.shell.run_actions([f"file-open:{quote_arg(tmp_put)}"])
            result_msg = f"Created {shape} (id={new_id}) and merged into document."
        finally:
            Path(tmp_put).unlink(missing_ok=True)

        return _ok(result_msg + "\n" + _result_text(r), {"id": new_id, "shape": shape})

    async def _tool_inkscape_path_ops(self, a: dict) -> list[TextContent]:
        op = a["op"]
        mapping = {
            "union": "path-union",
            "difference": "path-difference",
            "intersection": "path-intersection",
            "exclusion": "path-exclusion",
            "division": "path-division",
            "cut": "path-cut",
            "combine": "path-combine",
            "break-apart": "path-break-apart",
            "reverse": "path-reverse",
        }
        if op not in mapping:
            return _err(f"unknown path op: {op}")
        r = await self.shell.run_actions([mapping[op]])
        return _ok(_result_text(r))

    async def _tool_inkscape_align(self, a: dict) -> list[TextContent]:
        h = a.get("horizontal")
        v = a.get("vertical")
        rel = a.get("relative_to", "selection")
        actions: list[str] = []
        h_map = {"left": "align-left", "center": "align-horizontal-center",
                 "right": "align-right"}
        v_map = {"top": "align-top", "middle": "align-vertical-center",
                 "bottom": "align-bottom"}
        if h and h in h_map:
            actions.append(h_map[h])
        if v and v in v_map:
            actions.append(v_map[v])
        if not actions:
            return _err("no alignment specified")
        if rel == "page":
            actions.insert(0, "align-to-page")
        r = await self.shell.run_actions(actions)
        return _ok(_result_text(r))

    async def _tool_inkscape_list_objects(self, a: dict) -> list[TextContent]:
        r = await self.shell.run_actions(["query-all"])
        return _ok(_result_text(r))

    async def _tool_inkscape_get_svg(self, a: dict) -> list[TextContent]:
        tmp = _temp_svg_path("inkmcp_read_")
        try:
            r = await self.shell.run_actions(
                [f"export-filename:{quote_arg(tmp)}", "export-type:svg", "export-do"]
            )
            if not r.ok:
                return _err("failed to read SVG", _result_text(r))
            svg = Path(tmp).read_text(encoding="utf-8")
        finally:
            Path(tmp).unlink(missing_ok=True)
        return _ok(svg[:8000])

    async def _tool_inkscape_set_svg(self, a: dict) -> list[TextContent]:
        svg = a["svg"]
        tmp = _temp_svg_path("inkmcp_set_")
        try:
            Path(tmp).write_text(svg, encoding="utf-8")
            r = await self.shell.run_actions([f"file-open:{quote_arg(tmp)}"])
        finally:
            Path(tmp).unlink(missing_ok=True)
        return _ok(_result_text(r))

    async def _tool_inkscape_undo(self, a: dict) -> list[TextContent]:
        if a.get("redo"):
            r = await self.shell.run_actions(["edit-redo"])
        else:
            r = await self.shell.run_actions(["edit-undo"])
        return _ok(_result_text(r))

    async def _tool_inkscape_vacuum_defs(self, a: dict) -> list[TextContent]:
        r = await self.shell.run_actions(["vacuum-defs"])
        return _ok(_result_text(r))

    async def _tool_inkscape_fit_canvas(self, a: dict) -> list[TextContent]:
        mode = a.get("mode", "fit-drawing")
        if mode == "fit-drawing":
            r = await self.shell.run_actions(["fit-canvas-to-drawing"])
        elif mode == "set-size":
            w = a.get("width", 800)
            h = a.get("height", 600)
            # set page size by editing the SVG root width/height attributes
            # via object-set-attribute on the root element
            actions = [
                "select-all",
                f"object-set-attribute:width,{w}",
                f"object-set-attribute:height,{h}",
                "object-set-attribute:viewBox,0 0 {} {}".format(w, h),
            ]
            r = await self.shell.run_actions(actions)
        else:
            return _err(f"unknown fit mode: {mode}")
        return _ok(_result_text(r))

    # ------------------------------------------------------------------ #
    # entry point
    # ------------------------------------------------------------------ #
    async def run(self) -> None:
        async with stdio_server() as (read, write):
            await self.server.run(read, write, self.server.create_initialization_options())


# ---------------------------------------------------------------------- #
# CLI
# ---------------------------------------------------------------------- #
def main() -> None:
    import argparse

    p = argparse.ArgumentParser(description="Inkscape MCP server")
    p.add_argument("--inkscape-bin", default="", help="path to inkscape binary")
    p.add_argument("--with-gui", action="store_true", help="launch Inkscape with GUI")
    p.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="logging level (default INFO)",
    )
    p.add_argument(
        "--log-file",
        default="",
        help="log to file instead of stderr (useful when stdio is the MCP transport)",
    )
    args = p.parse_args()

    # Configure logging — must go to stderr/file, NOT stdout (stdout is MCP transport)
    log_kwargs: dict = {"level": args.log_level, "format": "%(asctime)s %(name)s %(levelname)s %(message)s"}
    if args.log_file:
        log_kwargs["filename"] = args.log_file
    logging.basicConfig(**log_kwargs)
    logger.info("starting inkscape-mcp server (bin=%s gui=%s)", args.inkscape_bin or "auto", args.with_gui)

    server = InkscapeMCPServer(
        inkscape_bin=args.inkscape_bin,
        with_gui=args.with_gui,
    )
    asyncio.run(server.run())


if __name__ == "__main__":
    main()