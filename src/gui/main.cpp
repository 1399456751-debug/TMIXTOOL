#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>

int main (int argc, char** argv)
{
    QGuiApplication app (argc, argv);

    QGuiApplication::setApplicationName ("TMIXTOOL");
    QGuiApplication::setOrganizationName ("TMIXTOOL");
    QGuiApplication::setApplicationVersion ("1.1.3");

    // Window and taskbar icon. The .rc resource covers the file icon in
    // Explorer; this covers the running window, and both are needed.
    QGuiApplication::setWindowIcon (QIcon (QStringLiteral (":/assets/tmixtool.ico")));

    // The tool is deliberately light-only for now. FluentWinUI3 follows the
    // system scheme, and the palette the UI is designed around is a light one.
    QGuiApplication::styleHints()->setColorScheme (Qt::ColorScheme::Light);

    // Must be set before the QML engine loads anything, or the style will not
    // take effect.
    QQuickStyle::setStyle ("FluentWinUI3");

    QQmlApplicationEngine engine;
    engine.loadFromModule ("Tmixtool", "Main");

    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
