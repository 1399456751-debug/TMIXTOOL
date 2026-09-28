#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQuickWindow>

// Keeps the window above other applications, which is how this tool is
// actually used: parked on top of a DAW while the mix plays.
//
// The flag is applied through the window manager rather than with Qt's own
// Qt::WindowStaysOnTopHint, because on Windows that hint is honoured by
// destroying and recreating the native window - the position jumps and the
// window flickers on every toggle.
//
// The preference lives in an ini file beside the executable, not in the
// registry: this ships as a portable build, and a tool that writes to the
// registry stops being portable.
class WindowPin : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY (QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY (bool pinned READ pinned WRITE setPinned NOTIFY pinnedChanged)

public:
    explicit WindowPin (QObject* parent = nullptr);

    QQuickWindow* window() const { return window_; }
    void setWindow (QQuickWindow* window);

    bool pinned() const { return pinned_; }
    void setPinned (bool pinned);

private:
    void apply();

    QQuickWindow* window_ = nullptr;
    bool pinned_ = false;

signals:
    void windowChanged();
    void pinnedChanged();
};
