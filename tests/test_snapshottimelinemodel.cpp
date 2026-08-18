#include <QtTest>
#include "core/snapshottimelinemodel.h"

class TestSnapshotTimelineModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testSetThumbnails();
    void testNewStatus();
    void testUpdateThumbnail();
    void testSnapshotOnly();

private:
    SnapshotTimelineModel *m_model = nullptr;
};

void TestSnapshotTimelineModel::init() {
    m_model = new SnapshotTimelineModel();
}

void TestSnapshotTimelineModel::cleanup() {
    delete m_model;
}

void TestSnapshotTimelineModel::testSetThumbnails() {
    QVector<QPixmap> thumbs = { QPixmap(10, 10), QPixmap(10, 10), QPixmap() };
    QVector<QString> labels = { "S1", "S2", "S3" };
    QVector<QUuid> identities = { QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid() };

    m_model->setThumbnails(thumbs, labels, identities);

    QCOMPARE(m_model->rowCount(), 3);
    QCOMPARE(m_model->data(m_model->index(0), SnapshotTimelineModel::LabelRole).toString(), QString("S1"));
    QCOMPARE(m_model->data(m_model->index(1), SnapshotTimelineModel::UuidRole).value<QUuid>(), identities[1]);
    QCOMPARE(m_model->data(m_model->index(2), SnapshotTimelineModel::ThumbnailRole).value<QPixmap>().isNull(), true);
}

void TestSnapshotTimelineModel::testNewStatus() {
    QVector<QPixmap> thumbs = { QPixmap(10, 10) };
    QVector<QString> labels = { "S1" };
    QVector<QUuid> identities = { QUuid::createUuid() };

    m_model->setThumbnails(thumbs, labels, identities);

    // Initially not new
    QCOMPARE(m_model->data(m_model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), false);

    // Mark as new (by dbId)
    m_model->markSnapshotAsNew(identities[0]);
    QCOMPARE(m_model->data(m_model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), true);

    // Clear new status (by dbId)
    m_model->clearNewStatus(identities[0]);
    QCOMPARE(m_model->data(m_model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), false);
}

void TestSnapshotTimelineModel::testUpdateThumbnail() {
    QVector<QPixmap> thumbs = { QPixmap(10, 10) };
    QVector<QString> labels = { "S1" };
    QVector<QUuid> identities = { QUuid::createUuid() };

    m_model->setThumbnails(thumbs, labels, identities);

    QPixmap newThumb(20, 20);
    m_model->updateThumbnail(0, newThumb);

    QCOMPARE(m_model->data(m_model->index(0), SnapshotTimelineModel::ThumbnailRole).value<QPixmap>().size(), QSize(20, 20));
}

void TestSnapshotTimelineModel::testSnapshotOnly() {
    QCOMPARE(m_model->isSnapshotOnly(), false);
    m_model->setSnapshotOnly(true);
    QCOMPARE(m_model->isSnapshotOnly(), true);
}

QTEST_MAIN(TestSnapshotTimelineModel)
#include "test_snapshottimelinemodel.moc"
