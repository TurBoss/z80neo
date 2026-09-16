#!/usr/bin/env python3
"""TurBoCPMTools — a PySide6 GUI for CP/M disk images.

Drives the standard `cpmtools` binaries (cpmls / cpmcp / cpmrm / cpmchattr /
mkfs.cpm) with a `diskdefs` file, so it opens the same images as the Windows
CpmtoolsGUI but runs natively on Linux/macOS/Windows.

Usage:
    source venv/bin/activate
    python turbocpmtools.py [image.img]
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QColor, QFontDatabase, QKeySequence, QPainter
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDockWidget,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QHeaderView,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

APP_NAME = "TurBoCPMTools"
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DISKDEFS = REPO_ROOT / "software" / "cpm" / "z80neo" / "diskdefs"
IMAGE_FILTER = "CP/M images (*.img *.dsk *.ima *.bin);;All files (*)"
IMPORT_FILTER = (
    "CP/M programs (*.com);;"
    "CP/M sources/text (*.asm *.sub *.z80 *.mac *.bas *.txt *.doc *.lib);;"
    "All files (*)"
)


def parse_diskdefs(path: Path) -> dict[str, dict[str, str]]:
    """Parse a cpmtools `diskdefs` file into {format_name: {key: value}}."""
    formats: dict[str, dict[str, str]] = {}
    current: str | None = None
    for raw in Path(path).read_text(errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if parts[0] == "diskdef" and len(parts) >= 2:
            current = parts[1]
            formats[current] = {}
        elif parts[0] == "end":
            current = None
        elif current is not None and len(parts) >= 2:
            formats[current][parts[0]] = parts[1]
    return formats


DISKDEF_KEYS = ("seclen", "tracks", "sectrk", "blocksize", "maxdir", "skew", "boottrk", "os")


def append_diskdef(path: Path, name: str, geom: dict) -> None:
    """Append a new `diskdef <name> … end` block to a diskdefs file."""
    lines = ["", f"diskdef {name}"]
    lines += [f"  {k} {geom[k]}" for k in DISKDEF_KEYS if k in geom]
    lines.append("end")
    with Path(path).open("a") as handle:
        handle.write("\n".join(lines) + "\n")


def read_directory(image: Path, geom: dict) -> list[dict]:
    """Read the raw CP/M directory entries.

    Surfaces the CP/M user number, allocation blocks and attribute bits that the
    cpmtools command line output does not show.
    """
    seclen = int(geom.get("seclen", 128))
    sectrk = int(geom.get("sectrk", 26))
    tracks = int(geom.get("tracks", 254))
    blksize = int(geom.get("blocksize", 1024))
    maxdir = int(geom.get("maxdir", 64))
    boottrk = int(geom.get("boottrk", 2))
    dsm = (tracks - boottrk) * sectrk * seclen // blksize - 1
    ptr16 = dsm > 255  # 16-bit allocation block pointers when the disk is big
    data = Path(image).read_bytes()
    base = boottrk * sectrk * seclen
    entries: list[dict] = []
    for i in range(maxdir):
        off = base + i * 32
        chunk = data[off : off + 32]
        if len(chunk) < 32:
            break
        user = chunk[0]
        if user == 0xE5 or user > 15:  # deleted / unused
            continue
        name = chunk[1:9].decode("latin1").rstrip()
        ext_bytes = chunk[9:12]
        ext = bytes(b & 0x7F for b in ext_bytes).decode("latin1").rstrip()
        attrs = (
            ("r" if ext_bytes[0] & 0x80 else "")
            + ("s" if ext_bytes[1] & 0x80 else "")
            + ("a" if ext_bytes[2] & 0x80 else "")
        )
        if ptr16:
            blocks = [chunk[16 + 2 * j] | (chunk[17 + 2 * j] << 8) for j in range(8)]
        else:
            blocks = list(chunk[16:32])
        entries.append(
            {
                "user": user,
                "name": name,
                "ext": ext,
                "extent": chunk[12],
                "s2": chunk[14] & 0x3F,
                "rc": chunk[15],
                "attrs": attrs,
                "blocks": [b for b in blocks if b],
                "offset": off,
            }
        )
    return entries


def split_name(name: str) -> tuple[str, str]:
    """Split an 8.3 CP/M name into (base, ext), upper-cased and trimmed."""
    base, _, ext = name.strip().upper().partition(".")
    return base[:8], ext[:3]


def valid_name(name: str) -> bool:
    base, ext = split_name(name)
    if not base:
        return False
    allowed = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'()+-@^_{}~`")
    return all(c in allowed for c in base + ext)


def rename_in_image(
    image: Path, geom: dict, user: int, old_name: str, new_name: str
) -> int:
    """Rename a CP/M file by rewriting its directory entries in place.

    Rewrites every extent of the file (name + extension) while preserving the
    attribute high bits, so data, attributes and allocation are untouched.
    Returns the number of directory entries changed.
    """
    old_base, old_ext = split_name(old_name)
    new_base, new_ext = split_name(new_name)
    new_ext = (new_ext + "   ")[:3]
    data = bytearray(Path(image).read_bytes())
    changed = 0
    for ent in read_directory(image, geom):
        if ent["user"] != user or ent["name"] != old_base or ent["ext"] != old_ext:
            continue
        off = ent["offset"]
        data[off + 1 : off + 9] = new_base.ljust(8).encode("latin1")
        for j in range(3):
            high = data[off + 9 + j] & 0x80  # keep attribute bit
            data[off + 9 + j] = ord(new_ext[j]) | high
        changed += 1
    if changed:
        Path(image).write_bytes(data)
    return changed


def disk_usage(image: Path, geom: dict) -> dict:
    """Total / used / free allocation blocks and directory slots."""
    seclen = int(geom.get("seclen", 128))
    sectrk = int(geom.get("sectrk", 26))
    tracks = int(geom.get("tracks", 254))
    blksize = int(geom.get("blocksize", 1024))
    maxdir = int(geom.get("maxdir", 64))
    boottrk = int(geom.get("boottrk", 2))
    total = (tracks - boottrk) * sectrk * seclen // blksize
    dir_blocks = (maxdir * 32 + blksize - 1) // blksize
    entries = read_directory(image, geom)
    used: set[int] = set()
    for ent in entries:
        used.update(ent["blocks"])
    free = max(total - dir_blocks - len(used), 0)
    return {
        "blksize": blksize,
        "total_blocks": total,
        "dir_blocks": dir_blocks,
        "used_blocks": len(used),
        "free_blocks": free,
        "files": len(entries),
        "dir_used": len(entries),
        "dir_total": maxdir,
    }


def block_map(image: Path, geom: dict) -> tuple[int, dict[int, str], int]:
    """Return (total_blocks, {block: owner}, directory_blocks) for the image."""
    seclen = int(geom.get("seclen", 128))
    sectrk = int(geom.get("sectrk", 26))
    tracks = int(geom.get("tracks", 254))
    blksize = int(geom.get("blocksize", 1024))
    maxdir = int(geom.get("maxdir", 64))
    boottrk = int(geom.get("boottrk", 2))
    total = (tracks - boottrk) * sectrk * seclen // blksize
    dir_blocks = (maxdir * 32 + blksize - 1) // blksize
    owner: dict[int, str] = {b: "<directory>" for b in range(dir_blocks)}
    for ent in read_directory(image, geom):
        name = f"{ent['name']}.{ent['ext']}" if ent["ext"] else ent["name"]
        for b in ent["blocks"]:
            owner.setdefault(b, name)
    return total, owner, dir_blocks


def hexdump(data: bytes, width: int = 16, limit: int = 65536) -> str:
    lines = []
    for off in range(0, min(len(data), limit), width):
        row = data[off : off + width]
        hexs = " ".join(f"{b:02x}" for b in row)
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        lines.append(f"{off:08x}  {hexs:<{width * 3}}  {text}")
    if len(data) > limit:
        lines.append(f"… {len(data) - limit:,} more bytes")
    return "\n".join(lines)


class CpmToolsError(RuntimeError):
    pass


class CpmTools:
    """Thin wrapper around the cpmtools command line tools."""

    def __init__(self, diskdefs: Path):
        self.diskdefs = Path(diskdefs)

    def _run(self, args: list[str]) -> str:
        # cpmtools looks for a file literally named `diskdefs` in the current
        # working directory (or /usr/share/diskdefs), so run from its directory.
        try:
            proc = subprocess.run(
                args,
                cwd=str(self.diskdefs.parent),
                capture_output=True,
                text=True,
            )
        except FileNotFoundError as exc:
            raise CpmToolsError(f"command not found: {exc.filename}") from exc
        if proc.returncode != 0:
            msg = (proc.stderr or proc.stdout or "command failed").strip()
            raise CpmToolsError(msg)
        return proc.stdout

    def listdir(self, image: Path, fmt: str) -> list[dict]:
        out = self._run(["cpmls", "-f", fmt, "-l", str(image)])
        entries: list[dict] = []
        for line in out.splitlines():
            if not line or line.endswith(":"):
                continue
            if line[0] not in "-d":
                continue
            parts = line.split(None, 5)
            if len(parts) < 6:
                continue
            perms, size, _mon, _day, _year, name = parts
            entries.append({"name": name, "size": int(size), "attr": perms})
        return entries

    def extract(self, image: Path, fmt: str, user: int, name: str, dest: Path) -> None:
        self._run(["cpmcp", "-f", fmt, str(image), f"{user}:{name}", str(dest)])

    def add(self, image: Path, fmt: str, user: int, host: Path, name: str) -> None:
        self._run(["cpmcp", "-f", fmt, str(image), str(host), f"{user}:{name}"])

    def delete(self, image: Path, fmt: str, user: int, name: str) -> None:
        self._run(["cpmrm", "-f", fmt, str(image), f"{user}:{name}"])

    def set_attrs(self, image: Path, fmt: str, user: int, name: str, attrs: str) -> None:
        self._run(["cpmchattr", "-f", fmt, str(image), attrs, f"{user}:{name}"])

    def mkfs(self, image: Path, fmt: str, size: int | None = None) -> None:
        """Create a new image; `size` (bytes) pads it to the full geometry.

        mkfs.cpm only writes the reserved tracks + directory and lets added
        files grow the image, but the Z80 CP/M disk layer addresses the whole
        geometry, so pad out to tracks*sectrk*seclen.
        """
        self._run(["mkfs.cpm", "-f", fmt, str(image)])
        if size:
            with Path(image).open("r+b") as handle:
                handle.truncate(size)


class ViewerDialog(QDialog):
    def __init__(self, name: str, data: bytes, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} — {name}")
        self.resize(760, 540)
        view = QPlainTextEdit()
        view.setReadOnly(True)
        view.setFont(QFontDatabase.systemFont(QFontDatabase.FixedFont))
        sample = data[:4096]
        printable = sum(1 for b in sample if 32 <= b < 127 or b in (9, 10, 13))
        is_text = bool(sample) and printable / len(sample) > 0.9
        view.setPlainText(
            data.decode("latin1") if is_text else hexdump(data)
        )
        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)
        buttons.accepted.connect(self.accept)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"{len(data):,} bytes — {'text' if is_text else 'hex'}"))
        layout.addWidget(view, 1)
        layout.addWidget(buttons)


class AttrDialog(QDialog):
    FLAGS = (
        ("r", "Read-only"),
        ("s", "System"),
        ("a", "Archived"),
        ("1", "F1 (user attr)"),
        ("2", "F2 (user attr)"),
        ("3", "F3 (user attr)"),
        ("4", "F4 (user attr)"),
    )

    def __init__(self, current: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} — file attributes")
        form = QFormLayout(self)
        self.boxes: dict[str, QCheckBox] = {}
        for flag, label in self.FLAGS:
            box = QCheckBox(label)
            box.setChecked(flag in current)
            self.boxes[flag] = box
            form.addRow(box)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def attributes(self) -> str:
        return "".join(f for f, _ in self.FLAGS if self.boxes[f].isChecked())


class FormatDialog(QDialog):
    """Define a CP/M disk format (geometry) and append it to the diskdefs."""

    DEFAULTS = {
        "seclen": "128",
        "tracks": "254",
        "sectrk": "26",
        "blocksize": "1024",
        "maxdir": "128",
        "skew": "0",
        "boottrk": "2",
        "os": "2.2",
    }

    def __init__(self, name: str = "", geom: dict | None = None, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} — disk format")
        geom = geom or {}
        form = QFormLayout(self)
        self.name_edit = QLineEdit(name)
        form.addRow("name", self.name_edit)
        self.fields: dict[str, QLineEdit] = {}
        for key in DISKDEF_KEYS:
            edit = QLineEdit(geom.get(key, self.DEFAULTS[key]))
            edit.textChanged.connect(self._update_info)
            self.fields[key] = edit
            form.addRow(key, edit)
        self.info = QLabel()
        form.addRow(self.info)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)
        self._update_info()

    def _int(self, key: str, default: int = 0) -> int:
        try:
            return int(self.fields[key].text().strip())
        except ValueError:
            return default

    def _update_info(self) -> None:
        size = self._int("tracks") * self._int("sectrk") * self._int("seclen")
        blk = self._int("blocksize", 1) or 1
        blocks = (
            (self._int("tracks") - self._int("boottrk"))
            * self._int("sectrk")
            * self._int("seclen")
        )
        self.info.setText(
            f"image {size:,} bytes · {blocks // blk} blocks of {blk} B"
        )

    def format_name(self) -> str:
        return self.name_edit.text().strip()

    def geometry(self) -> dict:
        return {k: self.fields[k].text().strip() for k in DISKDEF_KEYS}


class SpaceMapWidget(QWidget):
    """Paints the CP/M allocation blocks: directory / used / free."""

    DIR_COLOR = QColor("#b58900")
    USED_COLOR = QColor("#2f6fdb")
    FREE_COLOR = QColor("#303030")

    def __init__(self, total: int, owner: dict[int, str], dir_blocks: int, parent=None):
        super().__init__(parent)
        self.total = total
        self.owner = owner
        self.dir_blocks = dir_blocks
        self.cols = 32
        self.cell = 14
        rows = (total + self.cols - 1) // self.cols
        self.setMinimumSize(self.cols * self.cell, rows * self.cell)
        self.setMouseTracking(True)

    def _block_at(self, x: int, y: int) -> int | None:
        col = x // self.cell
        row = y // self.cell
        if not 0 <= col < self.cols:
            return None
        block = row * self.cols + col
        return block if 0 <= block < self.total else None

    def paintEvent(self, event) -> None:  # noqa: N802 (Qt API)
        painter = QPainter(self)
        for block in range(self.total):
            row, col = divmod(block, self.cols)
            if block < self.dir_blocks:
                color = self.DIR_COLOR
            elif block in self.owner:
                color = self.USED_COLOR
            else:
                color = self.FREE_COLOR
            painter.fillRect(
                col * self.cell, row * self.cell, self.cell - 1, self.cell - 1, color
            )

    def mouseMoveEvent(self, event) -> None:  # noqa: N802 (Qt API)
        pos = event.position().toPoint()
        block = self._block_at(pos.x(), pos.y())
        if block is None:
            self.setToolTip("")
            return
        kind = "directory" if block < self.dir_blocks else self.owner.get(block, "free")
        self.setToolTip(f"block {block}: {kind}")


class SpaceMapDialog(QDialog):
    def __init__(self, image: Path, geom: dict, parent=None):
        super().__init__(parent)
        total, owner, dir_blocks = block_map(image, geom)
        used = len(owner)
        free = max(total - used, 0)
        blk = int(geom.get("blocksize", 1024))
        self.setWindowTitle(f"{APP_NAME} — space map")
        self.resize(660, 560)
        layout = QVBoxLayout(self)
        layout.addWidget(
            QLabel(
                f"{image.name}: {total} blocks × {blk} B — "
                f"{used} used, {free} free  (hover a block for its owner)"
            )
        )
        layout.addWidget(SpaceMapWidget(total, owner, dir_blocks), 1)
        legend = QHBoxLayout()
        for text, color in (
            ("directory", SpaceMapWidget.DIR_COLOR),
            ("used", SpaceMapWidget.USED_COLOR),
            ("free", SpaceMapWidget.FREE_COLOR),
        ):
            chip = QLabel(f"  {text}  ")
            chip.setStyleSheet(f"background:{color.name()}; color:white;")
            legend.addWidget(chip)
        legend.addStretch(1)
        layout.addLayout(legend)
        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)
        buttons.accepted.connect(self.accept)
        layout.addWidget(buttons)


class MainWindow(QMainWindow):
    def __init__(self, image: str | None = None, diskdefs: Path = DEFAULT_DISKDEFS):
        super().__init__()
        self.diskdefs = Path(diskdefs)
        self.formats: dict[str, dict[str, str]] = {}
        self.tools: CpmTools | None = None
        self.entries: list[dict] = []
        self.raw_entries: list[dict] = []
        self.setWindowTitle(APP_NAME)
        self.resize(960, 580)
        self.setAcceptDrops(True)
        self._build_ui()
        self._load_diskdefs()
        if image:
            self.image_edit.setText(image)
            self.refresh()

    # -- UI -----------------------------------------------------------------
    def _build_ui(self) -> None:
        file_menu = self.menuBar().addMenu("&File")
        act_open = QAction("&Open image…", self, shortcut=QKeySequence.Open)
        act_open.triggered.connect(self.open_image)
        act_new = QAction("&New image…", self)
        act_new.triggered.connect(self.new_image)
        act_quit = QAction("&Quit", self, shortcut=QKeySequence.Quit)
        act_quit.triggered.connect(self.close)
        file_menu.addActions([act_open, act_new])
        file_menu.addSeparator()
        file_menu.addAction(act_quit)

        tools_menu = self.menuBar().addMenu("&Tools")
        act_defs = QAction("Select &diskdefs…", self)
        act_defs.triggered.connect(self.choose_diskdefs)
        act_format = QAction("&Define format…", self)
        act_format.triggered.connect(self.define_format)
        act_map = QAction("&Space map…", self)
        act_map.triggered.connect(self.show_space_map)
        act_refresh = QAction("&Refresh", self, shortcut=QKeySequence.Refresh)
        act_refresh.triggered.connect(self.refresh)
        tools_menu.addActions([act_defs, act_format, act_map, act_refresh])

        central = QWidget()
        outer = QVBoxLayout(central)

        top = QHBoxLayout()
        top.addWidget(QLabel("Format:"))
        self.format_combo = QComboBox()
        self.format_combo.setMinimumWidth(120)
        self.format_combo.currentTextChanged.connect(self.refresh)
        top.addWidget(self.format_combo)
        top.addWidget(QLabel("User:"))
        self.user_spin = QSpinBox()
        self.user_spin.setRange(0, 15)
        self.user_spin.setToolTip("CP/M user area used for add / extract / delete")
        self.user_spin.valueChanged.connect(self._update_props)
        top.addWidget(self.user_spin)
        top.addWidget(QLabel("Image:"))
        self.image_edit = QLineEdit()
        self.image_edit.returnPressed.connect(self.refresh)
        top.addWidget(self.image_edit, 1)
        btn_browse = QPushButton("Browse…")
        btn_browse.clicked.connect(self.open_image)
        top.addWidget(btn_browse)
        outer.addLayout(top)

        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["Name", "Size", "User", "Attr"])
        self.table.setSelectionBehavior(QTableWidget.SelectRows)
        self.table.setSelectionMode(QTableWidget.ExtendedSelection)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setAlternatingRowColors(True)
        self.table.verticalHeader().setVisible(False)
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        for col in (1, 2, 3):
            self.table.horizontalHeader().setSectionResizeMode(col, QHeaderView.ResizeToContents)
        self.table.doubleClicked.connect(self.view_selected)
        self.table.itemSelectionChanged.connect(self._update_props)
        outer.addWidget(self.table, 1)

        bottom = QHBoxLayout()
        for label, slot in (
            ("View…", self.view_selected),
            ("Extract…", self.extract_selected),
            ("Add…", self.add_file),
            ("Import dir…", self.import_directory),
            ("Rename…", self.rename_selected),
            ("Attrs…", self.set_attributes),
            ("Delete", self.delete_selected),
            ("New image…", self.new_image),
        ):
            btn = QPushButton(label)
            btn.clicked.connect(slot)
            bottom.addWidget(btn)
        bottom.addStretch(1)
        outer.addLayout(bottom)

        self.setCentralWidget(central)

        self.props = QLabel("Select a file to see its directory entry.")
        self.props.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.props.setWordWrap(True)
        self.props.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        dock = QDockWidget("Properties", self)
        dock.setWidget(self.props)
        dock.setMinimumWidth(230)
        self.addDockWidget(Qt.RightDockWidgetArea, dock)

        self.disk_label = QLabel("")
        self.statusBar().addPermanentWidget(self.disk_label)
        self.statusBar().showMessage(f"diskdefs: {self.diskdefs}")

    # -- diskdefs -----------------------------------------------------------
    def _load_diskdefs(self) -> None:
        self.formats = {}
        self.format_combo.clear()
        try:
            self.formats = parse_diskdefs(self.diskdefs)
        except OSError as exc:
            self._warn(f"Could not read diskdefs:\n{self.diskdefs}\n\n{exc}")
            return
        names = list(self.formats)
        self.format_combo.addItems(names)
        if "z80neo" in names:
            self.format_combo.setCurrentText("z80neo")
        self.tools = CpmTools(self.diskdefs)
        self.statusBar().showMessage(
            f"diskdefs: {self.diskdefs}  ({len(names)} formats)"
        )

    def choose_diskdefs(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Select diskdefs", str(self.diskdefs.parent)
        )
        if path:
            self.diskdefs = Path(path)
            self._load_diskdefs()
            self.refresh()

    def define_format(self) -> None:
        """Edit the geometry of a format and append it to the diskdefs file."""
        current = self.format_combo.currentText()
        dlg = FormatDialog(current, self.formats.get(current, {}), self)
        if dlg.exec() != QDialog.Accepted:
            return
        name = dlg.format_name()
        if not name:
            self._warn("Please give the format a name.")
            return
        if name in self.formats:
            self._warn(
                f"Format '{name}' already exists in\n{self.diskdefs}.\n"
                "Pick a new name."
            )
            return
        try:
            append_diskdef(self.diskdefs, name, dlg.geometry())
        except OSError as exc:
            self._warn(f"Could not write diskdefs:\n{exc}")
            return
        self._load_diskdefs()
        self.format_combo.setCurrentText(name)
        self.statusBar().showMessage(f"added format '{name}' to {self.diskdefs}")

    # -- image actions ------------------------------------------------------
    def show_space_map(self) -> None:
        image = self._image()
        geom = self._geom()
        if not image or not image.exists() or not geom:
            self._warn("Open an image first.")
            return
        try:
            SpaceMapDialog(image, geom, self).exec()
        except OSError as exc:
            self._warn(f"Could not read image:\n{exc}")

    def open_image(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Open CP/M image", str(Path.home()), IMAGE_FILTER
        )
        if path:
            self.image_edit.setText(path)
            self.refresh()

    def _image(self) -> Path | None:
        text = self.image_edit.text().strip()
        if not text:
            return None
        return Path(text).expanduser().resolve()

    def refresh(self) -> None:
        self.table.setRowCount(0)
        self.entries = []
        image = self._image()
        fmt = self.format_combo.currentText()
        if not image or not fmt or self.tools is None:
            return
        if not image.exists():
            self.statusBar().showMessage(f"not found: {image}")
            return
        try:
            self.entries = self.tools.listdir(image, fmt)
        except CpmToolsError as exc:
            self._warn(f"Could not read image:\n{exc}")
            return
        try:
            self.raw_entries = read_directory(image, self._geom())
        except OSError:
            self.raw_entries = []
        user_map: dict[tuple[str, str], int] = {}
        for raw in self.raw_entries:
            user_map.setdefault((raw["name"], raw["ext"]), raw["user"])
        self.table.setRowCount(len(self.entries))
        for row, ent in enumerate(self.entries):
            base = ent["name"].upper().split(".")
            key = (base[0][:8], (base[1] if len(base) > 1 else "")[:3])
            user = user_map.get(key, "")
            values = (ent["name"], f'{ent["size"]:,}', user, ent["attr"])
            for col, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                if col == 1:
                    item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
                self.table.setItem(row, col, item)
        total = sum(e["size"] for e in self.entries)
        self.statusBar().showMessage(
            f"{image.name}  [{fmt}]  {len(self.entries)} files, {total:,} bytes"
        )
        self._update_disk_info()
        self._update_props()

    def _geom(self) -> dict:
        return self.formats.get(self.format_combo.currentText(), {})

    def _update_disk_info(self) -> None:
        image = self._image()
        geom = self._geom()
        if not image or not image.exists() or not geom:
            self.disk_label.setText("")
            return
        try:
            u = disk_usage(image, geom)
        except OSError:
            self.disk_label.setText("")
            return
        k = u["blksize"] // 1024
        self.disk_label.setText(
            f"Used {u['used_blocks'] * k}K  Free {u['free_blocks'] * k}K  "
            f"of {u['total_blocks'] * k}K  ·  dir {u['dir_used']}/{u['dir_total']}"
        )

    def _selected(self) -> dict | None:
        row = self.table.currentRow()
        if 0 <= row < len(self.entries):
            return self.entries[row]
        return None

    def _selected_entries(self) -> list[dict]:
        """All selected rows, in table order (supports multi-select)."""
        rows = sorted({index.row() for index in self.table.selectedIndexes()})
        return [self.entries[r] for r in rows if 0 <= r < len(self.entries)]

    def _raw_for(self, name: str) -> list[dict]:
        base = name.upper().split(".")
        want = (base[0][:8], (base[1] if len(base) > 1 else "")[:3])
        return [e for e in self.raw_entries if (e["name"], e["ext"]) == want]

    def _update_props(self) -> None:
        ent = self._selected()
        if not ent:
            self.props.setText("Select a file to see its directory entry.")
            return
        lines = [
            f"<b>{ent['name']}</b>",
            f"Size: {ent['size']:,} bytes",
            f"Attributes: {ent['attr']}",
        ]
        raw = self._raw_for(ent["name"])
        if raw:
            lines.append("<hr>")
            for e in raw:
                flag = e["attrs"] or "-"
                lines.append(
                    f"user {e['user']} · extent {e['extent']} · rc {e['rc']} · "
                    f"{len(e['blocks'])} blocks · attr {flag}"
                )
        self.props.setText("<br>".join(lines))

    # -- file operations ----------------------------------------------------
    def _temp_copy(self, ent: dict) -> Path | None:
        image = self._image()
        fmt = self.format_combo.currentText()
        if not image or self.tools is None:
            return None
        tmp = Path(tempfile.mkdtemp(prefix="tbcpm-"))
        try:
            self.tools.extract(image, fmt, self.user_spin.value(), ent["name"], tmp)
        except CpmToolsError as exc:
            self._warn(f"Could not read file:\n{exc}")
            return None
        files = list(tmp.iterdir())
        return files[0] if files else None

    def view_selected(self) -> None:
        ent = self._selected()
        if not ent:
            return
        path = self._temp_copy(ent)
        if path is None:
            return
        ViewerDialog(ent["name"], path.read_bytes(), self).exec()

    def extract_selected(self) -> None:
        entries = self._selected_entries()
        image = self._image()
        fmt = self.format_combo.currentText()
        if not entries or not image or self.tools is None:
            return
        dest_dir = QFileDialog.getExistingDirectory(
            self, "Extract to directory", str(Path.home())
        )
        if not dest_dir:
            return
        user = self.user_spin.value()
        errors: list[str] = []
        for ent in entries:
            try:
                self.tools.extract(image, fmt, user, ent["name"], Path(dest_dir))
            except CpmToolsError as exc:
                errors.append(f"{ent['name']}: {exc}")
        if errors:
            self._warn("Some extracts failed:\n" + "\n".join(errors))
        else:
            self.statusBar().showMessage(
                f"extracted {len(entries)} file(s) ({user}:…) → {dest_dir}"
            )

    def add_file(self) -> None:
        image = self._image()
        fmt = self.format_combo.currentText()
        if not image or self.tools is None:
            return
        path, _ = QFileDialog.getOpenFileName(
            self, "Add file to image", str(Path.home()), IMPORT_FILTER
        )
        if not path:
            return
        self._add_paths([Path(path)])

    def import_directory(self) -> None:
        image = self._image()
        if not image or self.tools is None:
            return
        folder = QFileDialog.getExistingDirectory(
            self, "Import directory into image", str(Path.home())
        )
        if not folder:
            return
        files = sorted(p for p in Path(folder).iterdir() if p.is_file())
        if not files:
            self._warn(f"No files in {folder}")
            return
        self._add_paths(files)

    def _add_paths(self, paths: list[Path]) -> None:
        image = self._image()
        fmt = self.format_combo.currentText()
        if not image or self.tools is None:
            return
        user = self.user_spin.value()
        errors: list[str] = []
        added = 0
        for host in paths:
            if not host.is_file():
                continue
            name = host.name.upper()
            try:
                self.tools.add(image, fmt, user, host, name)
                added += 1
            except CpmToolsError as exc:
                errors.append(f"{name}: {exc}")
        self.refresh()
        msg = f"added {added} file(s) to {user}:"
        if errors:
            self._warn(msg + "\n\n" + "\n".join(errors))
        else:
            self.statusBar().showMessage(msg)

    def rename_selected(self) -> None:
        ent = self._selected()
        image = self._image()
        if not ent or not image or self.tools is None:
            return
        old = ent["name"]
        new, ok = QInputDialog.getText(
            self, "Rename", f"New name for {old}:", text=old.upper()
        )
        if not ok:
            return
        new = new.strip()
        if not new or new.upper() == old.upper():
            return
        if not valid_name(new):
            self._warn(f"'{new}' is not a valid 8.3 CP/M filename.")
            return
        user = self.user_spin.value()
        try:
            changed = rename_in_image(image, self._geom(), user, old, new)
        except OSError as exc:
            self._warn(f"Rename failed:\n{exc}")
            return
        if not changed:
            self._warn(f"No directory entry found for {user}:{old}")
            return
        self.refresh()
        self.statusBar().showMessage(
            f"renamed {old} → {new} ({changed} extent(s))"
        )

    def set_attributes(self) -> None:
        entries = self._selected_entries()
        image = self._image()
        fmt = self.format_combo.currentText()
        if not entries or not image or self.tools is None:
            return
        raw = self._raw_for(entries[0]["name"])
        current = raw[0]["attrs"] if raw else ""
        dlg = AttrDialog(current, self)
        if dlg.exec() != QDialog.Accepted:
            return
        attrs = dlg.attributes() or "n"  # empty selection -> reset all
        user = self.user_spin.value()
        errors: list[str] = []
        for ent in entries:
            try:
                self.tools.set_attrs(image, fmt, user, ent["name"], attrs)
            except CpmToolsError as exc:
                errors.append(f"{ent['name']}: {exc}")
        self.refresh()
        if errors:
            self._warn("Some attribute changes failed:\n" + "\n".join(errors))
        else:
            self.statusBar().showMessage(
                f"set attrs '{attrs}' on {len(entries)} file(s)"
            )

    def delete_selected(self) -> None:
        entries = self._selected_entries()
        image = self._image()
        fmt = self.format_combo.currentText()
        if not entries or not image or self.tools is None:
            return
        user = self.user_spin.value()
        if len(entries) == 1:
            msg = f"Delete {user}:{entries[0]['name']} from {image.name}?"
        else:
            names = ", ".join(e["name"] for e in entries)
            msg = f"Delete {len(entries)} files from {image.name}?\n\n{names}"
        if QMessageBox.question(self, APP_NAME, msg) != QMessageBox.Yes:
            return
        errors: list[str] = []
        for ent in entries:
            try:
                self.tools.delete(image, fmt, user, ent["name"])
            except CpmToolsError as exc:
                errors.append(f"{ent['name']}: {exc}")
        self.refresh()
        if errors:
            self._warn("Some deletes failed:\n" + "\n".join(errors))
        else:
            self.statusBar().showMessage(f"deleted {len(entries)} file(s)")

    def new_image(self) -> None:
        fmt = self.format_combo.currentText()
        if not fmt or self.tools is None:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "New CP/M image", str(Path.home()), IMAGE_FILTER
        )
        if not path:
            return
        geom = self._geom()
        try:
            size = (
                int(geom["tracks"]) * int(geom["sectrk"]) * int(geom["seclen"])
                if {"tracks", "sectrk", "seclen"} <= geom.keys()
                else 0
            )
        except ValueError:
            size = 0
        try:
            self.tools.mkfs(Path(path), fmt, size or None)
        except CpmToolsError as exc:
            self._warn(f"mkfs failed:\n{exc}")
            return
        self.image_edit.setText(path)
        self.refresh()
        self.statusBar().showMessage(f"created {path} [{fmt}] ({size:,} bytes)")

    # -- drag & drop --------------------------------------------------------
    def dragEnterEvent(self, event) -> None:  # noqa: N802 (Qt API)
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event) -> None:  # noqa: N802 (Qt API)
        if not self._image() or self.tools is None:
            self._warn("Open an image first, then drop files onto it.")
            return
        files: list[Path] = []
        for url in event.mimeData().urls():
            path = Path(url.toLocalFile())
            if path.is_dir():
                files.extend(p for p in sorted(path.iterdir()) if p.is_file())
            elif path.is_file():
                files.append(path)
        if files:
            self._add_paths(files)
            event.acceptProposedAction()

    # -- helpers ------------------------------------------------------------
    def _warn(self, text: str) -> None:
        QMessageBox.warning(self, APP_NAME, text)


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    image = sys.argv[1] if len(sys.argv) > 1 else None
    win = MainWindow(image=image)
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
