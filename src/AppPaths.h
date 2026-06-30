#pragma once

#include <QString>

class AppPaths {
public:
    static QString root();
    static QString data();
    static QString logs();
    static QString cache();
    static QString etf();
    static QString exports();
    static QString resources();
    static QString resourceFile(const QString& relativePath);
    static void ensureAll();
};
