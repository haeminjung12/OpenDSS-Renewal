#include <QGuiApplication>
#include <QWindow>

#include <iostream>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);

    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        std::cerr << "Expected Qt platform offscreen, got "
                  << QGuiApplication::platformName().toStdString() << '\n';
        return 1;
    }
    if (!QGuiApplication::allWindows().isEmpty()) {
        std::cerr << "Offscreen platform smoke unexpectedly created a window\n";
        return 2;
    }

    std::cout << "QT_OFFSCREEN_PLATFORM_OK\n";
    return 0;
}
