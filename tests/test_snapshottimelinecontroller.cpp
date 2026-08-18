#include <QtTest>
#include <QSignalSpy>
#include "controllers/imagesessioncontroller.h"
#include "controllers/appsettingscontroller.h"
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
    ImageSessionController     *m_sessionController = nullptr;
    ImageSession               *m_session = nullptr;
};

void TestSnapshotTimelineController::init() {
    m_sessionController = new ImageSessionController(nullptr); // Use nullptr instead of local settings
    m_controller = new SnapshotTimelineController();
    m_session = new ImageSession();

    m_controller->setSessionController(m_sessionController);
    m_sessionController->setActiveSession(m_session);
}

void TestSnapshotTimelineController::cleanup() {
    delete m_controller;
    delete m_session;
    delete m_sessionController;
}

void TestSnapshotTimelineController::testSelection() {
    // Setup model with some data
    auto* model = m_controller->model();
    QVector<QPixmap> thumbs = { QPixmap(), QPixmap(), QPixmap(), QPixmap(), QPixmap(), QPixmap() };
    QVector<QString> labels = { "S1", "S2", "S3", "S4", "S5", "S6" };
    QVector<QUuid> identities = { QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid(),
                                  QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid() };
    model->setThumbnails(thumbs, labels, identities);

    QSignalSpy spy(m_controller, &SnapshotTimelineController::snapshotSelected);

    m_controller->selectSnapshot(0);
    qDebug() << "Current selected index:" << m_controller->currentSelectedIndex();
    QCOMPARE(m_controller->currentSelectedIndex(), 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 0);

    // Selecting same index should not emit signal
    m_controller->selectSnapshot(0);
    QCOMPARE(spy.count(), 1);
}

void TestSnapshotTimelineController::testSecondarySelection() {
    QSignalSpy spy(m_controller, &SnapshotTimelineController::secondarySnapshotSelected);

    QUuid testUuid = QUuid::createUuid();
    QString testId = testUuid.toString(QUuid::WithoutBraces);
    m_controller->setSecondarySnapshot(testId);
    QCOMPARE(m_controller->secondarySnapshotId(), testId);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), testId);

    // Selecting same secondary should not emit signal
    m_controller->setSecondarySnapshot(testId);
    QCOMPARE(spy.count(), 1);
}

void TestSnapshotTimelineController::testRowForDbId() {
    // Setup model via controller's model (which is internal)
    auto* model = m_controller->model();
    QVector<QPixmap> thumbs = { QPixmap(), QPixmap() };
    QVector<QString> labels = { "S1", "S2" };
    QVector<QUuid> identities = { QUuid::createUuid(), QUuid::createUuid() };
    model->setThumbnails(thumbs, labels, identities);

    QCOMPARE(m_controller->rowForUuid(identities[0]), 0);
    QCOMPARE(m_controller->rowForUuid(identities[1]), 1);
    QCOMPARE(m_controller->rowForUuid(QUuid::createUuid()), -1);
}

void TestSnapshotTimelineController::testClearNewStatus() {
    auto* model = m_controller->model();
    QUuid testId = QUuid::createUuid();
    model->setThumbnails({ QPixmap() }, { "S1" }, { testId });
    model->markSnapshotAsNew(testId);
    QCOMPARE(model->data(model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), true);

    m_controller->clearNewStatus(testId);
    QCOMPARE(model->data(model->index(0), SnapshotTimelineModel::IsNewRole).toBool(), false);
}

void TestSnapshotTimelineController::testUpdateThumbnail() {
    auto* model = m_controller->model();
    model->setThumbnails({ QPixmap(10, 10) }, { "S1" }, { QUuid::createUuid() });

    QPixmap newThumb(20, 20);
    m_controller->updateThumbnail(0, newThumb);

    QCOMPARE(model->data(model->index(0), SnapshotTimelineModel::ThumbnailRole).value<QPixmap>().size(), QSize(20, 20));
}

QTEST_MAIN(TestSnapshotTimelineController)
#include "test_snapshottimelinecontroller.moc"
