#pragma once

#include "vfs/VfsBackend.h"

#include <QHash>
#include <QObject>
#include <QString>

class LocalVfs;

class VfsRegistry : public QObject
{
    Q_OBJECT
public:
    explicit VfsRegistry(QObject *parent = nullptr);
    ~VfsRegistry() override;

    VfsBackend *backendFor(const QUrl &url) const;

    [[nodiscard]] bool isAvailable(const QString &scheme) const;

    [[nodiscard]] QStringList schemes() const;

    void registerBackend(VfsBackend *backend);

private:
    QHash<QString, VfsBackend *> m_backends;
    LocalVfs *m_local = nullptr;
};
