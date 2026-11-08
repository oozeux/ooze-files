#include "vfs/VfsRegistry.h"

#include "core/RecentFiles.h"
#include "vfs/LocalVfs.h"
#include "vfs/RecentVfs.h"
#include "vfs/TrashVfs.h"

VfsRegistry::VfsRegistry(QObject *parent)
    : QObject(parent)
{
    m_local = new LocalVfs(this);
    m_backends.insert(m_local->scheme(), m_local);

    auto *recent = new RecentVfs(RecentFiles::defaultFilePath(), this);
    m_backends.insert(recent->scheme(), recent);

    auto *trash = new TrashVfs(this);
    m_backends.insert(trash->scheme(), trash);
}

VfsRegistry::~VfsRegistry() = default;

VfsBackend *VfsRegistry::backendFor(const QUrl &url) const
{
    const auto it = m_backends.constFind(url.scheme());
    if (it != m_backends.constEnd())
        return it.value();

    if (url.scheme().isEmpty())
        return m_local;
    return nullptr;
}

bool VfsRegistry::isAvailable(const QString &scheme) const
{
    return m_backends.contains(scheme);
}

QStringList VfsRegistry::schemes() const
{
    return m_backends.keys();
}

void VfsRegistry::registerBackend(VfsBackend *backend)
{
    if (!backend)
        return;
    m_backends.insert(backend->scheme(), backend);
}
