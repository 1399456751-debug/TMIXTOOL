#include "WindowPin.h"

#include <QCoreApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#  define NOMINMAX // windows.h defines min/max as macros
#  include <windows.h>
#endif

namespace {

// Beside the executable, so the setting travels with a copied folder.
// QSettings reports failure by simply not persisting, which is the right
// outcome for a read-only location: the toggle still works for the session.
QString settingsFile()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral ("/TMIXTOOL.ini");
}

constexpr auto kSettingKey = "window/alwaysOnTop";

} // namespace

WindowPin::WindowPin (QObject* parent)
    : QObject (parent)
{
    const QSettings settings (settingsFile(), QSettings::IniFormat);
    pinned_ = settings.value (QString::fromLatin1 (kSettingKey), false).toBool();
}

void WindowPin::setWindow (QQuickWindow* window)
{
    if (window_ == window)
        return;

    window_ = window;
    emit windowChanged();

    // The native window does not exist until the scene has been created, so
    // the stored preference is applied here rather than in the constructor.
    apply();
}

void WindowPin::setPinned (bool pinned)
{
    if (pinned_ == pinned)
        return;

    pinned_ = pinned;
    apply();

    QSettings settings (settingsFile(), QSettings::IniFormat);
    settings.setValue (QString::fromLatin1 (kSettingKey), pinned_);

    emit pinnedChanged();
}

void WindowPin::apply()
{
#ifdef Q_OS_WIN
    if (window_ == nullptr)
        return;

    // Forces the native window into existence, which is what the call needs.
    const auto handle = reinterpret_cast<HWND> (window_->winId());
    if (handle == nullptr)
        return;

    ::SetWindowPos (handle,
                    pinned_ ? HWND_TOPMOST : HWND_NOTOPMOST,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}
