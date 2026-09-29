#!/usr/bin/env python3
"""Z80NEO Loader — a PySide6 GUI for the z80neo serial bootloader.

Sends a program to the z80neo over the console UART (UART0, 115200 8N1) as
Intel HEX, paced one record at a time so the 128 kHz Z80 can keep up.  The
Z80-side loader lives in ``software/z80neo/loader`` (``zload.ihx``); see its
README for the protocol.

Usage:
    source venv/bin/activate    # if you made one
    python z80neo_loader.py [file]
"""

from __future__ import annotations

import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

from PySide6.QtCore import QThread, Signal
from PySide6.QtGui import QAction, QFont, QKeySequence, QTextCursor
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

APP_NAME = "Z80NEO Loader"
BAUD_DEFAULT = 115200
ACK = "."
ACK_TIMEOUT = 8.0          # seconds; the first page-0 record zeroes 16 KB first
PROMPT = "ld>"
FILE_FILTER = (
    "Programs (*.hex *.ihx *.bin *.com *.rom);;"
    "Intel HEX (*.hex *.ihx);;"
    "Binary (*.bin *.com *.rom);;"
    "All files (*)"
)

COMMON_PORTS = [
    "/dev/ttyACM0", "/dev/ttyACM1",
    "/dev/ttyUSB0", "/dev/ttyUSB1",
    "/dev/serial0", "/dev/ttyAMA0",
]


def intel_hex_record(addr: int, data: bytes) -> str:
    rec = bytes([len(data), (addr >> 8) & 0xFF, addr & 0xFF, 0x00]) + data
    csum = (-sum(rec)) & 0xFF
    return ":" + rec.hex().upper() + f"{csum:02X}"


def binary_to_hex(data: bytes, base: int, per_line: int = 16) -> list[str]:
    lines = [
        intel_hex_record(base + off, data[off:off + per_line])
        for off in range(0, len(data), per_line)
    ]
    lines.append(":00000001FF")
    return lines


@dataclass
class ImageInfo:
    lines: list[str] = field(default_factory=list)
    fmt: str = "?"
    count: int = 0
    lo: int | None = None
    hi: int | None = None
    entry: int | None = None

    def summary(self) -> str:
        if self.lo is None:
            span = "no data"
        elif self.lo == self.hi:
            span = f"0x{self.lo:04X}"
        else:
            span = f"0x{self.lo:04X}-0x{self.hi:04X}"
        ent = f"  entry 0x{self.entry:04X}" if self.entry is not None else ""
        return f"{self.fmt}  {self.count} bytes  {span}{ent}  ({len(self.lines)} lines)"


def analyze(lines: list[str]) -> ImageInfo:
    info = ImageInfo(lines=list(lines))
    base = 0
    for ln in lines:
        if ln.startswith(":"):
            if info.fmt == "?":
                info.fmt = "Intel HEX"
            try:
                rec = bytes.fromhex(ln[1:])
            except ValueError:
                continue
            if len(rec) < 5 or (sum(rec) & 0xFF) != 0:
                continue
            count, addr, typ = rec[0], (rec[1] << 8) | rec[2], rec[3]
            data = rec[4:4 + count]
            if typ == 0:
                a = (base + addr) & 0xFFFF
                info.count += count
                info.lo = a if info.lo is None else min(info.lo, a)
                end = a + count - 1
                info.hi = end if info.hi is None else max(info.hi, end)
            elif typ == 2 and count >= 2:
                base = ((data[0] << 8) | data[1]) << 4
            elif typ in (3, 5) and count >= 2:
                info.entry = (data[0] << 8) | data[1]
        elif ln.startswith("@"):
            if info.fmt == "?":
                info.fmt = "@ block"
            body = ln[1:]
            head = body.split(None, 1)
            if not head:
                continue
            try:
                a = int(head[0], 16) & 0xFFFF
            except ValueError:
                continue
            if len(head) > 1:
                raw = "".join(head[1].split())
                try:
                    data = bytes.fromhex(raw)
                except ValueError:
                    data = b""
                n = len(data)
                info.count += n
                if n:
                    info.lo = a if info.lo is None else min(info.lo, a)
                    end = a + n - 1
                    info.hi = end if info.hi is None else max(info.hi, end)
    return info


def ensure_terminator(lines: list[str]) -> list[str]:
    """The loader runs on an Intel EOF record or a blank line.  Append one when
    the source (e.g. an `@addr` block file) does not carry its own."""
    if not lines:
        return lines
    last = lines[-1]
    if last.startswith(":") and len(last) >= 11:
        try:
            rec = bytes.fromhex(last[1:])
        except ValueError:
            rec = b""
        if len(rec) >= 4 and rec[3] == 1:
            return lines
    return lines + [":00000001FF"]


def parse_source(path: Path, bin_base: int) -> ImageInfo:
    raw = path.read_bytes()
    stripped = raw.lstrip()
    if stripped[:1] in (b":", b"@"):
        text = raw.decode("ascii", "replace")
        lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
        return analyze(ensure_terminator(lines))
    lines = binary_to_hex(raw, bin_base)
    info = analyze(lines)
    info.fmt = f"Raw binary @0x{bin_base:04X}"
    return info


def list_ports() -> list[str]:
    ports: list[str] = []
    try:
        from serial.tools import list_ports as lp
        ports = [p.device for p in lp.comports()]
    except Exception:
        pass
    # Hide the kernel's /dev/ttyS* legacy ports unless there is nothing else.
    useful = [p for p in ports if not p.startswith("/dev/ttyS")]
    if useful:
        ports = useful
    if not ports:
        ports = [p for p in COMMON_PORTS if Path(p).exists()]
    return ports


class LoaderWorker(QThread):
    log = Signal(str)
    progress = Signal(int, int)
    status = Signal(str)
    done = Signal(bool, str)

    def __init__(self, port: str, baud: int, info: ImageInfo, follow: bool):
        super().__init__()
        self.port = port
        self.baud = baud
        self.info = info
        self.follow = follow
        self._stop = False

    def stop(self) -> None:
        self._stop = True

    def _read_for(self, seconds: float) -> bytes:
        out = bytearray()
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            try:
                data = self.ser.read(4096)  # type: ignore[attr-defined]
            except Exception:
                break
            if data:
                out += data
                end = time.monotonic() + 0.15
        return bytes(out)

    def _drain(self, seconds: float) -> None:
        text = self._read_for(seconds).decode("latin-1")
        if text:
            self.log.emit(text)

    def _wait_ack(self) -> bool:
        buf = bytearray()
        end = time.monotonic() + ACK_TIMEOUT
        while time.monotonic() < end and not self._stop:
            try:
                ch = self.ser.read(1)  # type: ignore[attr-defined]
            except Exception:
                return False
            if not ch:
                continue
            if ch == ACK.encode():
                text = buf.decode("latin-1")
                if text:
                    self.log.emit(text)
                return True
            buf += ch
        text = buf.decode("latin-1")
        if text:
            self.log.emit(text)
        return False

    def run(self) -> None:
        try:
            import serial  # noqa: F401
        except ImportError:
            self.done.emit(False, "pyserial is not installed (pip install pyserial)")
            return
        import serial as pyserial

        try:
            self.ser = pyserial.Serial(self.port, self.baud, timeout=0.1)
        except Exception as exc:  # pragma: no cover - depends on hardware
            self.done.emit(False, f"cannot open {self.port}: {exc}")
            return

        try:
            self.status.emit(f"opened {self.port} @ {self.baud}")
            time.sleep(0.3)
            self._drain(0.4)

            total = len(self.info.lines)
            self.progress.emit(0, total)
            for i, ln in enumerate(self.info.lines, start=1):
                if self._stop:
                    self.done.emit(False, "stopped")
                    return
                self.ser.write(ln.encode("ascii") + b"\n")
                self.ser.flush()
                if not self._wait_ack():
                    self.done.emit(False, f"timeout waiting for ACK on record {i}/{total}")
                    return
                self.progress.emit(i, total)

            self.status.emit("all records sent")
            self._drain(0.8)  # capture the loader's run summary
            if not self.follow:
                self.done.emit(True, "loaded (see console below)")
                return

            self.status.emit("loaded; following console (Stop to end)")
            while not self._stop:
                try:
                    data = self.ser.read(4096)
                except Exception:
                    break
                if data:
                    self.log.emit(data.decode("latin-1"))
                else:
                    time.sleep(0.02)
            self.done.emit(True, "stopped")
        finally:
            try:
                self.ser.close()
            except Exception:
                pass


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(APP_NAME)
        self.resize(860, 620)
        self.info: ImageInfo | None = None
        self.worker: LoaderWorker | None = None
        self.setAcceptDrops(True)
        self._build_ui()
        self.refresh_ports()

    # ---- UI ----------------------------------------------------------------

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        conn = QGroupBox("Connection")
        form = QFormLayout(conn)
        port_row = QHBoxLayout()
        self.port_combo = QComboBox()
        self.port_combo.setEditable(True)
        refresh = QPushButton("Refresh")
        refresh.clicked.connect(self.refresh_ports)
        port_row.addWidget(self.port_combo, 1)
        port_row.addWidget(refresh)
        form.addRow("Port:", port_row)

        baud_row = QHBoxLayout()
        self.baud_combo = QComboBox()
        for b in (115200, 57600, 38400, 19200, 9600):
            self.baud_combo.addItem(str(b), b)
        baud_row.addWidget(self.baud_combo)
        baud_row.addStretch(1)
        form.addRow("Baud:", baud_row)
        root.addWidget(conn)

        prog = QGroupBox("Program")
        pv = QVBoxLayout(prog)
        file_row = QHBoxLayout()
        self.file_edit = QLineEdit()
        self.file_edit.setReadOnly(True)
        self.file_edit.setPlaceholderText("drop a .hex / .ihx / .bin here, or Browse")
        browse = QPushButton("Browse…")
        browse.clicked.connect(self.browse)
        file_row.addWidget(self.file_edit, 1)
        file_row.addWidget(browse)
        pv.addLayout(file_row)

        base_row = QHBoxLayout()
        base_row.addWidget(QLabel("Raw binary base:"))
        self.base_spin = QSpinBox()
        self.base_spin.setRange(0, 0xFFFF)
        self.base_spin.setDisplayIntegerBase(16)
        self.base_spin.setPrefix("0x")
        self.base_spin.setValue(0x0000)
        self.base_spin.valueChanged.connect(lambda _=0: self.reload())
        base_row.addWidget(self.base_spin)
        base_row.addStretch(1)
        pv.addLayout(base_row)

        self.info_label = QLabel("no file loaded")
        f = QFont("monospace")
        f.setStyleHint(QFont.StyleHint.TypeWriter)
        self.info_label.setFont(f)
        pv.addWidget(self.info_label)

        btn_row = QHBoxLayout()
        self.follow_check = QCheckBox("Follow console after load")
        self.follow_check.setChecked(True)
        btn_row.addWidget(self.follow_check)
        btn_row.addStretch(1)
        self.load_btn = QPushButton("Load && Run")
        self.load_btn.setDefault(True)
        self.load_btn.clicked.connect(self.start_load)
        self.stop_btn = QPushButton("Stop")
        self.stop_btn.setEnabled(False)
        self.stop_btn.clicked.connect(self.stop_load)
        btn_row.addWidget(self.stop_btn)
        btn_row.addWidget(self.load_btn)
        pv.addLayout(btn_row)
        root.addWidget(prog)

        self.progress = QProgressBar()
        self.progress.setTextVisible(True)
        root.addWidget(self.progress)

        self.console = QPlainTextEdit()
        self.console.setReadOnly(True)
        self.console.setFont(f)
        self.console.setMaximumBlockCount(5000)
        root.addWidget(self.console, 1)

        clear = QAction("Clear log", self)
        clear.setShortcut(QKeySequence("Ctrl+L"))
        clear.triggered.connect(self.console.clear)
        self.addAction(clear)

        self.statusBar().showMessage("ready")

    # ---- helpers -----------------------------------------------------------

    def refresh_ports(self) -> None:
        current = self.port_combo.currentText()
        self.port_combo.clear()
        for p in list_ports():
            self.port_combo.addItem(p)
        if current:
            self.port_combo.setCurrentText(current)
        elif not self.port_combo.count():
            self.port_combo.setCurrentText("/dev/ttyACM0")

    def browse(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Open program", "", FILE_FILTER)
        if path:
            self.load_file(Path(path))

    def load_file(self, path: Path) -> None:
        self.file_edit.setText(str(path))
        self.reload()

    def reload(self) -> None:
        path = self.file_edit.text()
        if not path or not Path(path).exists():
            return
        try:
            self.info = parse_source(Path(path), self.base_spin.value())
        except Exception as exc:
            self.info = None
            self.info_label.setText(f"parse error: {exc}")
            return
        self.info_label.setText(self.info.summary())
        self.statusBar().showMessage(f"loaded {Path(path).name}")

    # ---- load --------------------------------------------------------------

    def start_load(self) -> None:
        if self.worker is not None and self.worker.isRunning():
            return
        if self.info is None or not self.info.lines:
            QMessageBox.warning(self, APP_NAME, "Choose a program file first.")
            return
        port = self.port_combo.currentText().strip()
        if not port:
            QMessageBox.warning(self, APP_NAME, "Choose a serial port.")
            return
        if self.info.lo is not None and self.info.lo < 0x4000 and self.info.hi is not None and self.info.hi >= 0x4000:
            QMessageBox.warning(
                self, APP_NAME,
                "This image mixes 0x0000-0x3FFF and 0x4000+ addresses.\n"
                "The loader cannot relocate that; split or relink it.",
            )
            return

        self.console.clear()
        self.progress.setValue(0)
        self.progress.setMaximum(len(self.info.lines))
        self.load_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)

        self.worker = LoaderWorker(
            port, self.baud_combo.currentData(),
            self.info, self.follow_check.isChecked(),
        )
        self.worker.log.connect(self._on_log)
        self.worker.progress.connect(self._on_progress)
        self.worker.status.connect(self.statusBar().showMessage)
        self.worker.done.connect(self._on_done)
        self.worker.finished.connect(self._on_worker_finished)
        self.worker.start()

    def stop_load(self) -> None:
        if self.worker is not None:
            self.worker.stop()
            self.statusBar().showMessage("stopping…")

    def _on_log(self, text: str) -> None:
        self.console.moveCursor(QTextCursor.MoveOperation.End)
        self.console.insertPlainText(text)
        self.console.moveCursor(QTextCursor.MoveOperation.End)

    def _on_progress(self, done: int, total: int) -> None:
        self.progress.setMaximum(total)
        self.progress.setValue(done)

    def _on_done(self, ok: bool, msg: str) -> None:
        self.load_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.statusBar().showMessage(("ok: " if ok else "failed: ") + msg)

    def _on_worker_finished(self) -> None:
        self.worker = None

    # ---- drag & drop -------------------------------------------------------

    def dragEnterEvent(self, event) -> None:  # noqa: N802
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event) -> None:  # noqa: N802
        urls = event.mimeData().urls()
        if urls:
            self.load_file(Path(urls[0].toLocalFile()))


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    win = MainWindow()
    if len(sys.argv) > 1:
        win.load_file(Path(sys.argv[1]))
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
