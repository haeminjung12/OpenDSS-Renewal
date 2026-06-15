#pragma once

#include <QAction>
#include <QObject>
#include <QString>
#include <QWidget>

inline void nameObject(QObject* object, const char* name) {
    if (!object)
        return;
    object->setObjectName(QString::fromLatin1(name));
}

inline void nameWidget(QWidget* widget, const char* name) {
    nameObject(widget, name);
    if (!widget)
        return;
    widget->setAccessibleName(QString::fromLatin1(name));
}

inline void nameAction(QAction* action, const char* name) {
    nameObject(action, name);
    if (!action)
        return;
    action->setData(QString::fromLatin1(name));
}
