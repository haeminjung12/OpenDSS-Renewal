#pragma once

#include <QList>
#include <QString>
#include <QtCore/Qt>

class QLabel;
class QFrame;
class QPushButton;
class QSplitter;
class QVBoxLayout;
class QWidget;

namespace desktop_app::ui {

QFrame* makePanel(const QString& title, const QString& subtitle = QString());
QVBoxLayout* makePanelBody(QFrame* panel, int left = 12, int top = 12, int right = 12, int bottom = 12);
QFrame* makeMetric(const QString& label, QLabel* valueLabel);
QWidget* makeStatusRow(const QString& label, const QString& value, const QString& tone = QString());
QPushButton* makeToolButton(const QString& text);
QFrame* makeCollapsedGroup(const QString& title, QWidget* child);
QWidget* makeWorkspacePlaceholder(const QString& title, const QString& body, const char* objectName);
QLabel* makeMutedLabel(const QString& text);
void configureWorkspaceSplitter(QSplitter* splitter,
                                const QString& settingsKey,
                                const QList<int>& defaultSizes,
                                const QList<int>& minimumSectionSizes = {},
                                Qt::Orientation orientation = Qt::Horizontal);
bool restoreWorkspaceSplitterState(QSplitter* splitter);
void saveWorkspaceSplitterState(QSplitter* splitter);

} // namespace desktop_app::ui
