#ifndef DAWTHEME_H
#define DAWTHEME_H

#include <QObject>
#include <QColor>
#include <QPalette>

namespace daq {
namespace editor {

class DAWTheme : public QObject
{
    Q_OBJECT
public:
    explicit DAWTheme(QObject* parent = nullptr);
    ~DAWTheme();

    // Theme colors
    QColor backgroundColor() const;
    QColor foregroundColor() const;
    QColor accentColor() const;
    QColor highlightColor() const;
    QColor shadowColor() const;

    // Apply theme to application
    void applyTheme(QApplication* app) const;

private:
    QColor m_backgroundColor;
    QColor m_foregroundColor;
    QColor m_accentColor;
    QColor m_highlightColor;
    QColor m_shadowColor;
};

} // namespace editor
} // namespace daq

#endif // DAWTHEME_H