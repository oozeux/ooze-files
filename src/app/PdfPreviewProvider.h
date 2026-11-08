#pragma once

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

// Renders the first page of a PDF with `pdftoppm` (poppler-utils) on a worker
// thread and stores it in the freedesktop thumbnail cache.  Keeping poppler
// out of the process avoids linking QtPdf just for previews, which keeps the
// idle memory footprint low.
class PdfPreviewProvider : public QQuickAsyncImageProvider
{
public:
    explicit PdfPreviewProvider(int maxThreads = 1);
    ~PdfPreviewProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize) override;

private:
    QThreadPool m_pool;
};
