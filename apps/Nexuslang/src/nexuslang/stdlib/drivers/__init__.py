"""
Device driver framework for NexusLang.

Provides userspace-accessible primitives for interacting with device drivers
and hardware through the Linux kernel interfaces:

  Character devices  - /dev/XXX files (serial, tty, GPIO, custom drivers)
  Block devices      - /dev/sdX, /dev/nvme0n1 (sector-level I/O)
  PCI devices        - /sys/bus/pci enumeration and resource access
  I2C devices        - /dev/i2c-X bus communication
  SPI devices        - /dev/spidevX.Y bus communication
  GPIO               - /sys/class/gpio and character device GPIO
  Interrupt handling - Signal-based interrupt simulation + hardware IRQ info
  Device tree        - /proc/device-tree / /sys/firmware/devicetree parsing
  udev               - /sys/class/* device enumeration
  Kernel modules     - modprobe/rmmod wrappers, /proc/modules querying
  USB devices        - /sys/bus/usb enumeration (vendor/product/serial)
  Network devices    - /sys/class/net stats and ioctl control, raw sockets
  DMA buffers        - /dev/dma_heap allocation (Linux 5.6+, dma-buf FDs)
  VFIO               - User-space PCI device access via /dev/vfio (IOMMU-mapped BARs, IRQs)

All operations degrade gracefully on platforms that do not expose the
required kernel interfaces: a DriverError is raised rather than crashing.

Platform notes
--------------
Full support requires Linux with appropriate kernel drivers loaded and
sufficient privileges (root or appropriate group membership).
Most read operations work without root; write/ioctl usually require elevated
privileges or appropriate udev rules.
VFIO and DMA heap operations require root or specific kernel configs.
"""
from __future__ import annotations

import os
import sys
import stat
import mmap
import signal
import struct
import ctypes
import ctypes.util
import platform
import threading
import subprocess
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

_IS_LINUX = platform.system() == "Linux"
_IS_POSIX = os.name == "posix"


# ---------------------------------------------------------------------------
# Errors
# ---------------------------------------------------------------------------

class DriverError(Exception):
    """Raised when a driver operation fails."""


def _require_linux(op: str) -> None:
    if not _IS_LINUX:
        raise DriverError(f"drivers.{op}: operation requires Linux")


def _require_posix(op: str) -> None:
    if not _IS_POSIX:
        raise DriverError(f"drivers.{op}: operation requires POSIX")


# ---------------------------------------------------------------------------
# Character device
# ---------------------------------------------------------------------------

class CharDevice:
    """Represents an open character device node (e.g. /dev/ttyS0, /dev/zero)."""

    def __init__(self, path: str, flags: int = os.O_RDWR) -> None:
        self._path = path
        self._fd: Optional[int] = None
        self._flags = flags

    def open(self) -> None:
        """Open the device node."""
        if not os.path.exists(self._path):
            raise DriverError(f"Character device not found: {self._path}")
        try:
            self._fd = os.open(self._path, self._flags)
        except OSError as exc:
            raise DriverError(f"Cannot open {self._path}: {exc}") from exc

    def close(self) -> None:
        """Close the device node."""
        if self._fd is not None:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = None

    def read(self, n: int) -> bytes:
        """Read up to *n* bytes from the device."""
        if self._fd is None:
            raise DriverError("Device not open")
        try:
            return os.read(self._fd, n)
        except OSError as exc:
            raise DriverError(f"Read from {self._path} failed: {exc}") from exc

    def write(self, data: bytes) -> int:
        """Write *data* to the device.  Returns number of bytes written."""
        if self._fd is None:
            raise DriverError("Device not open")
        try:
            return os.write(self._fd, data)
        except OSError as exc:
            raise DriverError(f"Write to {self._path} failed: {exc}") from exc

    def ioctl(self, request: int, arg: int = 0) -> int:
        """Perform an ioctl(2) call on the device file descriptor."""
        _require_linux("CharDevice.ioctl")
        if self._fd is None:
            raise DriverError("Device not open")
        import fcntl
        try:
            result = fcntl.ioctl(self._fd, request, arg)
            return result if isinstance(result, int) else 0
        except OSError as exc:
            raise DriverError(
                f"ioctl(fd={self._fd}, req=0x{request:x}) failed: {exc}"
            ) from exc

    def ioctl_buffer(self, request: int, buf: bytearray) -> bytearray:
        """Perform an ioctl that exchanges a mutable buffer."""
        _require_linux("CharDevice.ioctl_buffer")
        if self._fd is None:
            raise DriverError("Device not open")
        import fcntl
        try:
            result = fcntl.ioctl(self._fd, request, buf, True)
            return bytearray(result)
        except OSError as exc:
            raise DriverError(
                f"ioctl_buffer(fd={self._fd}, req=0x{request:x}) failed: {exc}"
            ) from exc

    @property
    def path(self) -> str:
        return self._path

    @property
    def fd(self) -> Optional[int]:
        return self._fd

    def is_open(self) -> bool:
        return self._fd is not None

    def __repr__(self) -> str:
        state = "open" if self.is_open() else "closed"
        return f"CharDevice({self._path!r}, {state})"

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        self.close()


# ---------------------------------------------------------------------------
# Block device
# ---------------------------------------------------------------------------

class BlockDevice:
    """Represents an open block device (e.g. /dev/sda, /dev/nvme0n1)."""

    # Standard Linux sector size
    SECTOR_SIZE = 512

    def __init__(self, path: str) -> None:
        self._path = path
        self._fd: Optional[int] = None

    def open(self, read_only: bool = True) -> None:
        """Open the block device.

        Defaults to read-only; pass *read_only=False* for write access
        (requires appropriate privileges).
        """
        if not os.path.exists(self._path):
            raise DriverError(f"Block device not found: {self._path}")
        flags = os.O_RDONLY if read_only else os.O_RDWR
        try:
            self._fd = os.open(self._path, flags)
        except OSError as exc:
            raise DriverError(f"Cannot open {self._path}: {exc}") from exc

    def close(self) -> None:
        if self._fd is not None:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = None

    def read_sector(self, lba: int) -> bytes:
        """Read one 512-byte sector at logical block address *lba*."""
        if self._fd is None:
            raise DriverError("Device not open")
        offset = lba * self.SECTOR_SIZE
        try:
            os.lseek(self._fd, offset, os.SEEK_SET)
            data = os.read(self._fd, self.SECTOR_SIZE)
        except OSError as exc:
            raise DriverError(
                f"read_sector(lba={lba}) on {self._path} failed: {exc}"
            ) from exc
        return data

    def write_sector(self, lba: int, data: bytes) -> None:
        """Write exactly one sector at *lba*.  Data must be SECTOR_SIZE bytes."""
        if self._fd is None:
            raise DriverError("Device not open")
        if len(data) != self.SECTOR_SIZE:
            raise DriverError(
                f"write_sector requires exactly {self.SECTOR_SIZE} bytes, "
                f"got {len(data)}"
            )
        offset = lba * self.SECTOR_SIZE
        try:
            os.lseek(self._fd, offset, os.SEEK_SET)
            os.write(self._fd, data)
        except OSError as exc:
            raise DriverError(
                f"write_sector(lba={lba}) on {self._path} failed: {exc}"
            ) from exc

    def get_size(self) -> int:
        """Return device size in bytes using BLKGETSIZE64 ioctl (Linux only)."""
        _require_linux("BlockDevice.get_size")
        if self._fd is None:
            raise DriverError("Device not open")
        import fcntl
        BLKGETSIZE64 = 0x80081272
        buf = bytearray(8)
        try:
            fcntl.ioctl(self._fd, BLKGETSIZE64, buf, True)
        except OSError as exc:
            raise DriverError(f"BLKGETSIZE64 failed: {exc}") from exc
        return struct.unpack_from("<Q", buf)[0]

    def get_logical_block_size(self) -> int:
        """Return reported logical block size via BLKSSZGET ioctl."""
        _require_linux("BlockDevice.get_logical_block_size")
        if self._fd is None:
            raise DriverError("Device not open")
        import fcntl
        BLKSSZGET = 0x1268
        buf = bytearray(4)
        try:
            fcntl.ioctl(self._fd, BLKSSZGET, buf, True)
        except OSError as exc:
            raise DriverError(f"BLKSSZGET failed: {exc}") from exc
        return struct.unpack_from("<I", buf)[0]

    def is_open(self) -> bool:
        return self._fd is not None

    def __repr__(self) -> str:
        state = "open" if self._fd is not None else "closed"
        return f"BlockDevice({self._path!r}, {state})"

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        self.close()


# ---------------------------------------------------------------------------
# PCI device enumeration
# ---------------------------------------------------------------------------

def _read_sysfs(path: str) -> str:
    """Read a sysfs file as stripped text, returning '' on error."""
    try:
        with open(path, "r", errors="replace") as fh:
            return fh.read().strip()
    except OSError:
        return ""


def _read_sysfs_hex(path: str) -> int:
    """Read a sysfs file as a hex integer, returning -1 on error."""
    raw = _read_sysfs(path)
    raw = raw.lstrip("0x").strip()
    if not raw:
        return -1
    try:
        return int(raw, 16)
    except ValueError:
        return -1


class PciDevice:
    """Represents a PCI device discovered via sysfs."""

    def __init__(self, sysfs_path: str) -> None:
        self._path = sysfs_path

    @property
    def address(self) -> str:
        """BDF address: 0000:00:1f.0"""
        return os.path.basename(self._path)

    @property
    def vendor_id(self) -> int:
        return _read_sysfs_hex(os.path.join(self._path, "vendor"))

    @property
    def device_id(self) -> int:
        return _read_sysfs_hex(os.path.join(self._path, "device"))

    @property
    def class_code(self) -> int:
        return _read_sysfs_hex(os.path.join(self._path, "class"))

    @property
    def subsystem_vendor(self) -> int:
        return _read_sysfs_hex(os.path.join(self._path, "subsystem_vendor"))

    @property
    def subsystem_device(self) -> int:
        return _read_sysfs_hex(os.path.join(self._path, "subsystem_device"))

    @property
    def driver(self) -> str:
        """Name of the currently bound kernel driver, or '' if none."""
        driver_link = os.path.join(self._path, "driver")
        if os.path.islink(driver_link):
            return os.path.basename(os.readlink(driver_link))
        return ""

    @property
    def enabled(self) -> bool:
        return _read_sysfs(os.path.join(self._path, "enable")) == "1"

    def read_config_byte(self, offset: int) -> int:
        """Read one byte from PCI configuration space (offset in bytes)."""
        _require_linux("PciDevice.read_config_byte")
        config_path = os.path.join(self._path, "config")
        try:
            with open(config_path, "rb") as fh:
                fh.seek(offset)
                raw = fh.read(1)
                return raw[0] if raw else -1
        except OSError as exc:
            raise DriverError(
                f"PCI config read at 0x{offset:x} failed: {exc}"
            ) from exc

    def read_config_word(self, offset: int) -> int:
        """Read two bytes (little-endian) from PCI config space."""
        _require_linux("PciDevice.read_config_word")
        config_path = os.path.join(self._path, "config")
        try:
            with open(config_path, "rb") as fh:
                fh.seek(offset)
                raw = fh.read(2)
                return struct.unpack_from("<H", raw)[0] if len(raw) == 2 else -1
        except OSError as exc:
            raise DriverError(
                f"PCI config read at 0x{offset:x} failed: {exc}"
            ) from exc

    def to_dict(self) -> Dict[str, Any]:
        return {
            "address": self.address,
            "vendor_id": f"0x{self.vendor_id:04x}",
            "device_id": f"0x{self.device_id:04x}",
            "class_code": f"0x{self.class_code:06x}",
            "driver": self.driver,
            "enabled": self.enabled,
        }

    def __repr__(self) -> str:
        return (f"PciDevice({self.address}, "
                f"vendor=0x{self.vendor_id:04x}, "
                f"device=0x{self.device_id:04x})")


def enumerate_pci_devices(
    vendor_id: Optional[int] = None,
    device_id: Optional[int] = None,
    class_code: Optional[int] = None,
) -> List[PciDevice]:
    """Return all PCI devices visible in sysfs, optionally filtered.

    Filters are applied as exact integer matches.  Pass *None* to match
    any value for that field.
    """
    _require_linux("enumerate_pci_devices")
    base = Path("/sys/bus/pci/devices")
    if not base.exists():
        raise DriverError("PCI sysfs not available: /sys/bus/pci/devices missing")
    devices = []
    for entry in sorted(base.iterdir()):
        dev = PciDevice(str(entry))
        if vendor_id is not None and dev.vendor_id != vendor_id:
            continue
        if device_id is not None and dev.device_id != device_id:
            continue
        if class_code is not None and dev.class_code != class_code:
            continue
        devices.append(dev)
    return devices


# ---------------------------------------------------------------------------
# I2C device
# ---------------------------------------------------------------------------

# Linux I2C ioctl numbers
_I2C_SLAVE = 0x0703
_I2C_SLAVE_FORCE = 0x0706
_I2C_RDWR = 0x0707
_I2C_SMBUS = 0x0720
_I2C_FUNCS = 0x0705

class I2cDevice:
    """Userspace I2C device client via /dev/i2c-X."""

    def __init__(self, bus: int, address: int) -> None:
        self._bus_path = f"/dev/i2c-{bus}"
        self._address = address
        self._fd: Optional[int] = None

    @property
    def bus(self) -> int:
        return int(self._bus_path.split("-")[-1])

    @property
    def address(self) -> int:
        return self._address

    def open(self) -> None:
        _require_linux("I2cDevice.open")
        if not os.path.exists(self._bus_path):
            raise DriverError(f"I2C bus not found: {self._bus_path}")
        try:
            self._fd = os.open(self._bus_path, os.O_RDWR)
        except OSError as exc:
            raise DriverError(f"Cannot open {self._bus_path}: {exc}") from exc
        import fcntl
        try:
            fcntl.ioctl(self._fd, _I2C_SLAVE, self._address)
        except OSError as exc:
            os.close(self._fd)
            self._fd = None
            raise DriverError(
                f"Cannot set I2C slave address 0x{self._address:02x}: {exc}"
            ) from exc

    def close(self) -> None:
        if self._fd is not None:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = None

    def is_open(self) -> bool:
        return self._fd is not None

    def read(self, n: int) -> bytes:
        """Read *n* bytes from the I2C device."""
        if self._fd is None:
            raise DriverError("I2C device not open")
        try:
            return os.read(self._fd, n)
        except OSError as exc:
            raise DriverError(f"I2C read failed: {exc}") from exc

    def write(self, data: bytes) -> int:
        """Write *data* to the I2C device."""
        if self._fd is None:
            raise DriverError("I2C device not open")
        try:
            return os.write(self._fd, data)
        except OSError as exc:
            raise DriverError(f"I2C write failed: {exc}") from exc

    def write_byte_data(self, register: int, value: int) -> None:
        """Write a single byte to a register (SMBus write-byte-data)."""
        self.write(bytes([register & 0xFF, value & 0xFF]))

    def read_byte_data(self, register: int) -> int:
        """Write register address then read back one byte."""
        self.write(bytes([register & 0xFF]))
        return self.read(1)[0]

    def read_word_data(self, register: int) -> int:
        """Read two bytes starting at *register*, returned as little-endian u16."""
        self.write(bytes([register & 0xFF]))
        data = self.read(2)
        return struct.unpack_from("<H", data)[0]

    def __repr__(self) -> str:
        return (f"I2cDevice(bus={self._bus_path!r}, "
                f"addr=0x{self._address:02x})")


# ---------------------------------------------------------------------------
# SPI device
# ---------------------------------------------------------------------------

# Linux SPI ioctl numbers
_SPI_IOC_MAGIC = ord("k")


def _SPI_IOC_MESSAGE(n: int) -> int:
    # _IOW macro equivalent: (direction=1<<30) | (size<<16) | (magic<<8) | nr
    # struct spi_ioc_transfer is 36 bytes each
    size = n * 36
    return (1 << 30) | (size << 16) | (_SPI_IOC_MAGIC << 8) | 0


class SpiDevice:
    """Userspace SPI device client via /dev/spidevX.Y."""

    MODE_0 = 0x00
    MODE_1 = 0x01
    MODE_2 = 0x02
    MODE_3 = 0x03

    def __init__(self, bus: int, cs: int) -> None:
        self._bus = bus
        self._cs = cs
        self._path = f"/dev/spidev{bus}.{cs}"
        self._fd: Optional[int] = None

    @property
    def bus(self) -> int:
        return self._bus

    @property
    def cs(self) -> int:
        return self._cs

    def open(self) -> None:
        _require_linux("SpiDevice.open")
        if not os.path.exists(self._path):
            raise DriverError(f"SPI device not found: {self._path}")
        try:
            self._fd = os.open(self._path, os.O_RDWR)
        except OSError as exc:
            raise DriverError(f"Cannot open {self._path}: {exc}") from exc

    def close(self) -> None:
        if self._fd is not None:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = None

    def is_open(self) -> bool:
        return self._fd is not None

    def transfer(self, tx_data: bytes, speed_hz: int = 500000) -> bytes:
        """Full-duplex SPI transfer using spi_ioc_transfer ioctl.

        Returns received bytes of the same length as *tx_data*.
        """
        _require_linux("SpiDevice.transfer")
        if self._fd is None:
            raise DriverError("SPI device not open")
        import fcntl, ctypes
        n = len(tx_data)
        rx_buf = (ctypes.c_uint8 * n)()
        tx_buf = (ctypes.c_uint8 * n)(*tx_data)
        # struct spi_ioc_transfer layout (36 bytes):
        # u64 tx_buf, u64 rx_buf, u32 len, u32 speed_hz, u16 delay_usecs,
        # u8 bits_per_word, u8 cs_change, u8 tx_nbits, u8 rx_nbits, u16 pad
        msg = struct.pack(
            "=QQIIHBBBBH",
            ctypes.addressof(tx_buf),
            ctypes.addressof(rx_buf),
            n,
            speed_hz,
            0,   # delay_usecs
            8,   # bits_per_word
            0,   # cs_change
            0,   # tx_nbits
            0,   # rx_nbits
            0,   # pad
        )
        buf = bytearray(msg)
        try:
            fcntl.ioctl(self._fd, _SPI_IOC_MESSAGE(1), buf)
        except OSError as exc:
            raise DriverError(f"SPI transfer failed: {exc}") from exc
        return bytes(rx_buf)

    def __repr__(self) -> str:
        return f"SpiDevice({self._path!r})"


# ---------------------------------------------------------------------------
# GPIO (sysfs interface)
# ---------------------------------------------------------------------------

class GpioPin:
    """Controls a single GPIO pin via the Linux sysfs GPIO interface.

    Usage::

        pin = GpioPin(18)
        pin.export()
        pin.set_direction("out")
        pin.write(1)
        pin.unexport()

    The character-device GPIO interface (/dev/gpiochipX) is preferred on
    newer kernels but not all boards expose it; the sysfs interface is used
    here for maximum portability.
    """

    # Direction constants
    IN = "in"
    OUT = "out"
    OUT_HIGH = "high"
    OUT_LOW = "low"

    @property
    def pin(self) -> int:
        return self._pin

    def __init__(self, pin: int) -> None:
        self._pin = pin
        self._base = Path(f"/sys/class/gpio/gpio{pin}")

    def export(self) -> None:
        """Export the GPIO pin to userspace via sysfs."""
        _require_linux("GpioPin.export")
        if self._base.exists():
            return  # Already exported
        try:
            with open("/sys/class/gpio/export", "w") as fh:
                fh.write(str(self._pin))
        except OSError as exc:
            raise DriverError(f"GPIO export pin {self._pin} failed: {exc}") from exc

    def unexport(self) -> None:
        """Unexport the GPIO pin."""
        _require_linux("GpioPin.unexport")
        try:
            with open("/sys/class/gpio/unexport", "w") as fh:
                fh.write(str(self._pin))
        except OSError as exc:
            raise DriverError(f"GPIO unexport pin {self._pin} failed: {exc}") from exc

    def set_direction(self, direction: str) -> None:
        """Set pin direction: "in", "out", "high", or "low"."""
        _require_linux("GpioPin.set_direction")
        direction_file = self._base / "direction"
        try:
            with open(direction_file, "w") as fh:
                fh.write(direction)
        except OSError as exc:
            raise DriverError(
                f"GPIO set_direction({direction}) for pin {self._pin} failed: {exc}"
            ) from exc

    def get_direction(self) -> str:
        """Return current pin direction."""
        _require_linux("GpioPin.get_direction")
        raw = _read_sysfs(str(self._base / "direction"))
        if not raw:
            raise DriverError(f"Cannot read direction for GPIO pin {self._pin}")
        return raw

    def read(self) -> int:
        """Read pin value (0 or 1)."""
        _require_linux("GpioPin.read")
        raw = _read_sysfs(str(self._base / "value"))
        if not raw:
            raise DriverError(f"Cannot read value for GPIO pin {self._pin}")
        return int(raw)

    def write(self, value: int) -> None:
        """Write pin value (0 or 1)."""
        _require_linux("GpioPin.write")
        try:
            with open(self._base / "value", "w") as fh:
                fh.write("1" if value else "0")
        except OSError as exc:
            raise DriverError(
                f"GPIO write({value}) for pin {self._pin} failed: {exc}"
            ) from exc

    def set_active_low(self, active_low: bool) -> None:
        """Configure active-low polarity."""
        _require_linux("GpioPin.set_active_low")
        try:
            with open(self._base / "active_low", "w") as fh:
                fh.write("1" if active_low else "0")
        except OSError as exc:
            raise DriverError(f"GPIO set_active_low failed: {exc}") from exc

    @property
    def pin_number(self) -> int:
        return self._pin

    def __repr__(self) -> str:
        return f"GpioPin({self._pin})"


def list_gpio_chips() -> List[str]:
    """Return a list of available GPIO chip names (e.g. ['gpiochip0'])."""
    _require_linux("list_gpio_chips")
    base = Path("/sys/class/gpio")
    if not base.exists():
        return []
    return [
        e.name for e in base.iterdir()
        if e.name.startswith("gpiochip")
    ]


# ---------------------------------------------------------------------------
# Interrupt / IRQ information
# ---------------------------------------------------------------------------

def list_irqs() -> List[Dict[str, Any]]:
    """Return a list of active IRQ entries from /proc/interrupts.

    Each entry is a dict with keys: 'irq', 'count', 'type', 'name'.
    Returns an empty list if /proc/interrupts is unavailable.
    """
    _require_linux("list_irqs")
    result: List[Dict[str, Any]] = []
    try:
        with open("/proc/interrupts", "r") as fh:
            lines = fh.read().splitlines()
    except OSError:
        return result
    for line in lines[1:]:  # skip header CPU0 ... CPUn
        parts = line.split()
        if not parts:
            continue
        irq_str = parts[0].rstrip(":")
        # Sum counts across all CPUs
        counts = []
        idx = 1
        while idx < len(parts) and parts[idx].isdigit():
            counts.append(int(parts[idx]))
            idx += 1
        irq_type = parts[idx] if idx < len(parts) else ""
        idx += 1
        name = " ".join(parts[idx:]) if idx < len(parts) else ""
        result.append({
            "irq": irq_str,
            "count": sum(counts),
            "type": irq_type,
            "name": name,
        })
    return result


def get_irq_affinity(irq: int) -> Optional[str]:
    """Read the CPU affinity mask for *irq* from /proc/irq/<irq>/smp_affinity.

    Returns the hex affinity mask string, or None if unavailable.
    """
    _require_linux("get_irq_affinity")
    path = f"/proc/irq/{irq}/smp_affinity"
    raw = _read_sysfs(path)
    return raw if raw else None


def set_irq_affinity(irq: int, affinity_hex: str) -> None:
    """Write a CPU affinity mask for *irq* to /proc/irq/<irq>/smp_affinity.

    Requires root privileges.
    """
    _require_linux("set_irq_affinity")
    path = f"/proc/irq/{irq}/smp_affinity"
    try:
        with open(path, "w") as fh:
            fh.write(affinity_hex.strip())
    except OSError as exc:
        raise DriverError(f"set_irq_affinity({irq}, {affinity_hex!r}): {exc}") from exc


# ---------------------------------------------------------------------------
# Software interrupt / signal simulation
# ---------------------------------------------------------------------------

class InterruptHandler:
    """Register Python callables as handlers for POSIX signals.

    Useful for testing interrupt-driven logic in userspace without real
    hardware IRQs.

    Usage::

        handler = InterruptHandler()
        handler.register(signal.SIGUSR1, lambda: print("IRQ received"))
        handler.activate()
        # ... send SIGUSR1 via os.kill(os.getpid(), signal.SIGUSR1)
        handler.deactivate()
    """

    def __init__(self) -> None:
        self._handlers: Dict[signal.Signals, Callable] = {}
        self._original: Dict[signal.Signals, Any] = {}
        self._lock = threading.Lock()

    def register(self, sig: int, callback: Callable) -> None:
        """Register *callback* for signal number *sig*."""
        _require_posix("InterruptHandler.register")
        with self._lock:
            self._handlers[signal.Signals(sig)] = callback

    def activate(self) -> None:
        """Install registered signal handlers."""
        _require_posix("InterruptHandler.activate")
        with self._lock:
            for sig, cb in self._handlers.items():
                prev = signal.signal(sig, lambda signum, frame, _cb=cb: _cb())
                self._original[sig] = prev

    def deactivate(self) -> None:
        """Restore original signal handlers."""
        _require_posix("InterruptHandler.deactivate")
        with self._lock:
            for sig, prev in self._original.items():
                try:
                    signal.signal(sig, prev)
                except (OSError, ValueError):
                    pass
            self._original.clear()

    def trigger(self, sig: int) -> None:
        """Send *sig* to the current process, invoking the registered handler.

        If no handler has been registered for *sig*, this method returns
        immediately (no-op) so that callers never accidentally deliver a
        signal whose default action would terminate the process.
        """
        _require_posix("InterruptHandler.trigger")
        sig_obj = signal.Signals(sig)
        with self._lock:
            if sig_obj not in self._handlers:
                return
        os.kill(os.getpid(), sig)


# ---------------------------------------------------------------------------
# Device tree (simplified read interface)
# ---------------------------------------------------------------------------

def read_device_tree_property(node_path: str, prop: str) -> Optional[bytes]:
    """Read a property file from the device tree.

    *node_path* is a path relative to /proc/device-tree (or absolute).
    Returns raw bytes or None if unavailable.

    Example::

        read_device_tree_property("chosen", "bootargs")
    """
    _require_linux("read_device_tree_property")
    if not node_path.startswith("/"):
        node_path = f"/proc/device-tree/{node_path}"
    full = os.path.join(node_path, prop)
    try:
        with open(full, "rb") as fh:
            return fh.read()
    except OSError:
        return None


def read_device_tree_string(node_path: str, prop: str) -> str:
    """Read a device-tree property as a null-terminated UTF-8 string."""
    data = read_device_tree_property(node_path, prop)
    if data is None:
        return ""
    return data.rstrip(b"\x00").decode("utf-8", errors="replace")


def list_device_tree_children(node_path: str) -> List[str]:
    """List child nodes of a device-tree node directory."""
    _require_linux("list_device_tree_children")
    if not node_path.startswith("/"):
        node_path = f"/proc/device-tree/{node_path}"
    try:
        return [
            e for e in os.listdir(node_path)
            if os.path.isdir(os.path.join(node_path, e))
        ]
    except OSError:
        return []


# ---------------------------------------------------------------------------
# udev / sysfs device enumeration helpers
# ---------------------------------------------------------------------------

def list_devices_by_class(device_class: str) -> List[Dict[str, str]]:
    """Enumerate devices in /sys/class/<device_class>.

    Returns a list of dicts with 'name' and common attribute keys found in
    the uevent file (DEVTYPE, DEVNAME, DRIVER, etc.).

    Example device_class values: 'net', 'block', 'input', 'tty', 'video4linux'
    """
    _require_linux("list_devices_by_class")
    base = Path(f"/sys/class/{device_class}")
    if not base.exists():
        return []
    result = []
    for entry in sorted(base.iterdir()):
        info: Dict[str, str] = {"name": entry.name}
        uevent_path = entry / "uevent"
        if uevent_path.exists():
            try:
                with open(uevent_path, "r") as fh:
                    for line in fh:
                        if "=" in line:
                            k, _, v = line.strip().partition("=")
                            info[k.lower()] = v
            except OSError:
                pass
        result.append(info)
    return result


def find_device_by_attr(
    device_class: str,
    attr: str,
    value: str,
) -> Optional[str]:
    """Find the first device name in *device_class* where *attr* equals *value*.

    Returns the device name string or None.
    """
    _require_linux("find_device_by_attr")
    base = Path(f"/sys/class/{device_class}")
    if not base.exists():
        return None
    for entry in sorted(base.iterdir()):
        attr_path = entry / attr
        if attr_path.exists():
            raw = _read_sysfs(str(attr_path))
            if raw == value.strip():
                return entry.name
    return None


def get_device_attribute(device_class: str, name: str, attr: str) -> str:
    """Read a single sysfs attribute for a named device.

    Example::

        get_device_attribute("net", "eth0", "speed")
    """
    _require_linux("get_device_attribute")
    path = f"/sys/class/{device_class}/{name}/{attr}"
    return _read_sysfs(path)


# ---------------------------------------------------------------------------
# Kernel module management
# ---------------------------------------------------------------------------

def _run_cmd(args: List[str], timeout: int = 10) -> Tuple[int, str, str]:
    """Run a command and return (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.returncode, result.stdout, result.stderr
    except FileNotFoundError as exc:
        raise DriverError(f"Command not found: {args[0]}") from exc
    except subprocess.TimeoutExpired as exc:
        raise DriverError(f"Command timed out: {args[0]}") from exc


def load_kernel_module(name: str, params: Optional[Dict[str, str]] = None) -> None:
    """Load a kernel module using modprobe.

    Parameters
    ----------
    name:
        Module name (e.g. ``"i2c_dev"``).
    params:
        Optional dictionary of module parameters (key=value pairs).

    Raises
    ------
    DriverError
        If modprobe is not found or the module cannot be loaded.
    """
    _require_linux("load_kernel_module")
    cmd = ["modprobe", name]
    if params:
        for k, v in params.items():
            cmd.append(f"{k}={v}")
    rc, _, stderr = _run_cmd(cmd)
    if rc != 0:
        raise DriverError(f"modprobe {name} failed (rc={rc}): {stderr.strip()}")


def unload_kernel_module(name: str, force: bool = False) -> None:
    """Unload a kernel module using rmmod.

    Parameters
    ----------
    name:
        Module name.
    force:
        If ``True`` pass ``--force`` to rmmod (dangerous, may panic kernel).

    Raises
    ------
    DriverError
        If rmmod fails.
    """
    _require_linux("unload_kernel_module")
    cmd = ["rmmod"]
    if force:
        cmd.append("--force")
    cmd.append(name)
    rc, _, stderr = _run_cmd(cmd)
    if rc != 0:
        raise DriverError(f"rmmod {name} failed (rc={rc}): {stderr.strip()}")


def is_module_loaded(name: str) -> bool:
    """Return ``True`` if the named kernel module is currently loaded.

    Reads ``/proc/modules`` rather than calling lsmod so it works without
    the module-init-tools package installed.
    """
    _require_linux("is_module_loaded")
    module_name = name.replace("-", "_")
    try:
        with open("/proc/modules") as fh:
            for line in fh:
                if line.split()[0] == module_name:
                    return True
    except OSError as exc:
        raise DriverError(f"Cannot read /proc/modules: {exc}") from exc
    return False


def list_loaded_modules() -> List[Dict[str, Any]]:
    """Return all currently loaded kernel modules.

    Each entry is a dict with keys:
    ``name``, ``size``, ``refcount``, ``dependencies``, ``state``, ``offset``.
    """
    _require_linux("list_loaded_modules")
    modules: List[Dict[str, Any]] = []
    try:
        with open("/proc/modules") as fh:
            for line in fh:
                parts = line.split()
                if len(parts) < 6:
                    continue
                deps_raw = parts[3]
                deps = [] if deps_raw in ("-", "") else deps_raw.rstrip(",").split(",")
                modules.append({
                    "name": parts[0],
                    "size": int(parts[1]),
                    "refcount": int(parts[2]),
                    "dependencies": [d for d in deps if d],
                    "state": parts[4],
                    "offset": parts[5].strip(),
                })
    except OSError as exc:
        raise DriverError(f"Cannot read /proc/modules: {exc}") from exc
    return modules


def get_module_info(name: str) -> Dict[str, str]:
    """Return metadata for a kernel module via modinfo.

    Returns a dict mapping field names to values
    (e.g. ``filename``, ``description``, ``author``, ``license``, ``depends``).

    Raises
    ------
    DriverError
        If modinfo is not available or returns non-zero.
    """
    _require_linux("get_module_info")
    rc, stdout, stderr = _run_cmd(["modinfo", name])
    if rc != 0:
        raise DriverError(f"modinfo {name} failed (rc={rc}): {stderr.strip()}")
    info: Dict[str, str] = {}
    for line in stdout.splitlines():
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        info[key.strip()] = value.strip()
    return info


def get_module_dependencies(name: str) -> List[str]:
    """Return direct kernel dependencies for a module.

    Uses ``modinfo --field=depends``.
    """
    _require_linux("get_module_dependencies")
    rc, stdout, stderr = _run_cmd(["modinfo", "--field=depends", name])
    if rc != 0:
        raise DriverError(
            f"modinfo --field=depends {name} failed (rc={rc}): {stderr.strip()}"
        )
    raw = stdout.strip()
    if not raw:
        return []
    return [d for d in raw.split(",") if d]


# ---------------------------------------------------------------------------
# USB device framework
# ---------------------------------------------------------------------------

class UsbDevice:
    """Represents a USB device discovered through the Linux sysfs bus.

    Attributes are read lazily from ``/sys/bus/usb/devices/<address>/``.
    """

    def __init__(self, sysfs_path: str) -> None:
        self._path = Path(sysfs_path)

    def _read(self, attr: str) -> str:
        try:
            return (self._path / attr).read_text().strip()
        except OSError:
            return ""

    def _read_hex(self, attr: str) -> Optional[int]:
        raw = self._read(attr)
        try:
            return int(raw, 16) if raw else None
        except ValueError:
            return None

    @property
    def sysfs_path(self) -> str:
        return str(self._path)

    @property
    def address(self) -> str:
        return self._path.name

    @property
    def vendor_id(self) -> Optional[int]:
        return self._read_hex("idVendor")

    @property
    def product_id(self) -> Optional[int]:
        return self._read_hex("idProduct")

    @property
    def manufacturer(self) -> str:
        return self._read("manufacturer")

    @property
    def product(self) -> str:
        return self._read("product")

    @property
    def serial_number(self) -> str:
        return self._read("serial")

    @property
    def bus_number(self) -> Optional[int]:
        raw = self._read("busnum")
        return int(raw) if raw.isdigit() else None

    @property
    def device_number(self) -> Optional[int]:
        raw = self._read("devnum")
        return int(raw) if raw.isdigit() else None

    @property
    def speed(self) -> str:
        return self._read("speed")

    @property
    def device_class(self) -> Optional[int]:
        return self._read_hex("bDeviceClass")

    @property
    def device_subclass(self) -> Optional[int]:
        return self._read_hex("bDeviceSubClass")

    @property
    def num_configurations(self) -> Optional[int]:
        raw = self._read("bNumConfigurations")
        return int(raw) if raw.isdigit() else None

    @property
    def max_power_ma(self) -> int:
        """Maximum power draw in milliamps from the active configuration."""
        raw = self._read("bMaxPower")
        # Kernel sysfs reports "100mA" style values
        raw = raw.replace("mA", "").strip()
        try:
            return int(raw)
        except (ValueError, TypeError):
            return 0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "address": self.address,
            "sysfs_path": self.sysfs_path,
            "vendor_id": f"{self.vendor_id:04x}" if self.vendor_id is not None else None,
            "product_id": f"{self.product_id:04x}" if self.product_id is not None else None,
            "manufacturer": self.manufacturer,
            "product": self.product,
            "serial_number": self.serial_number,
            "bus_number": self.bus_number,
            "device_number": self.device_number,
            "speed": self.speed,
            "device_class": self.device_class,
            "max_power_ma": self.max_power_ma,
        }

    def __repr__(self) -> str:
        vid = f"{self.vendor_id:04x}" if self.vendor_id is not None else "????"
        pid = f"{self.product_id:04x}" if self.product_id is not None else "????"
        return f"UsbDevice({vid}:{pid} at {self.address!r})"


def enumerate_usb_devices(
    vendor_id: Optional[int] = None,
    product_id: Optional[int] = None,
) -> List[UsbDevice]:
    """Return all USB devices visible through sysfs.

    Parameters
    ----------
    vendor_id:
        If given, filter by USB vendor ID (integer).
    product_id:
        If given, filter by USB product ID (integer).  Requires ``vendor_id``.

    Returns
    -------
    List[UsbDevice]
        Matching devices, empty list if none found or sysfs unavailable.
    """
    _require_linux("enumerate_usb_devices")
    usb_root = Path("/sys/bus/usb/devices")
    if not usb_root.exists():
        return []
    devices: List[UsbDevice] = []
    for entry in sorted(usb_root.iterdir()):
        # Skip interfaces (e.g. "1-1:1.0") and root hubs without idVendor
        if not (entry / "idVendor").exists():
            continue
        dev = UsbDevice(str(entry))
        if vendor_id is not None and dev.vendor_id != vendor_id:
            continue
        if product_id is not None and dev.product_id != product_id:
            continue
        devices.append(dev)
    return devices


def find_usb_device(vendor_id: int, product_id: int) -> Optional[UsbDevice]:
    """Find the first USB device matching vendor_id:product_id, or None."""
    matches = enumerate_usb_devices(vendor_id=vendor_id, product_id=product_id)
    return matches[0] if matches else None


# ---------------------------------------------------------------------------
# Network device drivers
# ---------------------------------------------------------------------------

# IOCTL codes for network interface control (from <linux/sockios.h>)
_SIOCGIFFLAGS   = 0x8913
_SIOCSIFFLAGS   = 0x8914
_SIOCGIFMTU     = 0x8921
_SIOCSIFMTU     = 0x8922
_SIOCGIFHWADDR  = 0x8927
_IFF_UP         = 0x1
_STRUCT_IFREQ   = "16sH14s"  # ifr_name (16 bytes) + ifr_flags (2) + pad (14)
_STRUCT_IFREQ_MTU = "16si12s"  # ifr_name (16) + ifr_mtu (int) + pad (12)


class NetDevice:
    """Represents a Linux network interface.

    Statistics and basic properties are read from
    ``/sys/class/net/<name>/`` and ``/proc/net/dev``.
    Mutating operations (MTU, flags) use socket ioctls.
    """

    def __init__(self, name: str) -> None:
        self._name = name

    @property
    def name(self) -> str:
        return self._name

    def _read_net_attr(self, attr: str) -> str:
        try:
            path = Path(f"/sys/class/net/{self._name}/{attr}")
            return path.read_text().strip()
        except OSError:
            return ""

    def _read_stat(self, stat: str) -> int:
        try:
            val = Path(f"/sys/class/net/{self._name}/statistics/{stat}").read_text().strip()
            return int(val)
        except (OSError, ValueError):
            return 0

    @property
    def mac_address(self) -> str:
        return self._read_net_attr("address")

    @property
    def mtu(self) -> int:
        raw = self._read_net_attr("mtu")
        return int(raw) if raw.isdigit() else 0

    @property
    def speed_mbps(self) -> Optional[int]:
        raw = self._read_net_attr("speed")
        try:
            v = int(raw)
            return v if v > 0 else None
        except (ValueError, TypeError):
            return None

    @property
    def operstate(self) -> str:
        return self._read_net_attr("operstate")

    @property
    def is_up(self) -> bool:
        return self.operstate.lower() == "up"

    @property
    def rx_bytes(self) -> int:
        return self._read_stat("rx_bytes")

    @property
    def tx_bytes(self) -> int:
        return self._read_stat("tx_bytes")

    @property
    def rx_packets(self) -> int:
        return self._read_stat("rx_packets")

    @property
    def tx_packets(self) -> int:
        return self._read_stat("tx_packets")

    @property
    def rx_errors(self) -> int:
        return self._read_stat("rx_errors")

    @property
    def tx_errors(self) -> int:
        return self._read_stat("tx_errors")

    @property
    def rx_dropped(self) -> int:
        return self._read_stat("rx_dropped")

    @property
    def tx_dropped(self) -> int:
        return self._read_stat("tx_dropped")

    def set_mtu(self, mtu: int) -> None:
        """Set the MTU for this interface via ioctl SIOCSIFMTU.

        Requires CAP_NET_ADMIN.
        """
        _require_linux("set_mtu")
        import socket as _socket
        sock = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM, 0)
        try:
            import fcntl
            ifreq = struct.pack(_STRUCT_IFREQ_MTU,
                                self._name.encode()[:15].ljust(16, b'\x00'),
                                int(mtu),
                                b'\x00' * 12)
            result = fcntl.ioctl(sock.fileno(), _SIOCSIFMTU, ifreq)
            _ = struct.unpack(_STRUCT_IFREQ_MTU, result)[1]
        except OSError as exc:
            raise DriverError(f"set_mtu({self._name}, {mtu}) failed: {exc}") from exc
        finally:
            sock.close()

    def _set_flags(self, flags_op: int) -> None:
        """Apply an ioctl to set/clear IFF_UP using SIOCSIFFLAGS."""
        _require_linux("set_flags")
        import socket as _socket
        import fcntl
        sock = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM, 0)
        try:
            # Get current flags
            ifreq = struct.pack(_STRUCT_IFREQ,
                                self._name.encode()[:15].ljust(16, b'\x00'),
                                0,
                                b'\x00' * 14)
            result = fcntl.ioctl(sock.fileno(), _SIOCGIFFLAGS, ifreq)
            cur_flags = struct.unpack(_STRUCT_IFREQ, result)[1]
            new_flags = cur_flags | flags_op if flags_op > 0 else cur_flags & ~(-flags_op)
            ifreq2 = struct.pack(_STRUCT_IFREQ,
                                 self._name.encode()[:15].ljust(16, b'\x00'),
                                 new_flags,
                                 b'\x00' * 14)
            fcntl.ioctl(sock.fileno(), _SIOCSIFFLAGS, ifreq2)
        except OSError as exc:
            raise DriverError(f"set_flags({self._name}) failed: {exc}") from exc
        finally:
            sock.close()

    def bring_up(self) -> None:
        """Bring the interface up (IFF_UP). Requires CAP_NET_ADMIN."""
        self._set_flags(_IFF_UP)

    def bring_down(self) -> None:
        """Bring the interface down. Requires CAP_NET_ADMIN."""
        self._set_flags(-_IFF_UP)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "mac_address": self.mac_address,
            "mtu": self.mtu,
            "speed_mbps": self.speed_mbps,
            "operstate": self.operstate,
            "is_up": self.is_up,
            "rx_bytes": self.rx_bytes,
            "tx_bytes": self.tx_bytes,
            "rx_packets": self.rx_packets,
            "tx_packets": self.tx_packets,
            "rx_errors": self.rx_errors,
            "tx_errors": self.tx_errors,
        }

    def __repr__(self) -> str:
        return f"NetDevice({self._name!r}, mac={self.mac_address!r}, state={self.operstate!r})"


def enumerate_net_devices() -> List[NetDevice]:
    """Return all network interfaces visible through ``/sys/class/net``."""
    _require_linux("enumerate_net_devices")
    net_root = Path("/sys/class/net")
    if not net_root.exists():
        return []
    return [NetDevice(entry.name) for entry in sorted(net_root.iterdir())
            if not entry.name.startswith(".")]


def create_raw_socket(interface: str) -> Any:
    """Create a raw packet socket bound to a network interface.

    Returns a ``socket.socket`` object capable of sending and receiving
    raw Ethernet frames.  Requires ``CAP_NET_RAW``.

    Parameters
    ----------
    interface:
        Interface name, e.g. ``"eth0"``.
    """
    _require_linux("create_raw_socket")
    import socket as _socket
    try:
        # AF_PACKET / SOCK_RAW / all protocols
        sock = _socket.socket(_socket.AF_PACKET, _socket.SOCK_RAW, 0)
        sock.bind((interface, 0))
        return sock
    except OSError as exc:
        raise DriverError(f"create_raw_socket({interface!r}) failed: {exc}") from exc


def send_raw_packet(sock: Any, packet: bytes) -> int:
    """Send a raw Ethernet frame.

    Parameters
    ----------
    sock:
        Socket returned by :func:`create_raw_socket`.
    packet:
        Complete Ethernet frame as bytes (including destination MAC, source
        MAC, EtherType, and payload — no FCS needed).

    Returns
    -------
    int
        Number of bytes sent.
    """
    try:
        return sock.send(packet)
    except OSError as exc:
        raise DriverError(f"send_raw_packet failed: {exc}") from exc


def receive_raw_packet(sock: Any, max_len: int = 65535) -> bytes:
    """Receive a raw Ethernet frame (blocking).

    Parameters
    ----------
    sock:
        Socket returned by :func:`create_raw_socket`.
    max_len:
        Maximum frame length to receive.

    Returns
    -------
    bytes
        The raw Ethernet frame bytes (may include FCS depending on driver).
    """
    try:
        return sock.recv(int(max_len))
    except OSError as exc:
        raise DriverError(f"receive_raw_packet failed: {exc}") from exc


# ---------------------------------------------------------------------------
# DMA buffer management
# ---------------------------------------------------------------------------

# dma_heap ioctl (Linux 5.6+, include/uapi/linux/dma-heap.h)
# struct dma_heap_allocation_data {
#     uint64_t len;          // requested buffer size
#     uint32_t fd;           // returned file descriptor
#     uint32_t fd_flags;     // O_RDWR | O_CLOEXEC
#     uint64_t heap_flags;   // must be zero
# };
_DMA_HEAP_IOCTL_ALLOC_BASE = 0xC0
_DMA_HEAP_IOC_MAGIC = 0x48  # 'H'
# IOCTL: _IOWR('H', 0, struct dma_heap_allocation_data)
# struct size = 8+4+4+8 = 24 bytes
_DMA_HEAP_IOCTL_ALLOC = (3 << 30) | (_DMA_HEAP_IOC_MAGIC << 8) | (0x00) | (24 << 16)
_DMA_HEAP_ALLOC_STRUCT = "=QIIQ"  # len, fd, fd_flags, heap_flags
_DMA_HEAP_ALLOC_SIZE = struct.calcsize(_DMA_HEAP_ALLOC_STRUCT)

_DEFAULT_DMA_HEAP = "/dev/dma_heap/system"


class DmaBuffer:
    """A DMA-coherent memory buffer allocated through ``/dev/dma_heap``.

    On Linux 5.6+ the kernel exports named heaps at ``/dev/dma_heap/``.
    The ``system`` heap provides cached (coherent) memory suitable for
    CPU-accessible DMA buffers.  The ``system-uncached`` heap (where
    available) provides write-combining memory.

    Usage::

        buf = DmaBuffer.allocate(4096)
        view = buf.map()
        view[0:4] = b"test"
        buf.unmap()
        buf.close()

    The buffer ``fd`` can be shared with kernel drivers that accept
    DMA-BUF file descriptors (e.g. V4L2, DRM).
    """

    def __init__(self, fd: int, size: int, heap: str) -> None:
        self._fd = fd
        self._size = size
        self._heap = heap
        self._mmap: Optional[mmap.mmap] = None

    @property
    def fd(self) -> int:
        """DMA-BUF file descriptor (share with kernel drivers)."""
        return self._fd

    @property
    def size(self) -> int:
        """Allocated buffer size in bytes."""
        return self._size

    @property
    def heap(self) -> str:
        """Heap name used for allocation."""
        return self._heap

    def map(self) -> mmap.mmap:
        """Map the DMA buffer into the process address space.

        Returns a ``mmap.mmap`` object that can be used like a bytearray.
        The mapping is cached; repeated calls return the same mapping.
        """
        if self._mmap is not None:
            return self._mmap
        if self._fd < 0:
            raise DriverError("DmaBuffer already closed")
        try:
            self._mmap = mmap.mmap(self._fd, self._size)
        except OSError as exc:
            raise DriverError(f"DmaBuffer.map() failed: {exc}") from exc
        return self._mmap

    def unmap(self) -> None:
        """Unmap the buffer from the process address space."""
        if self._mmap is not None:
            try:
                self._mmap.close()
            except OSError:
                pass
            self._mmap = None

    def close(self) -> None:
        """Release the DMA-BUF file descriptor."""
        self.unmap()
        if self._fd >= 0:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = -1

    def __enter__(self) -> "DmaBuffer":
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()

    def __repr__(self) -> str:
        return f"DmaBuffer(fd={self._fd}, size={self._size}, heap={self._heap!r})"

    @classmethod
    def allocate(cls, size: int, heap: str = "system") -> "DmaBuffer":
        """Allocate a DMA buffer from the named heap.

        Parameters
        ----------
        size:
            Size in bytes of the buffer to allocate.
        heap:
            Heap name.  Common values: ``"system"`` (cached),
            ``"system-uncached"`` (write-combining).  Available heaps
            can be listed with :func:`list_dma_heaps`.

        Returns
        -------
        DmaBuffer
            A new buffer object.  Caller is responsible for calling
            :meth:`close` when done (or using as a context manager).

        Raises
        ------
        DriverError
            If ``/dev/dma_heap`` is unavailable or the allocation fails.
        """
        _require_linux("DmaBuffer.allocate")
        heap_path = f"/dev/dma_heap/{heap}"
        if not os.path.exists(heap_path):
            raise DriverError(
                f"DMA heap {heap!r} not found at {heap_path}. "
                "Requires Linux 5.6+ with CONFIG_DMABUF_HEAPS=y"
            )
        try:
            heap_fd = os.open(heap_path, os.O_RDWR | os.O_CLOEXEC)
        except OSError as exc:
            raise DriverError(f"Cannot open DMA heap {heap!r}: {exc}") from exc

        try:
            import fcntl
            # Build dma_heap_allocation_data struct
            alloc_data = struct.pack(
                _DMA_HEAP_ALLOC_STRUCT,
                int(size),          # len
                0,                  # fd (output)
                os.O_RDWR | os.O_CLOEXEC,  # fd_flags
                0,                  # heap_flags (must be zero)
            )
            result = fcntl.ioctl(heap_fd, _DMA_HEAP_IOCTL_ALLOC, alloc_data)
            _, buf_fd, _, _ = struct.unpack(_DMA_HEAP_ALLOC_STRUCT, result)
        except OSError as exc:
            os.close(heap_fd)
            raise DriverError(
                f"dma_heap ioctl allocation failed (size={size}, heap={heap!r}): {exc}"
            ) from exc
        finally:
            os.close(heap_fd)

        return cls(buf_fd, size, heap)


def list_dma_heaps() -> List[str]:
    """Return names of all available DMA heaps (``/dev/dma_heap/``).

    Returns an empty list if the kernel does not support DMA heaps.
    """
    _require_linux("list_dma_heaps")
    heap_dir = Path("/dev/dma_heap")
    if not heap_dir.exists():
        return []
    return sorted(entry.name for entry in heap_dir.iterdir()
                  if not entry.name.startswith("."))


# ---------------------------------------------------------------------------
# VFIO user-space device driver framework
# ---------------------------------------------------------------------------

# VFIO IOCTL numbers (from <linux/vfio.h>)
# Base magic: VFIO_TYPE = ';' = 0x3b
_VFIO_TYPE = 0x3B

def _vfio_io(nr: int) -> int:
    """Build a VFIO _IO ioctl number."""
    return (_VFIO_TYPE << 8) | nr

def _vfio_ior(nr: int, size: int) -> int:
    """Build a VFIO _IOR ioctl number."""
    return (2 << 30) | (_VFIO_TYPE << 8) | nr | (size << 16)

def _vfio_iow(nr: int, size: int) -> int:
    """Build a VFIO _IOW ioctl number."""
    return (1 << 30) | (_VFIO_TYPE << 8) | nr | (size << 16)

def _vfio_iowr(nr: int, size: int) -> int:
    """Build a VFIO _IOWR ioctl number."""
    return (3 << 30) | (_VFIO_TYPE << 8) | nr | (size << 16)


VFIO_GET_API_VERSION       = _vfio_io(0)
VFIO_CHECK_EXTENSION       = _vfio_io(1)
VFIO_SET_IOMMU             = _vfio_io(2)
VFIO_TYPE1_IOMMU           = 1
VFIO_TYPE1v2_IOMMU         = 6
VFIO_GROUP_GET_STATUS      = _vfio_ior(3, 8)   # struct vfio_group_status
VFIO_GROUP_SET_CONTAINER   = _vfio_iow(4, 4)   # int fd
VFIO_GROUP_UNSET_CONTAINER = _vfio_io(5)
VFIO_GROUP_GET_DEVICE_FD   = _vfio_ior(6, 8)   # char * device name

# struct vfio_device_info  { uint32_t argsz; uint32_t flags; uint32_t num_regions; uint32_t num_irqs; }
_VFIO_DEVICE_INFO_STRUCT = "=IIII"
VFIO_DEVICE_GET_INFO       = _vfio_iowr(7, struct.calcsize(_VFIO_DEVICE_INFO_STRUCT))

# struct vfio_region_info { uint32_t argsz; uint32_t flags; uint32_t index; uint32_t cap_offset; uint64_t size; uint64_t offset; }
_VFIO_REGION_INFO_STRUCT = "=IIIIqq"
VFIO_DEVICE_GET_REGION_INFO = _vfio_iowr(8, struct.calcsize(_VFIO_REGION_INFO_STRUCT))

# VFIO region flags
VFIO_REGION_INFO_FLAG_READ  = 1 << 0
VFIO_REGION_INFO_FLAG_WRITE = 1 << 1
VFIO_REGION_INFO_FLAG_MMAP  = 1 << 2

# struct vfio_irq_info { uint32_t argsz; uint32_t flags; uint32_t index; uint32_t count; }
_VFIO_IRQ_INFO_STRUCT = "=IIII"
VFIO_DEVICE_GET_IRQ_INFO   = _vfio_iowr(9, struct.calcsize(_VFIO_IRQ_INFO_STRUCT))

# VFIO_DEVICE_SET_IRQS: variable-length struct — we use the fixed header portion
_VFIO_SET_IRQS_STRUCT = "=IIIII"
VFIO_DEVICE_SET_IRQS       = _vfio_iow(10, struct.calcsize(_VFIO_SET_IRQS_STRUCT))

VFIO_DEVICE_RESET          = _vfio_io(11)

# VFIO_IOMMU_MAP_DMA: struct { uint32_t argsz; uint32_t flags; uint64_t vaddr; uint64_t iova; uint64_t size; }
_VFIO_IOMMU_MAP_STRUCT = "=IIQQq"
VFIO_IOMMU_MAP_DMA         = _vfio_iowr(13, struct.calcsize(_VFIO_IOMMU_MAP_STRUCT))

# VFIO_IOMMU_UNMAP_DMA: struct { uint32_t argsz; uint32_t flags; uint64_t iova; uint64_t size; }
_VFIO_IOMMU_UNMAP_STRUCT = "=IIQQ"
VFIO_IOMMU_UNMAP_DMA       = _vfio_iowr(14, struct.calcsize(_VFIO_IOMMU_UNMAP_STRUCT))

# VFIO_IRQ flags
VFIO_IRQ_SET_DATA_NONE     = 1 << 0
VFIO_IRQ_SET_DATA_BOOL     = 1 << 1
VFIO_IRQ_SET_DATA_EVENTFD  = 1 << 2
VFIO_IRQ_SET_ACTION_MASK   = 1 << 3
VFIO_IRQ_SET_ACTION_UNMASK = 1 << 4
VFIO_IRQ_SET_ACTION_TRIGGER = 1 << 5


class VfioContainer:
    """VFIO container — the top-level IOMMU address-space manager.

    The container is opened via ``/dev/vfio/vfio``, then one or more
    VFIO groups are added to it before an IOMMU type is committed.

    Usage::

        container = VfioContainer()
        container.open()
        container.set_iommu(VFIO_TYPE1v2_IOMMU)
        ...
        container.close()
    """

    def __init__(self) -> None:
        self._fd: int = -1

    @property
    def fd(self) -> int:
        return self._fd

    def open(self) -> None:
        """Open the VFIO container."""
        _require_linux("VfioContainer.open")
        if not os.path.exists("/dev/vfio/vfio"):
            raise DriverError(
                "VFIO container /dev/vfio/vfio not found. "
                "Load the vfio module: modprobe vfio"
            )
        try:
            self._fd = os.open("/dev/vfio/vfio", os.O_RDWR)
        except OSError as exc:
            raise DriverError(f"Cannot open VFIO container: {exc}") from exc

    def close(self) -> None:
        """Close the VFIO container."""
        if self._fd >= 0:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = -1

    def check_extension(self, extension: int) -> bool:
        """Return True if the VFIO extension is supported."""
        if self._fd < 0:
            raise DriverError("VfioContainer not open")
        try:
            import fcntl
            ret = fcntl.ioctl(self._fd, VFIO_CHECK_EXTENSION, extension)
            return ret != 0
        except OSError as exc:
            raise DriverError(f"VFIO_CHECK_EXTENSION failed: {exc}") from exc

    def get_api_version(self) -> int:
        """Return the VFIO API version (should be 0)."""
        if self._fd < 0:
            raise DriverError("VfioContainer not open")
        try:
            import fcntl
            return fcntl.ioctl(self._fd, VFIO_GET_API_VERSION, 0)
        except OSError as exc:
            raise DriverError(f"VFIO_GET_API_VERSION failed: {exc}") from exc

    def set_iommu(self, iommu_type: int = VFIO_TYPE1v2_IOMMU) -> None:
        """Commit an IOMMU type for this container.

        Must be called after at least one group has been added.
        Typically use ``VFIO_TYPE1v2_IOMMU`` (recommended) or
        ``VFIO_TYPE1_IOMMU``.
        """
        if self._fd < 0:
            raise DriverError("VfioContainer not open")
        try:
            import fcntl
            fcntl.ioctl(self._fd, VFIO_SET_IOMMU, iommu_type)
        except OSError as exc:
            raise DriverError(f"VFIO_SET_IOMMU({iommu_type}) failed: {exc}") from exc

    def map_dma(self, vaddr: int, iova: int, size: int, flags: int = 0) -> None:
        """Map a virtual address range into the IOMMU at a given IOVA.

        Parameters
        ----------
        vaddr:
            Virtual address (process address space).
        iova:
            I/O virtual address (device-visible address).
        size:
            Mapping size in bytes.
        flags:
            VFIO_DMA_MAP_FLAG_* flags (0 for default read+write).
        """
        if self._fd < 0:
            raise DriverError("VfioContainer not open")
        try:
            import fcntl
            data = struct.pack(
                _VFIO_IOMMU_MAP_STRUCT,
                struct.calcsize(_VFIO_IOMMU_MAP_STRUCT),  # argsz
                int(flags),
                int(vaddr),
                int(iova),
                int(size),
            )
            fcntl.ioctl(self._fd, VFIO_IOMMU_MAP_DMA, data)
        except OSError as exc:
            raise DriverError(f"VFIO_IOMMU_MAP_DMA failed: {exc}") from exc

    def unmap_dma(self, iova: int, size: int) -> None:
        """Unmap a previously mapped IOVA range."""
        if self._fd < 0:
            raise DriverError("VfioContainer not open")
        try:
            import fcntl
            data = struct.pack(
                _VFIO_IOMMU_UNMAP_STRUCT,
                struct.calcsize(_VFIO_IOMMU_UNMAP_STRUCT),
                0,          # flags
                int(iova),
                int(size),
            )
            fcntl.ioctl(self._fd, VFIO_IOMMU_UNMAP_DMA, data)
        except OSError as exc:
            raise DriverError(f"VFIO_IOMMU_UNMAP_DMA failed: {exc}") from exc

    def __enter__(self) -> "VfioContainer":
        self.open()
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()

    def __repr__(self) -> str:
        return f"VfioContainer(fd={self._fd})"


class VfioGroup:
    """A VFIO IOMMU group — a set of devices sharing an IOMMU domain.

    Each VFIO group corresponds to a directory under ``/dev/vfio/<N>``.
    Before accessing devices the group must be added to a
    :class:`VfioContainer`.

    Usage::

        group = VfioGroup(42)
        group.open()
        group.set_container(container)
        dev = group.get_device("0000:01:00.0")
        ...
        group.close()
    """

    def __init__(self, group_id: int) -> None:
        self._group_id = group_id
        self._fd: int = -1

    @property
    def group_id(self) -> int:
        return self._group_id

    @property
    def fd(self) -> int:
        return self._fd

    def open(self) -> None:
        """Open the VFIO group device node."""
        _require_linux("VfioGroup.open")
        path = f"/dev/vfio/{self._group_id}"
        if not os.path.exists(path):
            raise DriverError(
                f"VFIO group {self._group_id} not found at {path}. "
                "Ensure the device is bound to vfio-pci and the group exists."
            )
        try:
            self._fd = os.open(path, os.O_RDWR)
        except OSError as exc:
            raise DriverError(f"Cannot open VFIO group {self._group_id}: {exc}") from exc

    def close(self) -> None:
        """Close the VFIO group file descriptor."""
        if self._fd >= 0:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = -1

    def is_viable(self) -> bool:
        """Return True if all devices in this group are bound to VFIO."""
        if self._fd < 0:
            raise DriverError("VfioGroup not open")
        try:
            import fcntl
            data = struct.pack("=II", 8, 0)   # argsz=8, flags=0
            result = fcntl.ioctl(self._fd, VFIO_GROUP_GET_STATUS, data)
            _, flags = struct.unpack("=II", result)
            return bool(flags & 1)  # VFIO_GROUP_FLAGS_VIABLE
        except OSError as exc:
            raise DriverError(f"VFIO_GROUP_GET_STATUS failed: {exc}") from exc

    def set_container(self, container: VfioContainer) -> None:
        """Associate this group with a container (must be done before get_device)."""
        if self._fd < 0:
            raise DriverError("VfioGroup not open")
        try:
            import fcntl
            fcntl.ioctl(self._fd, VFIO_GROUP_SET_CONTAINER, container.fd)
        except OSError as exc:
            raise DriverError(f"VFIO_GROUP_SET_CONTAINER failed: {exc}") from exc

    def unset_container(self) -> None:
        """Detach this group from its container."""
        if self._fd < 0:
            raise DriverError("VfioGroup not open")
        try:
            import fcntl
            fcntl.ioctl(self._fd, VFIO_GROUP_UNSET_CONTAINER, 0)
        except OSError as exc:
            raise DriverError(f"VFIO_GROUP_UNSET_CONTAINER failed: {exc}") from exc

    def get_device(self, pci_address: str) -> "VfioDevice":
        """Open and return a :class:`VfioDevice` by PCI address.

        Parameters
        ----------
        pci_address:
            DBSF notation, e.g. ``"0000:01:00.0"``.
        """
        if self._fd < 0:
            raise DriverError("VfioGroup not open")
        try:
            import fcntl
            name_bytes = pci_address.encode() + b"\x00"
            dev_fd = fcntl.ioctl(self._fd, VFIO_GROUP_GET_DEVICE_FD, name_bytes)
        except OSError as exc:
            raise DriverError(
                f"VFIO_GROUP_GET_DEVICE_FD({pci_address!r}) failed: {exc}"
            ) from exc
        if dev_fd <= 0:
            raise DriverError(f"No VFIO device file descriptor for {pci_address!r}")
        return VfioDevice(dev_fd, pci_address)

    def __enter__(self) -> "VfioGroup":
        self.open()
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()

    def __repr__(self) -> str:
        return f"VfioGroup(group_id={self._group_id}, fd={self._fd})"


class VfioDevice:
    """A VFIO device — provides mmappable regions and IRQ control.

    Instances are obtained from :meth:`VfioGroup.get_device`.

    Regions correspond to PCI BARs and ROM.  Use :meth:`get_region_info`
    to discover size and offset, then :meth:`mmap_region` to map a BAR.
    """

    def __init__(self, fd: int, address: str) -> None:
        self._fd = fd
        self._address = address
        self._mappings: List[mmap.mmap] = []

    @property
    def fd(self) -> int:
        return self._fd

    @property
    def address(self) -> str:
        return self._address

    def close(self) -> None:
        """Close the device and release all BAR mappings."""
        for m in self._mappings:
            try:
                m.close()
            except OSError:
                pass
        self._mappings.clear()
        if self._fd >= 0:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = -1

    def get_info(self) -> Dict[str, Any]:
        """Return basic device info (num_regions, num_irqs, flags)."""
        if self._fd < 0:
            raise DriverError("VfioDevice closed")
        try:
            import fcntl
            size = struct.calcsize(_VFIO_DEVICE_INFO_STRUCT)
            data = struct.pack(_VFIO_DEVICE_INFO_STRUCT, size, 0, 0, 0)
            result = fcntl.ioctl(self._fd, VFIO_DEVICE_GET_INFO, data)
            argsz, flags, num_regions, num_irqs = struct.unpack(_VFIO_DEVICE_INFO_STRUCT, result)
            return {"argsz": argsz, "flags": flags,
                    "num_regions": num_regions, "num_irqs": num_irqs}
        except OSError as exc:
            raise DriverError(f"VFIO_DEVICE_GET_INFO failed: {exc}") from exc

    def get_region_info(self, region_index: int) -> Dict[str, Any]:
        """Return info for a device region (PCI BAR or ROM).

        Parameters
        ----------
        region_index:
            Region index (0-5 = BARs 0-5; 6 = ROM; 8 = config space).

        Returns
        -------
        dict
            Keys: ``index``, ``flags``, ``size``, ``offset``,
            ``readable``, ``writable``, ``mappable``.
        """
        if self._fd < 0:
            raise DriverError("VfioDevice closed")
        try:
            import fcntl
            size = struct.calcsize(_VFIO_REGION_INFO_STRUCT)
            data = struct.pack(_VFIO_REGION_INFO_STRUCT, size, 0, region_index, 0, 0, 0)
            result = fcntl.ioctl(self._fd, VFIO_DEVICE_GET_REGION_INFO, data)
            argsz, flags, index, cap_offset, reg_size, offset = struct.unpack(
                _VFIO_REGION_INFO_STRUCT, result
            )
            return {
                "index": index,
                "flags": flags,
                "size": reg_size,
                "offset": offset,
                "cap_offset": cap_offset,
                "readable":  bool(flags & VFIO_REGION_INFO_FLAG_READ),
                "writable":  bool(flags & VFIO_REGION_INFO_FLAG_WRITE),
                "mappable":  bool(flags & VFIO_REGION_INFO_FLAG_MMAP),
            }
        except OSError as exc:
            raise DriverError(f"VFIO_DEVICE_GET_REGION_INFO({region_index}) failed: {exc}") from exc

    def mmap_region(self, region_index: int) -> mmap.mmap:
        """Memory-map a device BAR region and return the mmap object.

        The mapping is tracked and released on :meth:`close`.

        Parameters
        ----------
        region_index:
            Region index as returned by :meth:`get_region_info`.

        Returns
        -------
        mmap.mmap
            CPU-accessible mapping of the BAR.
        """
        info = self.get_region_info(region_index)
        if not info["mappable"]:
            raise DriverError(
                f"Region {region_index} of VFIO device {self._address!r} is not mappable"
            )
        if info["size"] == 0:
            raise DriverError(f"Region {region_index} has zero size")
        try:
            mapping = mmap.mmap(
                self._fd,
                info["size"],
                access=mmap.ACCESS_READ | mmap.ACCESS_WRITE,
                offset=info["offset"],
            )
        except OSError as exc:
            raise DriverError(
                f"mmap_region({region_index}) of {self._address!r} failed: {exc}"
            ) from exc
        self._mappings.append(mapping)
        return mapping

    def get_irq_info(self, irq_index: int) -> Dict[str, Any]:
        """Return interrupt info for the given IRQ index.

        Returns
        -------
        dict
            Keys: ``index``, ``flags``, ``count``.
        """
        if self._fd < 0:
            raise DriverError("VfioDevice closed")
        try:
            import fcntl
            size = struct.calcsize(_VFIO_IRQ_INFO_STRUCT)
            data = struct.pack(_VFIO_IRQ_INFO_STRUCT, size, 0, irq_index, 0)
            result = fcntl.ioctl(self._fd, VFIO_DEVICE_GET_IRQ_INFO, data)
            argsz, flags, index, count = struct.unpack(_VFIO_IRQ_INFO_STRUCT, result)
            return {"index": index, "flags": flags, "count": count}
        except OSError as exc:
            raise DriverError(f"VFIO_DEVICE_GET_IRQ_INFO({irq_index}) failed: {exc}") from exc

    def set_irqs(
        self,
        index: int,
        action: int = VFIO_IRQ_SET_ACTION_TRIGGER,
        data_type: int = VFIO_IRQ_SET_DATA_NONE,
        start: int = 0,
        count: int = 1,
    ) -> None:
        """Configure device interrupts.

        Parameters
        ----------
        index:
            IRQ index (0 = legacy/MSI; 1 = MSI; 2 = MSI-X).
        action:
            One of ``VFIO_IRQ_SET_ACTION_MASK`` / ``UNMASK`` / ``TRIGGER``.
        data_type:
            ``VFIO_IRQ_SET_DATA_NONE``, ``VFIO_IRQ_SET_DATA_BOOL``, or
            ``VFIO_IRQ_SET_DATA_EVENTFD``.
        start:
            First interrupt in range.
        count:
            Number of interrupts.
        """
        if self._fd < 0:
            raise DriverError("VfioDevice closed")
        try:
            import fcntl
            flags = action | data_type
            size = struct.calcsize(_VFIO_SET_IRQS_STRUCT)
            data = struct.pack(_VFIO_SET_IRQS_STRUCT, size, flags, index, start, count)
            fcntl.ioctl(self._fd, VFIO_DEVICE_SET_IRQS, data)
        except OSError as exc:
            raise DriverError(f"VFIO_DEVICE_SET_IRQS failed: {exc}") from exc

    def reset(self) -> None:
        """Issue a device reset via VFIO_DEVICE_RESET."""
        if self._fd < 0:
            raise DriverError("VfioDevice closed")
        try:
            import fcntl
            fcntl.ioctl(self._fd, VFIO_DEVICE_RESET, 0)
        except OSError as exc:
            raise DriverError(f"VFIO_DEVICE_RESET failed: {exc}") from exc

    def __enter__(self) -> "VfioDevice":
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()

    def __repr__(self) -> str:
        return f"VfioDevice({self._address!r}, fd={self._fd})"


def bind_vfio_pci(pci_address: str) -> None:
    """Bind a PCI device to the vfio-pci driver.

    This is a two-step process:
    1. Unbind from the current driver (if any).
    2. Write the PCI address to ``/sys/bus/pci/drivers/vfio-pci/bind``.

    Parameters
    ----------
    pci_address:
        DBSF notation, e.g. ``"0000:01:00.0"``.

    Raises
    ------
    DriverError
        If the vfio-pci driver is not loaded or the bind fails.
    """
    _require_linux("bind_vfio_pci")
    bind_path = Path("/sys/bus/pci/drivers/vfio-pci/bind")
    if not bind_path.exists():
        raise DriverError(
            "vfio-pci driver not loaded. Run: modprobe vfio-pci"
        )
    # Unbind from current driver if bound
    device_path = Path(f"/sys/bus/pci/devices/{pci_address}")
    driver_link = device_path / "driver"
    if driver_link.is_symlink():
        unbind_path = driver_link.resolve() / "unbind"
        try:
            unbind_path.write_text(pci_address)
        except OSError as exc:
            raise DriverError(
                f"Cannot unbind {pci_address!r} from current driver: {exc}"
            ) from exc
    # Bind to vfio-pci
    try:
        bind_path.write_text(pci_address)
    except OSError as exc:
        raise DriverError(f"Cannot bind {pci_address!r} to vfio-pci: {exc}") from exc


def unbind_vfio_pci(pci_address: str) -> None:
    """Unbind a PCI device from the vfio-pci driver.

    Parameters
    ----------
    pci_address:
        DBSF notation, e.g. ``"0000:01:00.0"``.

    Raises
    ------
    DriverError
        If the device is not bound to vfio-pci or the unbind fails.
    """
    _require_linux("unbind_vfio_pci")
    unbind_path = Path("/sys/bus/pci/drivers/vfio-pci/unbind")
    if not unbind_path.exists():
        raise DriverError("vfio-pci driver not loaded or no devices bound")
    try:
        unbind_path.write_text(pci_address)
    except OSError as exc:
        raise DriverError(
            f"Cannot unbind {pci_address!r} from vfio-pci: {exc}"
        ) from exc


def get_iommu_group(pci_address: str) -> Optional[int]:
    """Return the IOMMU group number for a PCI device, or None.

    Reads the symlink at
    ``/sys/bus/pci/devices/<addr>/iommu_group`` and returns its
    last path component as an integer.
    """
    _require_linux("get_iommu_group")
    link = Path(f"/sys/bus/pci/devices/{pci_address}/iommu_group")
    if not link.exists():
        return None
    try:
        target = os.readlink(str(link))
        return int(Path(target).name)
    except (OSError, ValueError):
        return None


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def _register_char_device_functions(runtime: Any) -> None:
    runtime.register_function(
        "open_char_device",
        lambda rt, path, flags=os.O_RDWR: _open_char_device(path, int(flags)),
    )
    runtime.register_function(
        "close_char_device",
        lambda rt, dev: dev.close(),
    )
    runtime.register_function(
        "read_char_device",
        lambda rt, dev, n: dev.read(int(n)),
    )
    runtime.register_function(
        "write_char_device",
        lambda rt, dev, data: dev.write(
            data if isinstance(data, (bytes, bytearray)) else str(data).encode()
        ),
    )
    runtime.register_function(
        "ioctl_char_device",
        lambda rt, dev, request, arg=0: dev.ioctl(int(request), int(arg)),
    )
    runtime.register_function(
        "char_device_is_open",
        lambda rt, dev: dev.is_open(),
    )


def _register_block_device_functions(runtime: Any) -> None:
    runtime.register_function(
        "open_block_device",
        lambda rt, path, read_only=True: _open_block_device(path, bool(read_only)),
    )
    runtime.register_function(
        "close_block_device",
        lambda rt, dev: dev.close(),
    )
    runtime.register_function(
        "read_sector",
        lambda rt, dev, lba: dev.read_sector(int(lba)),
    )
    runtime.register_function(
        "write_sector",
        lambda rt, dev, lba, data: dev.write_sector(int(lba), bytes(data)),
    )
    runtime.register_function(
        "block_device_size",
        lambda rt, dev: dev.get_size(),
    )
    runtime.register_function(
        "block_device_sector_size",
        lambda rt, dev: dev.get_logical_block_size(),
    )


def _register_pci_functions(runtime: Any) -> None:
    runtime.register_function(
        "enumerate_pci",
        lambda rt: [d.to_dict() for d in enumerate_pci_devices()],
    )
    runtime.register_function(
        "find_pci_device",
        lambda rt, vendor, device: _find_pci(int(vendor, 16) if isinstance(vendor, str) else int(vendor),
                                             int(device, 16) if isinstance(device, str) else int(device)),
    )
    runtime.register_function(
        "pci_device_info",
        lambda rt, dev: dev.to_dict() if isinstance(dev, PciDevice) else {},
    )


def _register_i2c_functions(runtime: Any) -> None:
    runtime.register_function(
        "open_i2c_device",
        lambda rt, bus, addr: _open_i2c(int(bus), int(addr)),
    )
    runtime.register_function(
        "close_i2c_device",
        lambda rt, dev: dev.close(),
    )
    runtime.register_function(
        "i2c_read",
        lambda rt, dev, n: dev.read(int(n)),
    )
    runtime.register_function(
        "i2c_write",
        lambda rt, dev, data: dev.write(
            data if isinstance(data, (bytes, bytearray)) else bytes([int(data)])
        ),
    )
    runtime.register_function(
        "i2c_write_byte",
        lambda rt, dev, reg, val: dev.write_byte_data(int(reg), int(val)),
    )
    runtime.register_function(
        "i2c_read_byte",
        lambda rt, dev, reg: dev.read_byte_data(int(reg)),
    )
    runtime.register_function(
        "i2c_read_word",
        lambda rt, dev, reg: dev.read_word_data(int(reg)),
    )


def _register_spi_functions(runtime: Any) -> None:
    runtime.register_function(
        "open_spi_device",
        lambda rt, bus, cs: _open_spi(int(bus), int(cs)),
    )
    runtime.register_function(
        "close_spi_device",
        lambda rt, dev: dev.close(),
    )
    runtime.register_function(
        "spi_transfer",
        lambda rt, dev, data, speed=500000: dev.transfer(
            data if isinstance(data, (bytes, bytearray)) else bytes(data),
            int(speed),
        ),
    )


def _register_gpio_functions(runtime: Any) -> None:
    runtime.register_function(
        "gpio_export",
        lambda rt, pin: _gpio_export(int(pin)),
    )
    runtime.register_function(
        "gpio_unexport",
        lambda rt, pin: GpioPin(int(pin)).unexport(),
    )
    runtime.register_function(
        "gpio_set_direction",
        lambda rt, pin, direction: _gpio_set_direction(int(pin), str(direction)),
    )
    runtime.register_function(
        "gpio_read",
        lambda rt, pin: GpioPin(int(pin)).read(),
    )
    runtime.register_function(
        "gpio_write",
        lambda rt, pin, value: _gpio_write(int(pin), int(value)),
    )
    runtime.register_function(
        "list_gpio_chips",
        lambda rt: list_gpio_chips(),
    )


def _register_irq_functions(runtime: Any) -> None:
    runtime.register_function(
        "list_irqs",
        lambda rt: list_irqs(),
    )
    runtime.register_function(
        "get_irq_affinity",
        lambda rt, irq: get_irq_affinity(int(irq)),
    )
    runtime.register_function(
        "set_irq_affinity",
        lambda rt, irq, mask: set_irq_affinity(int(irq), str(mask)),
    )


def _register_interrupt_handler_functions(runtime: Any) -> None:
    runtime.register_function(
        "create_interrupt_handler",
        lambda rt: InterruptHandler(),
    )
    runtime.register_function(
        "register_interrupt",
        lambda rt, handler, sig, cb: handler.register(int(sig), cb),
    )
    runtime.register_function(
        "activate_interrupts",
        lambda rt, handler: handler.activate(),
    )
    runtime.register_function(
        "deactivate_interrupts",
        lambda rt, handler: handler.deactivate(),
    )
    runtime.register_function(
        "trigger_interrupt",
        lambda rt, handler, sig: handler.trigger(int(sig)),
    )


def _register_device_tree_functions(runtime: Any) -> None:
    runtime.register_function(
        "read_device_tree_property",
        lambda rt, node, prop: read_device_tree_property(str(node), str(prop)),
    )
    runtime.register_function(
        "read_device_tree_string",
        lambda rt, node, prop: read_device_tree_string(str(node), str(prop)),
    )
    runtime.register_function(
        "list_device_tree_children",
        lambda rt, node: list_device_tree_children(str(node)),
    )


def _register_udev_sysfs_functions(runtime: Any) -> None:
    runtime.register_function(
        "list_devices_by_class",
        lambda rt, cls: list_devices_by_class(str(cls)),
    )
    runtime.register_function(
        "find_device_by_attr",
        lambda rt, cls, attr, val: find_device_by_attr(str(cls), str(attr), str(val)),
    )
    runtime.register_function(
        "get_device_attribute",
        lambda rt, cls, name, attr: get_device_attribute(str(cls), str(name), str(attr)),
    )


def _register_kernel_module_functions(runtime: Any) -> None:
    runtime.register_function(
        "load_kernel_module",
        lambda rt, name, params=None: load_kernel_module(str(name), params),
    )
    runtime.register_function(
        "unload_kernel_module",
        lambda rt, name, force=False: unload_kernel_module(str(name), bool(force)),
    )
    runtime.register_function(
        "is_module_loaded",
        lambda rt, name: is_module_loaded(str(name)),
    )
    runtime.register_function(
        "list_loaded_modules",
        lambda rt: list_loaded_modules(),
    )
    runtime.register_function(
        "get_module_info",
        lambda rt, name: get_module_info(str(name)),
    )
    runtime.register_function(
        "get_module_dependencies",
        lambda rt, name: get_module_dependencies(str(name)),
    )


def _register_usb_functions(runtime: Any) -> None:
    runtime.register_function(
        "enumerate_usb_devices",
        lambda rt, vid=None, pid=None: [
            d.to_dict() for d in enumerate_usb_devices(
                vendor_id=int(vid, 16) if isinstance(vid, str) else (int(vid) if vid is not None else None),
                product_id=int(pid, 16) if isinstance(pid, str) else (int(pid) if pid is not None else None),
            )
        ],
    )
    runtime.register_function(
        "find_usb_device",
        lambda rt, vid, pid: (
            lambda d: d.to_dict() if d else None
        )(find_usb_device(
            int(vid, 16) if isinstance(vid, str) else int(vid),
            int(pid, 16) if isinstance(pid, str) else int(pid),
        )),
    )
    runtime.register_function(
        "get_usb_device_info",
        lambda rt, dev: dev.to_dict() if isinstance(dev, UsbDevice) else {},
    )


def _register_network_device_functions(runtime: Any) -> None:
    runtime.register_function(
        "enumerate_net_devices",
        lambda rt: [d.to_dict() for d in enumerate_net_devices()],
    )
    runtime.register_function(
        "get_net_device",
        lambda rt, name: NetDevice(str(name)),
    )
    runtime.register_function(
        "net_device_info",
        lambda rt, dev: dev.to_dict() if isinstance(dev, NetDevice) else {},
    )
    runtime.register_function(
        "net_set_mtu",
        lambda rt, dev, mtu: dev.set_mtu(int(mtu)),
    )
    runtime.register_function(
        "net_bring_up",
        lambda rt, dev: dev.bring_up(),
    )
    runtime.register_function(
        "net_bring_down",
        lambda rt, dev: dev.bring_down(),
    )
    runtime.register_function(
        "create_raw_socket",
        lambda rt, iface: create_raw_socket(str(iface)),
    )
    runtime.register_function(
        "send_raw_packet",
        lambda rt, sock, pkt: send_raw_packet(
            sock, pkt if isinstance(pkt, (bytes, bytearray)) else bytes(pkt)
        ),
    )
    runtime.register_function(
        "receive_raw_packet",
        lambda rt, sock, max_len=65535: receive_raw_packet(sock, int(max_len)),
    )


def _register_dma_functions(runtime: Any) -> None:
    runtime.register_function(
        "dma_alloc",
        lambda rt, size, heap="system": DmaBuffer.allocate(int(size), str(heap)),
    )
    runtime.register_function(
        "dma_map",
        lambda rt, buf: buf.map(),
    )
    runtime.register_function(
        "dma_unmap",
        lambda rt, buf: buf.unmap(),
    )
    runtime.register_function(
        "dma_free",
        lambda rt, buf: buf.close(),
    )
    runtime.register_function(
        "dma_buffer_fd",
        lambda rt, buf: buf.fd,
    )
    runtime.register_function(
        "dma_buffer_size",
        lambda rt, buf: buf.size,
    )
    runtime.register_function(
        "list_dma_heaps",
        lambda rt: list_dma_heaps(),
    )


def _register_vfio_functions(runtime: Any) -> None:
    runtime.register_function(
        "vfio_open_container",
        lambda rt: (lambda c: c.open() or c)(VfioContainer()),
    )
    runtime.register_function(
        "vfio_close_container",
        lambda rt, c: c.close(),
    )
    runtime.register_function(
        "vfio_set_iommu",
        lambda rt, c, iommu_type=VFIO_TYPE1v2_IOMMU: c.set_iommu(int(iommu_type)),
    )
    runtime.register_function(
        "vfio_map_dma",
        lambda rt, c, vaddr, iova, size, flags=0: c.map_dma(int(vaddr), int(iova), int(size), int(flags)),
    )
    runtime.register_function(
        "vfio_unmap_dma",
        lambda rt, c, iova, size: c.unmap_dma(int(iova), int(size)),
    )
    runtime.register_function(
        "vfio_open_group",
        lambda rt, group_id: (lambda g: g.open() or g)(VfioGroup(int(group_id))),
    )
    runtime.register_function(
        "vfio_close_group",
        lambda rt, g: g.close(),
    )
    runtime.register_function(
        "vfio_group_set_container",
        lambda rt, g, c: g.set_container(c),
    )
    runtime.register_function(
        "vfio_group_is_viable",
        lambda rt, g: g.is_viable(),
    )
    runtime.register_function(
        "vfio_get_device",
        lambda rt, g, pci_addr: g.get_device(str(pci_addr)),
    )
    runtime.register_function(
        "vfio_device_info",
        lambda rt, dev: dev.get_info(),
    )
    runtime.register_function(
        "vfio_region_info",
        lambda rt, dev, region: dev.get_region_info(int(region)),
    )
    runtime.register_function(
        "vfio_mmap_region",
        lambda rt, dev, region: dev.mmap_region(int(region)),
    )
    runtime.register_function(
        "vfio_irq_info",
        lambda rt, dev, irq_index: dev.get_irq_info(int(irq_index)),
    )
    runtime.register_function(
        "vfio_set_irqs",
        lambda rt, dev, index, action=VFIO_IRQ_SET_ACTION_TRIGGER,
               data_type=VFIO_IRQ_SET_DATA_NONE, start=0, count=1:
            dev.set_irqs(int(index), int(action), int(data_type), int(start), int(count)),
    )
    runtime.register_function(
        "vfio_device_reset",
        lambda rt, dev: dev.reset(),
    )
    runtime.register_function(
        "vfio_close_device",
        lambda rt, dev: dev.close(),
    )
    runtime.register_function(
        "bind_vfio_pci",
        lambda rt, pci_addr: bind_vfio_pci(str(pci_addr)),
    )
    runtime.register_function(
        "unbind_vfio_pci",
        lambda rt, pci_addr: unbind_vfio_pci(str(pci_addr)),
    )
    runtime.register_function(
        "get_iommu_group",
        lambda rt, pci_addr: get_iommu_group(str(pci_addr)),
    )

def register_driver_functions(runtime: Any) -> None:
    """Register all driver framework functions with the NexusLang runtime."""
    _register_char_device_functions(runtime)
    _register_block_device_functions(runtime)
    _register_pci_functions(runtime)
    _register_i2c_functions(runtime)
    _register_spi_functions(runtime)
    _register_gpio_functions(runtime)
    _register_irq_functions(runtime)
    _register_interrupt_handler_functions(runtime)
    _register_device_tree_functions(runtime)
    _register_udev_sysfs_functions(runtime)
    _register_kernel_module_functions(runtime)
    _register_usb_functions(runtime)
    _register_network_device_functions(runtime)
    _register_dma_functions(runtime)
    _register_vfio_functions(runtime)


# ---------------------------------------------------------------------------
# Private helpers used by lambda registrations
# ---------------------------------------------------------------------------

def _open_char_device(path: str, flags: int) -> CharDevice:
    dev = CharDevice(path, flags)
    dev.open()
    return dev


def _open_block_device(path: str, read_only: bool) -> BlockDevice:
    dev = BlockDevice(path)
    dev.open(read_only=read_only)
    return dev


def _open_i2c(bus: int, addr: int) -> I2cDevice:
    dev = I2cDevice(bus, addr)
    dev.open()
    return dev


def _open_spi(bus: int, cs: int) -> SpiDevice:
    dev = SpiDevice(bus, cs)
    dev.open()
    return dev


def _find_pci(
    vendor_id: int,
    device_id: int,
) -> Optional[Dict[str, Any]]:
    try:
        devices = enumerate_pci_devices(vendor_id=vendor_id, device_id=device_id)
        return devices[0].to_dict() if devices else None
    except DriverError:
        return None


def _gpio_export(pin: int) -> None:
    GpioPin(pin).export()


def _gpio_set_direction(pin: int, direction: str) -> None:
    GpioPin(pin).set_direction(direction)


def _gpio_write(pin: int, value: int) -> None:
    GpioPin(pin).write(value)
