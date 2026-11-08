#include "app/PdfPreviewProvider.h"

#include "core/ThumbnailCache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QQuickTextureFactory>
#include <QStandardPaths>
#include <QUuid>
#include <QUrl>

#include <atomic>
#include <memory>
#include <mutex>
#include <QCoreApplication>

namespace {

constexpr int kDefaultSide = 1024;
constexpr int kMaxSide = 2048;

QImage renderPdf(const QString &path, int side, const std::atomic_bool &cancelled)
{
    if (path.isEmpty() || cancelled.load(std::memory_order_relaxed))
        return {};

    const QSize target(side, side);
    const QImage cached = ThumbnailCache::loadFor(path, target);
    if (!cached.isNull())
        return cached;

    const QString pdftoppm = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
    if (pdftoppm.isEmpty())
        return {};

    const QString prefix = QDir::tempPath() + QStringLiteral("/ooze-pdf-")
        + QUuid::createUuid().toString(QUuid::Id128);
    const QString output = prefix + QStringLiteral(".png");

    QProcess process;
    process.setProgram(pdftoppm);
    process.setArguments({
        QStringLiteral("-f"), QStringLiteral("1"),
        QStringLiteral("-l"), QStringLiteral("1"),
        QStringLiteral("-singlefile"),
        QStringLiteral("-png"),
        QStringLiteral("-scale-to"), QString::number(side),
        path,
        prefix,
    });
    process.start();

    if (!process.waitForStarted(3000))
        return {};

    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished();
        QFile::remove(output);
        return {};
    }

    const QImage image(output);
    QFile::remove(output);

    if (image.isNull() || cancelled.load(std::memory_order_relaxed))
        return image;

    ThumbnailCache::storeFor(path, target, image);
    return image;
}

class PdfResponse;

struct PdfState
{
    QString path;
    int side = kDefaultSide;
    QImage image;
    bool notified = false;
    std::atomic_bool cancelled{false};
    std::mutex mutex;
    PdfResponse *response = nullptr;
};

class PdfTask : public QRunnable
{
public:
    explicit PdfTask(std::shared_ptr<PdfState> state)
        : m_state(std::move(state))
    {
    }

    void run() override;

private:
    std::shared_ptr<PdfState> m_state;
};

class PdfResponse : public QQuickImageResponse
{
public:
    explicit PdfResponse(std::shared_ptr<PdfState> state)
        : m_state(std::move(state))
    {
        m_state->response = this;
    }

    ~PdfResponse() override
    {
        m_state->cancelled.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->response = nullptr;
    }

    QQuickTextureFactory *textureFactory() const override
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->image.isNull())
            return nullptr;
        return QQuickTextureFactory::textureFactoryForImage(m_state->image);
    }

    QString errorString() const override
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (!m_state->image.isNull())
            return {};
        if (QStandardPaths::findExecutable(QStringLiteral("pdftoppm")).isEmpty())
            return QCoreApplication::translate("Ooze", "pdftoppm no está instalado (poppler-utils)");
        return QCoreApplication::translate("Ooze", "No se pudo generar la vista previa del PDF");
    }

    void cancel() override
    {
        m_state->cancelled.store(true, std::memory_order_release);
    }

private:
    std::shared_ptr<PdfState> m_state;
};

void PdfTask::run()
{
    if (!m_state->cancelled.load(std::memory_order_acquire))
        m_state->image = renderPdf(m_state->path, m_state->side, m_state->cancelled);

    std::lock_guard<std::mutex> lock(m_state->mutex);
    if (m_state->response == nullptr || m_state->notified)
        return;
    m_state->notified = true;
    emit m_state->response->finished();
}

QString localPathFromId(const QString &id)
{
    QString raw = id;
    const qsizetype query = raw.indexOf(QLatin1Char('?'));
    if (query >= 0)
        raw.truncate(query);

    QUrl url = QUrl::fromEncoded(raw.toUtf8());
    if (!url.isLocalFile())
        url = QUrl(QUrl::fromPercentEncoding(raw.toUtf8()));
    if (!url.isLocalFile())
        url = QUrl(raw);

    return url.isLocalFile() ? url.toLocalFile() : QString();
}

} // namespace

PdfPreviewProvider::PdfPreviewProvider(int maxThreads)
{
    m_pool.setMaxThreadCount(qMax(1, maxThreads));
    m_pool.setObjectName(QStringLiteral("ooze-files-pdf"));
}

PdfPreviewProvider::~PdfPreviewProvider()
{
    m_pool.waitForDone();
}

QQuickImageResponse *PdfPreviewProvider::requestImageResponse(
    const QString &id, const QSize &requestedSize)
{
    auto state = std::make_shared<PdfState>();
    state->path = localPathFromId(id);
    state->side = requestedSize.isValid() && requestedSize.width() > 0
        ? qBound(128, requestedSize.width(), kMaxSide)
        : kDefaultSide;

    auto *response = new PdfResponse(state);
    m_pool.start(new PdfTask(std::move(state)));
    return response;
}
