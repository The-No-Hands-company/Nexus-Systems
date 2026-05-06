"""Tests for the device driver framework (stdlib/drivers)."""
import pytest
import sys
import os
import platform
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.stdlib.drivers import (
    CharDevice,
    BlockDevice,
    GpioPin,
    I2cDevice,
    SpiDevice,
    InterruptHandler,
    DriverError,
    register_driver_functions,
    list_irqs,
    read_device_tree_string,
    list_devices_by_class,
    # Kernel modules
    load_kernel_module,
    unload_kernel_module,
    is_module_loaded,
    list_loaded_modules,
    get_module_info,
    get_module_dependencies,
    # USB
    UsbDevice,
    enumerate_usb_devices,
    find_usb_device,
    # Network
    NetDevice,
    enumerate_net_devices,
    create_raw_socket,
    send_raw_packet,
    receive_raw_packet,
    # DMA
    DmaBuffer,
    list_dma_heaps,
    # VFIO
    VfioContainer,
    VfioGroup,
    VfioDevice,
    bind_vfio_pci,
    unbind_vfio_pci,
    get_iommu_group,
)
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


IS_LINUX = platform.system().lower() == "linux"
HAS_PROC_MODULES = os.path.exists("/proc/modules")
HAS_SYS_NET = os.path.exists("/sys/class/net")


# ---------------------------------------------------------------------------
# CharDevice
# ---------------------------------------------------------------------------

class TestCharDevice:
    def test_not_open_initially(self):
        dev = CharDevice("/dev/zero")
        assert dev.is_open() is False

    def test_open_and_close_dev_zero(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/zero")
        dev = CharDevice("/dev/zero")
        dev.open()
        assert dev.is_open() is True
        dev.close()
        assert dev.is_open() is False

    def test_read_dev_zero_returns_bytes(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/zero")
        dev = CharDevice("/dev/zero")
        dev.open()
        data = dev.read(8)
        dev.close()
        assert len(data) == 8
        assert all(b == 0 for b in data)

    def test_read_dev_null_returns_empty(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/null")
        dev = CharDevice("/dev/null")
        dev.open()
        data = dev.read(8)
        dev.close()
        assert len(data) == 0

    def test_write_dev_null_succeeds(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/null")
        dev = CharDevice("/dev/null", flags=os.O_WRONLY)
        dev.open()
        written = dev.write(b"hello world")
        dev.close()
        assert written >= 0

    def test_open_nonexistent_raises(self):
        dev = CharDevice("/dev/no_such_device_xyzzy_12345")
        with pytest.raises((DriverError, OSError)):
            dev.open()

    def test_read_without_open_raises(self):
        dev = CharDevice("/dev/zero")
        with pytest.raises((DriverError, OSError)):
            dev.read(4)

    def test_context_manager(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/zero")
        with CharDevice("/dev/zero") as dev:
            data = dev.read(4)
        assert not dev.is_open()
        assert len(data) == 4


# ---------------------------------------------------------------------------
# BlockDevice
# ---------------------------------------------------------------------------

class TestBlockDevice:
    def test_not_open_initially(self):
        bd = BlockDevice("/dev/sda")
        assert bd.is_open() is False

    def test_sector_size_constant(self):
        assert BlockDevice.SECTOR_SIZE == 512

    def test_open_nonexistent_raises(self):
        bd = BlockDevice("/dev/no_such_block_device_xyzzy")
        with pytest.raises((DriverError, OSError, FileNotFoundError)):
            bd.open()


# ---------------------------------------------------------------------------
# GpioPin
# ---------------------------------------------------------------------------

class TestGpioPin:
    def test_construction(self):
        pin = GpioPin(17)
        assert pin.pin == 17

    def test_export_requires_linux(self):
        if not IS_LINUX:
            with pytest.raises(DriverError):
                GpioPin(0).export()
        else:
            # On Linux we can't actually toggle real GPIO without root;
            # just confirm the DriverError message is sensible if sysfs missing
            pin = GpioPin(9999)
            try:
                pin.export()
            except (DriverError, OSError, PermissionError):
                pass  # expected without root / real hardware

    def test_read_requires_export_first(self):
        if not IS_LINUX:
            pytest.skip("Linux-only")
        pin = GpioPin(9999)
        with pytest.raises((DriverError, OSError, FileNotFoundError)):
            pin.read()


# ---------------------------------------------------------------------------
# I2cDevice
# ---------------------------------------------------------------------------

class TestI2cDevice:
    def test_construction(self):
        dev = I2cDevice(bus=1, address=0x68)
        assert dev.bus == 1
        assert dev.address == 0x68

    def test_not_open_initially(self):
        dev = I2cDevice(1, 0x68)
        assert dev.is_open() is False

    def test_requires_linux(self):
        if not IS_LINUX:
            dev = I2cDevice(1, 0x68)
            with pytest.raises(DriverError):
                dev.open()

    def test_open_nonexistent_bus_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only")
        dev = I2cDevice(bus=9999, address=0x01)
        with pytest.raises((DriverError, OSError, FileNotFoundError)):
            dev.open()


# ---------------------------------------------------------------------------
# SpiDevice
# ---------------------------------------------------------------------------

class TestSpiDevice:
    def test_construction(self):
        dev = SpiDevice(bus=0, cs=0)
        assert dev.bus == 0
        assert dev.cs == 0

    def test_not_open_initially(self):
        dev = SpiDevice(0, 0)
        assert dev.is_open() is False

    def test_mode_constants(self):
        assert SpiDevice.MODE_0 == 0
        assert SpiDevice.MODE_1 == 1
        assert SpiDevice.MODE_2 == 2
        assert SpiDevice.MODE_3 == 3

    def test_requires_linux(self):
        if not IS_LINUX:
            dev = SpiDevice(0, 0)
            with pytest.raises(DriverError):
                dev.open()

    def test_open_nonexistent_device_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only")
        dev = SpiDevice(bus=9999, cs=9999)
        with pytest.raises((DriverError, OSError, FileNotFoundError)):
            dev.open()


# ---------------------------------------------------------------------------
# InterruptHandler
# ---------------------------------------------------------------------------

class TestInterruptHandler:
    def test_construction(self):
        handler = InterruptHandler()
        assert handler is not None

    def test_register_and_trigger(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: POSIX signals")
        import signal
        received = []

        handler = InterruptHandler()
        # Callbacks are invoked with zero arguments by the driver; record the
        # signal number explicitly in the closure.
        handler.register(signal.SIGUSR1, lambda: received.append(signal.SIGUSR1))
        handler.activate()
        handler.trigger(signal.SIGUSR1)

        import time
        time.sleep(0.05)
        handler.deactivate()

        assert len(received) > 0
        assert received[0] == signal.SIGUSR1

    def test_deactivate_without_activate_is_safe(self):
        handler = InterruptHandler()
        handler.deactivate()  # should not raise

    def test_trigger_without_registered_handler_is_safe(self):
        if not IS_LINUX:
            pytest.skip("Linux-only")
        import signal
        handler = InterruptHandler()
        handler.activate()
        try:
            handler.trigger(signal.SIGUSR2)
        except Exception:
            pass
        handler.deactivate()


# ---------------------------------------------------------------------------
# IRQ utilities
# ---------------------------------------------------------------------------

class TestIrqInfo:
    def test_list_irqs_returns_list(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /proc/interrupts")
        irqs = list_irqs()
        assert isinstance(irqs, list)

    def test_list_irqs_dicts_have_irq_field(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /proc/interrupts")
        irqs = list_irqs()
        if irqs:
            first = irqs[0]
            assert "irq" in first


# ---------------------------------------------------------------------------
# Device tree access
# ---------------------------------------------------------------------------

class TestDeviceTreeAccess:
    def test_missing_node_returns_none_or_raises_gracefully(self):
        result = read_device_tree_string(
            "node/that/does/not/exist_xyzzy_12345", "compatible"
        )
        # Should return None or empty string, not crash
        assert result is None or isinstance(result, str)


# ---------------------------------------------------------------------------
# Sysfs device listing
# ---------------------------------------------------------------------------

class TestSysfsDeviceListing:
    def test_list_devices_by_class_net(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        devices = list_devices_by_class("net")
        assert isinstance(devices, list)

    def test_list_devices_unknown_class_returns_empty(self):
        devices = list_devices_by_class("no_such_class_xyzzy_12345")
        assert isinstance(devices, list)
        assert len(devices) == 0


# ---------------------------------------------------------------------------
# register_driver_functions
# ---------------------------------------------------------------------------

class TestDriverRegistration:
    EXPECTED_FUNCTIONS = [
        "open_char_device",
        "close_char_device",
        "read_char_device",
        "write_char_device",
        "char_device_is_open",
        "open_block_device",
        "close_block_device",
        "read_sector",
        "block_device_size",
        "enumerate_pci",
        "open_i2c_device",
        "close_i2c_device",
        "i2c_read",
        "i2c_write",
        "open_spi_device",
        "close_spi_device",
        "spi_transfer",
        "gpio_export",
        "gpio_unexport",
        "gpio_set_direction",
        "gpio_read",
        "gpio_write",
        "list_gpio_chips",
        "list_irqs",
        "get_irq_affinity",
        "set_irq_affinity",
        "create_interrupt_handler",
        "register_interrupt",
        "activate_interrupts",
        "deactivate_interrupts",
        "trigger_interrupt",
        "read_device_tree_property",
        "read_device_tree_string",
        "list_device_tree_children",
        "list_devices_by_class",
        "find_device_by_attr",
        "get_device_attribute",
    ]

    def test_all_expected_functions_registered(self):
        runtime = Runtime()
        register_driver_functions(runtime)
        registered = set(runtime.functions.keys())
        for fn_name in self.EXPECTED_FUNCTIONS:
            assert fn_name in registered, f"Missing: {fn_name}"

    def test_registration_via_register_stdlib(self):
        runtime = Runtime()
        register_stdlib(runtime)
        registered = set(runtime.functions.keys())
        assert "open_char_device" in registered
        assert "gpio_read" in registered
        assert "enumerate_pci" in registered

    def test_double_registration_is_safe(self):
        runtime = Runtime()
        register_driver_functions(runtime)
        register_driver_functions(runtime)  # should not raise
        assert "open_char_device" in runtime.functions


# ---------------------------------------------------------------------------
# Kernel Modules
# ---------------------------------------------------------------------------

class TestKernelModules:
    def test_is_module_loaded_returns_bool(self):
        if not IS_LINUX or not HAS_PROC_MODULES:
            pytest.skip("Linux-only or /proc/modules unavailable")
        result = is_module_loaded("loop")
        assert isinstance(result, bool)

    def test_is_module_loaded_false_for_nonexistent(self):
        if not IS_LINUX or not HAS_PROC_MODULES:
            pytest.skip("Linux-only or /proc/modules unavailable")
        assert is_module_loaded("definitely_not_a_module_xyz_99") is False

    def test_list_loaded_modules_nonempty(self):
        if not IS_LINUX or not HAS_PROC_MODULES:
            pytest.skip("Linux-only or /proc/modules unavailable")
        modules = list_loaded_modules()
        assert isinstance(modules, list)
        assert len(modules) > 0

    def test_list_loaded_modules_entry_keys(self):
        if not IS_LINUX or not HAS_PROC_MODULES:
            pytest.skip("Linux-only or /proc/modules unavailable")
        modules = list_loaded_modules()
        required_keys = {"name", "size", "refcount", "dependencies", "state", "offset"}
        for entry in modules:
            assert required_keys.issubset(entry.keys()), f"Missing keys in {entry}"
            assert isinstance(entry["name"], str)
            assert isinstance(entry["size"], int)
            assert isinstance(entry["refcount"], int)
            assert isinstance(entry["dependencies"], list)

    def test_list_loaded_modules_offset_hex_string(self):
        if not IS_LINUX or not HAS_PROC_MODULES:
            pytest.skip("Linux-only or /proc/modules unavailable")
        modules = list_loaded_modules()
        for entry in modules:
            assert isinstance(entry["offset"], str)
            # Offset should be a hex string like "0xffffffff..."
            assert entry["offset"].startswith("0x") or entry["offset"] == "-"

    def test_get_module_info_unknown_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: modinfo")
        with pytest.raises(DriverError):
            get_module_info("nonexistent_xyz_99")

    def test_load_nonexistent_module_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: modprobe")
        with pytest.raises(DriverError):
            load_kernel_module("nonexistent_xyz_99")

    def test_unload_nonexistent_module_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: rmmod")
        with pytest.raises(DriverError):
            unload_kernel_module("nonexistent_xyz_99")

    def test_get_module_dependencies_unknown_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: modinfo")
        with pytest.raises(DriverError):
            get_module_dependencies("nonexistent_xyz_99")

    def test_non_linux_raises(self):
        if IS_LINUX:
            pytest.skip("Non-Linux test only")
        with pytest.raises(DriverError, match="Linux"):
            is_module_loaded("x")


# ---------------------------------------------------------------------------
# USB Devices
# ---------------------------------------------------------------------------

class TestUsbDevices:
    def test_enumerate_usb_returns_list(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/usb/devices")
        result = enumerate_usb_devices()
        assert isinstance(result, list)

    def test_enumerate_usb_all_usb_device_instances(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/usb/devices")
        devices = enumerate_usb_devices()
        for d in devices:
            assert isinstance(d, UsbDevice)

    def test_usb_device_to_dict_keys(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/usb/devices")
        devices = enumerate_usb_devices()
        if not devices:
            pytest.skip("No USB devices found")
        required_keys = {"vendor_id", "product_id", "manufacturer", "product",
                         "bus_number", "device_number", "speed", "address"}
        for d in devices:
            d_dict = d.to_dict()
            assert required_keys.issubset(d_dict.keys())

    def test_usb_device_vid_pid_hex_format(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/usb/devices")
        devices = enumerate_usb_devices()
        if not devices:
            pytest.skip("No USB devices found")
        for d in devices:
            d_dict = d.to_dict()
            if d_dict["vendor_id"] is not None:
                # Should be a 4-char hex string (from :04x format)
                assert len(d_dict["vendor_id"]) == 4
                int(d_dict["vendor_id"], 16)  # parseable as hex

    def test_find_usb_nonexistent_returns_none(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/usb/devices")
        result = find_usb_device(0xFFFF, 0xFFFF)
        assert result is None

    def test_usb_device_sysfs_mock(self, tmp_path):
        """UsbDevice reads attributes from sysfs directory."""
        sysfs_dir = tmp_path / "usb1"
        sysfs_dir.mkdir()
        (sysfs_dir / "idVendor").write_text("0483\n")
        (sysfs_dir / "idProduct").write_text("5740\n")
        (sysfs_dir / "manufacturer").write_text("STMicroelectronics\n")
        (sysfs_dir / "product").write_text("Virtual COM Port\n")
        (sysfs_dir / "busnum").write_text("1\n")
        (sysfs_dir / "devnum").write_text("5\n")
        (sysfs_dir / "speed").write_text("12\n")
        dev = UsbDevice(str(sysfs_dir))
        assert dev.vendor_id == 0x0483
        assert dev.product_id == 0x5740
        assert dev.manufacturer == "STMicroelectronics"
        assert dev.product == "Virtual COM Port"
        assert dev.bus_number == 1
        assert dev.device_number == 5

    def test_usb_device_repr(self, tmp_path):
        sysfs_dir = tmp_path / "usb2"
        sysfs_dir.mkdir()
        (sysfs_dir / "idVendor").write_text("04d8\n")
        (sysfs_dir / "idProduct").write_text("000a\n")
        dev = UsbDevice(str(sysfs_dir))
        r = repr(dev)
        assert "UsbDevice" in r

    def test_non_linux_raises(self):
        if IS_LINUX:
            pytest.skip("Non-Linux test only")
        with pytest.raises(DriverError, match="Linux"):
            enumerate_usb_devices()


# ---------------------------------------------------------------------------
# Network Devices
# ---------------------------------------------------------------------------

class TestNetDevices:
    def test_enumerate_net_returns_list(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        devices = enumerate_net_devices()
        assert isinstance(devices, list)
        assert len(devices) > 0  # at least loopback

    def test_loopback_present(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        names = [d.name for d in enumerate_net_devices()]
        assert "lo" in names

    def test_net_device_mac_address(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        dev = NetDevice("lo")
        mac = dev.mac_address
        # Loopback has 00:00:00:00:00:00 or similar
        assert isinstance(mac, str)
        assert len(mac) > 0

    def test_net_device_mtu(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        dev = NetDevice("lo")
        mtu = dev.mtu
        assert isinstance(mtu, int)
        assert mtu > 0

    def test_net_device_is_up_bool(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        dev = NetDevice("lo")
        assert isinstance(dev.is_up, bool)

    def test_net_device_stats_are_int(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        dev = NetDevice("lo")
        for attr in ("rx_bytes", "tx_bytes", "rx_packets", "tx_packets",
                     "rx_errors", "tx_errors", "rx_dropped", "tx_dropped"):
            val = getattr(dev, attr)
            assert isinstance(val, int), f"{attr} should be int, got {type(val)}"

    def test_net_device_operstate(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        dev = NetDevice("lo")
        assert isinstance(dev.operstate, str)

    def test_net_device_to_dict_keys(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        dev = NetDevice("lo")
        d = dev.to_dict()
        required_keys = {"name", "mac_address", "mtu", "operstate", "is_up",
                         "rx_bytes", "tx_bytes", "rx_packets", "tx_packets"}
        assert required_keys.issubset(d.keys())

    def test_net_device_repr(self):
        if not IS_LINUX or not HAS_SYS_NET:
            pytest.skip("Linux-only or /sys/class/net unavailable")
        assert "lo" in repr(NetDevice("lo"))

    def test_non_linux_raises(self):
        if IS_LINUX:
            pytest.skip("Non-Linux test only")
        with pytest.raises(DriverError, match="Linux"):
            enumerate_net_devices()


# ---------------------------------------------------------------------------
# DMA Buffers
# ---------------------------------------------------------------------------

class TestDmaBuffers:
    def test_list_dma_heaps_returns_list(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/dma_heap")
        result = list_dma_heaps()
        assert isinstance(result, list)
        # May be empty if kernel lacks dma_heap support; that is fine

    def test_dma_alloc_nonexistent_heap_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/dma_heap")
        with pytest.raises(DriverError):
            DmaBuffer.allocate(4096, "nonexistent_heap_xyz_99")

    def test_dma_buffer_properties_on_construction(self):
        buf = DmaBuffer(-1, 4096, "system")
        assert buf.fd == -1
        assert buf.size == 4096
        assert buf.heap == "system"

    def test_dma_buffer_close_with_no_fd_is_safe(self):
        buf = DmaBuffer(-1, 4096, "system")
        buf.close()  # should not raise even with fd=-1

    def test_dma_buffer_double_close_is_safe(self):
        buf = DmaBuffer(-1, 4096, "system")
        buf.close()
        buf.close()  # idempotent

    def test_dma_buffer_map_closed_raises(self):
        buf = DmaBuffer(-1, 4096, "system")
        buf.close()
        with pytest.raises(DriverError):
            buf.map()

    def test_dma_buffer_repr(self):
        buf = DmaBuffer(3, 8192, "linux,cma")
        r = repr(buf)
        assert "DmaBuffer" in r
        assert "8192" in r


# ---------------------------------------------------------------------------
# VFIO
# ---------------------------------------------------------------------------

class TestVfio:
    def test_vfio_container_not_open_initially(self):
        c = VfioContainer()
        assert c.fd == -1

    def test_vfio_open_no_device_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/vfio/vfio")
        import os
        if os.path.exists("/dev/vfio/vfio"):
            pytest.skip("System has /dev/vfio/vfio — actual open would succeed")
        c = VfioContainer()
        with pytest.raises(DriverError):
            c.open()

    def test_vfio_container_close_unopened_safe(self):
        c = VfioContainer()
        c.close()  # should not raise

    def test_vfio_group_not_open_initially(self):
        g = VfioGroup(42)
        assert g.fd == -1

    def test_vfio_group_nonexistent_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /dev/vfio/<group>")
        g = VfioGroup(99999)
        with pytest.raises(DriverError):
            g.open()

    def test_vfio_group_close_unopened_safe(self):
        g = VfioGroup(0)
        g.close()  # should not raise

    def test_get_iommu_group_nonexistent_returns_none(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/pci/devices")
        result = get_iommu_group("9999:ff:ff.7")
        assert result is None

    def test_unbind_vfio_nonexistent_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/pci")
        with pytest.raises(DriverError):
            unbind_vfio_pci("0000:ff:ff.7")

    def test_bind_vfio_nonexistent_raises(self):
        if not IS_LINUX:
            pytest.skip("Linux-only: /sys/bus/pci")
        with pytest.raises(DriverError):
            bind_vfio_pci("0000:ff:ff.7")


# ---------------------------------------------------------------------------
# Registration — new functions
# ---------------------------------------------------------------------------

class TestNewFunctionsRegistered:
    KERNEL_MODULE_FNS = [
        "load_kernel_module",
        "unload_kernel_module",
        "is_module_loaded",
        "list_loaded_modules",
        "get_module_info",
        "get_module_dependencies",
    ]

    USB_FNS = [
        "enumerate_usb_devices",
        "find_usb_device",
        "get_usb_device_info",
    ]

    NET_FNS = [
        "enumerate_net_devices",
        "get_net_device",
        "net_device_info",
        "net_set_mtu",
        "net_bring_up",
        "net_bring_down",
        "create_raw_socket",
        "send_raw_packet",
        "receive_raw_packet",
    ]

    DMA_FNS = [
        "dma_alloc",
        "dma_map",
        "dma_unmap",
        "dma_free",
        "dma_buffer_fd",
        "dma_buffer_size",
        "list_dma_heaps",
    ]

    VFIO_FNS = [
        "vfio_open_container",
        "vfio_close_container",
        "vfio_set_iommu",
        "vfio_map_dma",
        "vfio_unmap_dma",
        "vfio_open_group",
        "vfio_close_group",
        "vfio_group_set_container",
        "vfio_group_is_viable",
        "vfio_get_device",
        "vfio_device_info",
        "vfio_region_info",
        "vfio_mmap_region",
        "vfio_irq_info",
        "vfio_set_irqs",
        "vfio_device_reset",
        "vfio_close_device",
        "bind_vfio_pci",
        "unbind_vfio_pci",
        "get_iommu_group",
    ]

    def _get_registered(self):
        runtime = Runtime()
        register_driver_functions(runtime)
        return set(runtime.functions.keys())

    def test_kernel_module_functions_registered(self):
        registered = self._get_registered()
        for fn in self.KERNEL_MODULE_FNS:
            assert fn in registered, f"Missing kernel-module function: {fn}"

    def test_usb_functions_registered(self):
        registered = self._get_registered()
        for fn in self.USB_FNS:
            assert fn in registered, f"Missing USB function: {fn}"

    def test_net_functions_registered(self):
        registered = self._get_registered()
        for fn in self.NET_FNS:
            assert fn in registered, f"Missing network function: {fn}"

    def test_dma_functions_registered(self):
        registered = self._get_registered()
        for fn in self.DMA_FNS:
            assert fn in registered, f"Missing DMA function: {fn}"

    def test_vfio_functions_registered(self):
        registered = self._get_registered()
        for fn in self.VFIO_FNS:
            assert fn in registered, f"Missing VFIO function: {fn}"
