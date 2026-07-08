"""Inkscape shell-mode controller.

Spawns ``inkscape --shell`` as a subprocess and drives it by writing
action strings (``action:arg; action:arg; ...``) to stdin and reading
stdout/stderr back.  This is the simplest, most robust integration
path — no DBus dependency, works headless or with ``--with-gui``.
"""

from __future__ import annotations

import asyncio
import logging
import os
import re
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

logger = logging.getLogger("inkmcp.controller")

PROMPT_RE = re.compile(r"> *$")  # the shell prompt is a single ``>``

# Inkscape error markers — case-insensitive substrings in stderr/stdout
_ERROR_MARKERS = (
    "error",
    "unknown action",
    "no such",
    "failed",
    "invalid",
    "cannot",
    "could not",
    "does not exist",
    "not found",
    "warning:",  # warnings often indicate real problems
)

# Pipe buffer limit on Linux is typically 64 KiB — read in bigger chunks
_READ_CHUNK = 65536


@dataclass
class ShellResult:
    ok: bool
    stdout: str = ""
    stderr: str = ""
    actions: list[str] = field(default_factory=list)
    raw_command: str = ""


class InkscapeShell:
    """Asynchronous wrapper around a persistent ``inkscape --shell`` process."""

    def __init__(
        self,
        inkscape_bin: str = "inkscape",
        with_gui: bool = False,
        extra_args: Optional[list[str]] = None,
        cwd: Optional[str] = None,
        env: Optional[dict[str, str]] = None,
    ) -> None:
        self.inkscape_bin = inkscape_bin or self._auto_detect()
        self.with_gui = with_gui
        self.extra_args = list(extra_args or [])
        self.cwd = cwd
        self.env = env
        self._proc: Optional[asyncio.subprocess.Process] = None
        self._lock = asyncio.Lock()
        self._started = False

    # ------------------------------------------------------------------ #
    # lifecycle
    # ------------------------------------------------------------------ #
    async def start(self) -> None:
        if self._proc and self._proc.returncode is None:
            return
        args = [self.inkscape_bin, "--shell"]
        if not self.with_gui:
            args.append("--batch-process")
        args.extend(self.extra_args)

        env = dict(os.environ)
        if self.env:
            env.update(self.env)

        logger.info("spawning inkscape shell: %s", " ".join(args))
        self._proc = await asyncio.create_subprocess_exec(
            *args,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=self.cwd,
            env=env,
            limit=2 ** 20,
        )
        await self._read_until_prompt(timeout=20)
        self._started = True
        logger.info("inkscape shell ready (pid=%s)", self._proc.pid)

    async def stop(self) -> None:
        if not self._proc or self._proc.returncode is not None:
            return
        logger.info("stopping inkscape shell (pid=%s)", self._proc.pid)
        try:
            if self._proc.stdin and not self._proc.stdin.is_closing():
                self._proc.stdin.write_eof()
            await asyncio.wait_for(self._proc.wait(), timeout=5)
        except Exception:
            try:
                self._proc.kill()
            except ProcessLookupError:
                pass
        self._proc = None
        self._started = False

    @property
    def is_running(self) -> bool:
        return self._proc is not None and self._proc.returncode is None

    # ------------------------------------------------------------------ #
    # core command
    # ------------------------------------------------------------------ #
    async def run_actions(self, actions: list[str], timeout: float = 60.0) -> ShellResult:
        """Send a list of action tokens to the shell and collect output.

        Each entry in *actions* may already contain a colon-arg, e.g.
        ``"file-open:my.svg"`` or just ``"select-all"``.
        """
        if not self.is_running:
            await self.start()
        cmd = "; ".join(actions)
        async with self._lock:
            return await self._send(cmd, timeout=timeout)

    async def run_action_string(self, cmd: str, timeout: float = 60.0) -> ShellResult:
        """Send a raw action string (semicolon-separated) to the shell."""
        if not self.is_running:
            await self.start()
        async with self._lock:
            return await self._send(cmd, timeout=timeout)

    # ------------------------------------------------------------------ #
    # internals
    # ------------------------------------------------------------------ #
    async def _send(self, cmd: str, timeout: float) -> ShellResult:
        assert self._proc and self._proc.stdin
        logger.debug("sending: %s", cmd)
        self._proc.stdin.write((cmd + "\n").encode())
        await self._proc.stdin.drain()
        stdout, stderr = await self._read_until_prompt(timeout=timeout)
        ok = self._detect_ok(stdout, stderr)
        result = ShellResult(
            ok=ok,
            stdout=stdout,
            stderr=stderr,
            actions=[a.strip() for a in cmd.split(";")],
            raw_command=cmd,
        )
        logger.debug("ok=%s stdout_len=%d stderr_len=%d", ok, len(stdout), len(stderr))
        return result

    def _detect_ok(self, stdout: str, stderr: str) -> bool:
        """Heuristic: an action succeeded if stderr is empty and no error
        markers appear in stdout/stderr."""
        if not stderr and not stdout:
            return True
        combined = (stderr + "\n" + stdout).lower()
        for marker in _ERROR_MARKERS:
            if marker in combined:
                return False
        return True

    async def _read_until_prompt(self, timeout: float) -> tuple[str, str]:
        """Read stdout until we see the ``>`` prompt or time out.

        Both stdout and stderr are drained **concurrently** to prevent
        pipe-buffer deadlocks (Inkscape writing >64 KiB to stderr while
        we only read stdout).
        """
        assert self._proc
        stdout_buf = bytearray()
        stderr_buf = bytearray()
        done = asyncio.Event()

        async def _drain_stdout() -> None:
            assert self._proc and self._proc.stdout
            while not done.is_set():
                try:
                    chunk = await asyncio.wait_for(
                        self._proc.stdout.read(_READ_CHUNK), timeout=1.0
                    )
                except asyncio.TimeoutError:
                    continue
                if not chunk:
                    break
                stdout_buf.extend(chunk)
                text = stdout_buf.decode(errors="replace")
                last_line = text.rsplit("\n", 1)[-1] if text else ""
                if PROMPT_RE.search(last_line):
                    done.set()
                    return

        async def _drain_stderr() -> None:
            assert self._proc and self._proc.stderr
            while not done.is_set():
                try:
                    chunk = await asyncio.wait_for(
                        self._proc.stderr.read(_READ_CHUNK), timeout=0.5
                    )
                except asyncio.TimeoutError:
                    continue
                if not chunk:
                    break
                stderr_buf.extend(chunk)

        try:
            async with asyncio.timeout(timeout):
                out_task = asyncio.create_task(_drain_stdout())
                err_task = asyncio.create_task(_drain_stderr())
                await done.wait()
                err_task.cancel()
                out_task.cancel()
        except asyncio.TimeoutError:
            logger.warning("timeout after %.1fs waiting for prompt", timeout)

        out = stdout_buf.decode(errors="replace")
        out = PROMPT_RE.sub("", out).rstrip()
        return out, stderr_buf.decode(errors="replace")

    @staticmethod
    def _auto_detect() -> str:
        for candidate in ("inkscape", "inkscape-bin"):
            path = shutil.which(candidate)
            if path:
                logger.info("auto-detected inkscape: %s", path)
                return path
        for p in (
            "/usr/bin/inkscape",
            "/usr/local/bin/inkscape",
            str(Path.home() / ".local/bin/inkscape"),
            "/snap/bin/inkscape",
            "/opt/inkscape/bin/inkscape",
        ):
            if Path(p).exists():
                logger.info("found inkscape at fallback: %s", p)
                return p
        logger.warning("inkscape binary not found, will fail on spawn")
        return "inkscape"


# ---------------------------------------------------------------------- #
# convenience helper
# ---------------------------------------------------------------------- #
def quote_arg(value: Any) -> str:  # type: ignore[name-defined]
    """Quote a single action argument safely for the shell syntax."""
    s = str(value)
    if any(c in s for c in ":; "):
        return "'" + s.replace("'", "\\'") + "'"
    return s