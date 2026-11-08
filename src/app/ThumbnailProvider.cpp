#include "app/ThumbnailProvider.h"

#include "core/ThumbnailCache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QQuickTextureFactory>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThreadPool>
#include <QUrl>

#include <atomic>
#include <memory>
#include <mutex>
#include <QCoreApplication>

namespace {

constexpr int kDefaultSide = 320;
constexpr int kMaxSide = 512;
constexpr int kProcessTimeoutMs = 20000;

// Movies frequently open on a black frame or a title card, so seek roughly a
// tenth of the way in, like other file managers do.
double videoDuration(const QString &path)
{
    const QString ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty())
        return 0.0;

    QProcess process;
    process.setProgram(ffprobe);
    process.setArguments({
        QStringLiteral("-v"),
        QStringLiteral("error"),
        QStringLiteral("-show_entries"),
        QStringLiteral("format=duration"),
        QStringLiteral("-of"),
        QStringLiteral("default=nw=1:nk=1"),
        path,
    });
    process.start();

    if (!process.waitForStarted(3000))
        return 0.0;

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return 0.0;
    }

    bool ok = false;
    const double duration = QString::fromLatin1(process.readAllStandardOutput())
                                .trimmed()
                                .toDouble(&ok);
    return ok && duration > 0.0 ? duration : 0.0;
}

QImage generateThumbnail(const QString &path, int side,
                         const std::atomic_bool &cancelled)
{
    if (path.isEmpty())
        return {};

    if (cancelled.load(std::memory_order_relaxed))
        return {};

    const QSize target(side, side);
    const QImage cached = ThumbnailCache::loadFor(path, target);
    if (!cached.isNull())
        return cached;

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return {};

    if (cancelled.load(std::memory_order_relaxed))
        return {};

    QList<double> seeks;
    const double duration = videoDuration(path);
    if (duration > 1.0) {
        const double position = qBound(1.0, duration * 0.1, duration - 1.0);
        seeks.append(position);
    } else {
        seeks.append(1.0);
    }
    seeks.append(0.0);

    for (const double seek : seeks) {
        if (cancelled.load(std::memory_order_relaxed))
            return {};

        QTemporaryFile temporary(QDir::tempPath()
                                 + QStringLiteral("/ooze-thumb-XXXXXX.png"));
        if (!temporary.open())
            return {};

        const QString temporaryPath = temporary.fileName();
        temporary.close();

        QProcess process;
        process.setProgram(ffmpeg);
        process.setArguments({
            QStringLiteral("-nostdin"),
            QStringLiteral("-v"),
            QStringLiteral("error"),
            QStringLiteral("-threads"),
            QStringLiteral("1"),
            QStringLiteral("-ss"),
            QString::number(seek, 'f', 3),
            QStringLiteral("-i"),
            path,
            QStringLiteral("-frames:v"),
            QStringLiteral("1"),
            QStringLiteral("-vf"),
            QStringLiteral("scale=%1:%1:force_original_aspect_ratio=decrease")
                .arg(side),
            QStringLiteral("-y"),
            temporaryPath,
        });
        process.start();

        if (!process.waitForStarted(3000))
            return {};

        if (!process.waitForFinished(kProcessTimeoutMs)) {
            process.kill();
            process.waitForFinished();
            continue;
        }

        const QImage image(temporaryPath);
        if (image.isNull())
            continue;

        ThumbnailCache::storeFor(path, target, image);
        return image;
    }

    return {};
}

class ThumbnailResponse;

// Shared between the response (owned by the QML pixmap reader thread) and the
// runnable executed by the thread pool.  Keeping the data outside the response
// lets the runnable finish safely even when the response has already been
// deleted after a cancellation.
struct ThumbnailState
{
    QString path;
    int side = kDefaultSide;
    QImage image;
    bool notified = false;
    std::atomic_bool cancelled{false};
    std::mutex mutex;
    ThumbnailResponse *response = nullptr;
};

class ThumbnailTask : public QRunnable
{
public:
    explicit ThumbnailTask(std::shared_ptr<ThumbnailState> state)
        : m_state(std::move(state))
    {
    }

    void run() override;

private:
    std::shared_ptr<ThumbnailState> m_state;
};

class ThumbnailResponse : public QQuickImageResponse
{
public:
    explicit ThumbnailResponse(std::shared_ptr<ThumbnailState> state)
        : m_state(std::move(state))
    {
        m_state->response = this;
    }

    ~ThumbnailResponse() override
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
        if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
            return QCoreApplication::translate("Ooze", "ffmpeg no está instalado");
        return QCoreApplication::translate("Ooze", "No se pudo generar la miniatura");
    }

    void cancel() override
    {
        m_state->cancelled.store(true, std::memory_order_release);
    }

private:
    std::shared_ptr<ThumbnailState> m_state;
};

void ThumbnailTask::run()
{
    if (!m_state->cancelled.load(std::memory_order_acquire))
        m_state->image = generateThumbnail(m_state->path, m_state->side,
                                           m_state->cancelled);

    // The response lives in the pixmap reader thread, so emitting while
    // holding the lock queues the signal and prevents the destructor from
    // clearing the pointer in the middle of the emission.
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

ThumbnailProvider::ThumbnailProvider(int maxThreads)
{
    m_pool.setMaxThreadCount(qMax(1, maxThreads));
    m_pool.setObjectName(QStringLiteral("ooze-files-thumbnails"));
}

ThumbnailProvider::~ThumbnailProvider()
{
    m_pool.waitForDone();
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(
    const QString &id, const QSize &requestedSize)
{
    auto state = std::make_shared<ThumbnailState>();
    state->path = localPathFromId(id);

    const QFileInfo info(state->path);
    if (!state->path.isEmpty() && info.isFile()) {
        state->side = requestedSize.isValid() && requestedSize.width() > 0
            ? qBound(64, requestedSize.width(), kMaxSide)
            : kDefaultSide;
    }

    auto *response = new ThumbnailResponse(state);
    m_pool.start(new ThumbnailTask(std::move(state)));
    return response;
}
