#pragma once

#include <string>

namespace nexus::utility::platform {

/**
 * @brief Cross-platform power management utilities.
 */
class PowerManagement {
public:
    enum class PowerState {
        OnBattery,
        OnAC,
        Unknown
    };

    enum class BatteryStatus {
        Charging,
        Discharging,
        Full,
        NotPresent,
        Unknown
    };

    struct PowerInfo {
        PowerState state;
        BatteryStatus batteryStatus;
        int batteryPercent; // 0-100, -1 if unknown
        int batteryTimeRemaining; // seconds, -1 if unknown
    };

    /**
     * @brief Gets current power information.
     */
    static PowerInfo getPowerInfo() {
        PowerInfo info;
        info.state = PowerState::Unknown;
        info.batteryStatus = BatteryStatus::Unknown;
        info.batteryPercent = -1;
        info.batteryTimeRemaining = -1;

#ifdef _WIN32
        // Windows: Use GetSystemPowerStatus
#elif __linux__
        // Linux: Read from /sys/class/power_supply/
#elif __APPLE__
        // macOS: Use IOKit IOPSCopyPowerSourcesInfo
#endif

        return info;
    }

    /**
     * @brief Checks if on battery power.
     */
    static bool isOnBattery() {
        return getPowerInfo().state == PowerState::OnBattery;
    }

    /**
     * @brief Gets battery percentage.
     */
    static int getBatteryPercent() {
        return getPowerInfo().batteryPercent;
    }

    /**
     * @brief Prevents system sleep.
     */
    static bool preventSleep(bool prevent) {
#ifdef _WIN32
        // Windows: SetThreadExecutionState
        return false;
#elif __linux__
        // Linux: Use systemd-inhibit or D-Bus
        return false;
#elif __APPLE__
        // macOS: IOPMAssertionCreateWithName
        return false;
#else
        return false;
#endif
    }

    /**
     * @brief Requests system sleep/shutdown.
     */
    static bool requestShutdown(bool reboot = false) {
#ifdef _WIN32
        // Windows: InitiateSystemShutdown
        return false;
#elif __linux__
        // Linux: systemctl poweroff/reboot
        return false;
#elif __APPLE__
        // macOS: AppleScript or IOKit
        return false;
#else
        return false;
#endif
    }
};

} // namespace nexus::utility::platform
