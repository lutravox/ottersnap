#include <QtTest>
#include "core/notificationmodel.h"

class TestNotificationModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testRoleNames();
    void testAddMessage();
    void testIdsIncrement();
    void testTimeoutFlipsFadeOut();
    void testDefaultTimeout();
    void testRemoveItem();
    void testRemoveMissingIsNoOp();
    void testRemoveMiddle();
    void testTimersStoppedOnRemove();

private:
    NotificationModel *m_model = nullptr;
};

void TestNotificationModel::init() {
    m_model = new NotificationModel();
}

void TestNotificationModel::cleanup() {
    delete m_model;
}

void TestNotificationModel::testRoleNames() {
    QHash<int, QByteArray> names = m_model->roleNames();
    QCOMPARE(names[NotificationModel::IdRole], QByteArray("id"));
    QCOMPARE(names[NotificationModel::MessageRole], QByteArray("message"));
    QCOMPARE(names[NotificationModel::FadeOutRole], QByteArray("fadeOut"));
}

void TestNotificationModel::testAddMessage() {
    QCOMPARE(m_model->rowCount(), 0);

    m_model->addMessage("hello", -1);

    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->data(m_model->index(0), NotificationModel::MessageRole).toString(),
             QString("hello"));
    QCOMPARE(m_model->data(m_model->index(0), NotificationModel::FadeOutRole).toBool(), false);
}

void TestNotificationModel::testIdsIncrement() {
    m_model->addMessage("a", -1);
    m_model->addMessage("b", -1);
    m_model->addMessage("c", -1);

    const int first = m_model->data(m_model->index(0), NotificationModel::IdRole).toInt();
    const int second = m_model->data(m_model->index(1), NotificationModel::IdRole).toInt();
    const int third = m_model->data(m_model->index(2), NotificationModel::IdRole).toInt();
    QCOMPARE(second, first + 1);
    QCOMPARE(third, second + 1);
}

void TestNotificationModel::testTimeoutFlipsFadeOut() {
    m_model->addMessage("short", 30);

    QTest::qWait(150);

    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->data(m_model->index(0), NotificationModel::FadeOutRole).toBool(), true);
}

void TestNotificationModel::testDefaultTimeout() {
    m_model->addMessage("default", -1);

    // Well below the 5 s default; must not have flipped yet.
    QTest::qWait(50);
    QCOMPARE(m_model->data(m_model->index(0), NotificationModel::FadeOutRole).toBool(), false);

    m_model->addMessage("instant", 30);
    QTest::qWait(150);
    QCOMPARE(m_model->data(m_model->index(1), NotificationModel::FadeOutRole).toBool(), true);
}

void TestNotificationModel::testRemoveItem() {
    m_model->addMessage("a", -1);
    m_model->addMessage("b", -1);

    const int idA = m_model->data(m_model->index(0), NotificationModel::IdRole).toInt();

    m_model->removeItem(idA);

    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->data(m_model->index(0), NotificationModel::MessageRole).toString(),
             QString("b"));
}

void TestNotificationModel::testRemoveMissingIsNoOp() {
    m_model->addMessage("a", -1);

    m_model->removeItem(999);

    QCOMPARE(m_model->rowCount(), 1);
}

void TestNotificationModel::testRemoveMiddle() {
    m_model->addMessage("a", -1);
    m_model->addMessage("b", -1);
    m_model->addMessage("c", -1);

    const int idB = m_model->data(m_model->index(1), NotificationModel::IdRole).toInt();

    m_model->removeItem(idB);

    QCOMPARE(m_model->rowCount(), 2);
    QCOMPARE(m_model->data(m_model->index(0), NotificationModel::MessageRole).toString(),
             QString("a"));
    QCOMPARE(m_model->data(m_model->index(1), NotificationModel::MessageRole).toString(),
             QString("c"));
}

void TestNotificationModel::testTimersStoppedOnRemove() {
    m_model->addMessage("short", 30);
    const int id = m_model->data(m_model->index(0), NotificationModel::IdRole).toInt();

    m_model->removeItem(id);

    // Row is gone; the (already-fired or pending) timer must not re-add it.
    QTest::qWait(150);
    QCOMPARE(m_model->rowCount(), 0);
}

QTEST_GUILESS_MAIN(TestNotificationModel)
#include "test_notificationmodel.moc"
