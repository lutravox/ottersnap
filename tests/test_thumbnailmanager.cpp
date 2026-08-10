#include <QCoreApplication>
#include <QImage>
#include <QString>
#include <QtTest>
#include "core/thumbnailmanager.h"

class TestThumbnailManager : public QObject {
    Q_OBJECT

  private slots:
    void testFormatThumbnail();
};

void TestThumbnailManager::testFormatThumbnail() {
    QImage img(100, 50, QImage::Format_RGB32);
    img.fill(Qt::blue);

    int    thumbSize = 32;
    QImage thumb = ThumbnailManager::formatThumbnail(img, thumbSize);

    QCOMPARE(thumb.width(), thumbSize);
    QCOMPARE(thumb.height(), thumbSize);
    QVERIFY(thumb.pixelColor(thumbSize / 2, thumbSize / 2) == QColor(Qt::blue));
}

QTEST_MAIN(TestThumbnailManager)
#include "test_thumbnailmanager.moc"
