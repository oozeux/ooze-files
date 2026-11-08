#pragma once

#include <QObject>
#include <QVariantList>

class ApplicationController;

class ActionRegistry : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList mainMenu READ mainMenu NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY entriesChanged)

public:
    explicit ActionRegistry(ApplicationController *controller, QObject *parent = nullptr);

    [[nodiscard]] QVariantList mainMenu() const { return m_mainMenu; }
    [[nodiscard]] QVariantList commands() const { return m_commands; }

    Q_INVOKABLE void trigger(const QString &id);

signals:
    void entriesChanged();
    void uiRequested(const QString &id);

private:
    void rebuild();

    ApplicationController *m_controller = nullptr;
    QVariantList m_mainMenu;
    QVariantList m_commands;
};
