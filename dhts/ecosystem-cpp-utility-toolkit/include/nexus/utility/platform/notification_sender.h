#pragma once

#include <string>

namespace nexus::utility::platform {

/**
 * @brief Cross-platform notification sender.
 */
class NotificationSender {
public:
    enum class Priority {
        Low,
        Normal,
        High,
        Critical
    };

    struct Notification {
        std::string title;
        std::string message;
        std::string icon;
        Priority priority = Priority::Normal;
        int timeout = 5000; // milliseconds
    };

    /**
     * @brief Sends notification.
     */
    static bool send(const Notification& notification) {
#ifdef _WIN32
        // Windows: Use Windows Toast Notifications or balloon tips
        return sendWindows(notification);
#elif __linux__
        // Linux: Use libnotify or D-Bus
        return sendLinux(notification);
#elif __APPLE__
        // macOS: Use NSUserNotificationCenter
        return sendMacOS(notification);
#else
        return false;
#endif
    }

    /**
     * @brief Sends simple notification.
     */
    static bool sendSimple(const std::string& title, const std::string& message) {
        Notification notif;
        notif.title = title;
        notif.message = message;
        return send(notif);
    }

private:
    static bool sendWindows(const Notification& notif) {
        // Implementation would use Windows API
        return false;
    }

    static bool sendLinux(const Notification& notif) {
        // Implementation would use libnotify or D-Bus
        // Example: notify-send command or D-Bus org.freedesktop.Notifications
        return false;
    }

    static bool sendMacOS(const Notification& notif) {
        // Implementation would use Objective-C NSUserNotificationCenter
        return false;
    }
};

} // namespace nexus::utility::platform
