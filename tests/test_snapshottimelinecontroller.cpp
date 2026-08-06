#include <QtTest>
#include <QSignalSpy>
#include "controllers/snapshottimelinecontroller.h"
#include "core/imagesession.h"
#include "core/snapshottimelinemodel.h"

class TestSnapshotTimelineController : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testSelection();
    void testSecondarySelection();
    void testRowForDbId();
    void testClearNewStatus();
    void testUpdateThumbnail();

private:
    SnapshotTimelineController *m_controller = nullptr;
};

void TestSnapshotTimelineController::init() {
    m_controller = new SnapshotTimelineController();
}

void TestSnapshotTimelineController::cleanup() {
    delete m_controller;
}

void TestSnapshotTimelineController::testSelection() {
    QSignalSpy spy(m_controller, &SnapshotTimelineController::snapshotSelected);

    m_controller->selectSnapshot(5);
    QCOMPARE(m_controller->currentSelectedIndex(), 5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 5);

    // Selecting same index should not emit signal
    m_controller->selectSnapshot(5);
    QCOMPARE(spy.count(), 1);
}

void TestSnapshotTimelineController::testSecondarySelection() {
    QSignalSpy spy(m_controller, &SnapshotTimelineController::secondarySnapshotSelected);

    m_controller->setSecondarySnapshot(100);
    QCOMPARE(m_controller->secondarySnapshotDbId(), 100);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 100);

    // Selecting same secondary should not emit signal
    m_controller->setSecondarySnapshot(100);
    QCOMPARE(spy.count(), 1);
}

void TestSnapshotTimelineController::testRowForDbId() {
    // Setup model via controller's model (which is internal)
    auto* model = m_controller->model();
    QVector<QPixmap> thumbs = { QPixmap(), QPixmap() };
    QVector<QString> labels = { "S1", "S2" };
    QVector<int> indices = { 100, 200 };
    model->setThumbnails(thumbs, labels, indices);

    QCOMPARE(m_controller->rowForDbId(100), 0);
    QCOMPARE(m_controller->rowForDbId(200), 1);
    QCOMPARE(m_controller->rowForDbId(300), -1);
}

void TestSnapshotTimelineController::testClearNewStatus() {
    auto* model = m_controller->model();
    model->setThumbnails({ QPixmap() }, { "S1" }, { 100 });
    model->markSnapshotAsNew(100);
    QCOMPARE(model->data(model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), true);

    m_controller->clearNewStatus(100);
    QCOMPARE(model->data(model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), false);
}

void TestSnapshotTimelineController::testUpdateThumbnail() {
    auto* model = m_controller->model();
    model->setThumbnails({ QPixmap(10, 10) }, { "S1" }, { 100 });

    QPixmap newThumb(20, 20);
    m_controller->updateThumbnail(0, newThumb);

    QCOMPARE(model->data(model->index(0), SnapshotTimelineModel::ThumbnailRole).value<QPixmap>().size(), QSize(20, 20));
}

QTEST_MAIN(TestSnapshotTimelineController)
#include "test_snapshottimelinecontroller.moc"
