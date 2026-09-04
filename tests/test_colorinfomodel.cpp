#include <QColor>
#include <QtTest>
#include "core/colorinfomodel.h"

class TestColorInfoModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testDefaults();
    void testHexColor();
    void testAlphaValue();
    void testVisible();
    void testSetPickedColor();
    void testSetClustersFromAnalyzer();
    void testSetClustersVariantList();
    void testSelection();
    void testResetSelection();
    void testHandleClusterSelected();

private:
    ColorInfoModel *m_model = nullptr;
};

void TestColorInfoModel::init() {
    m_model = new ColorInfoModel();
}

void TestColorInfoModel::cleanup() {
    delete m_model;
}

void TestColorInfoModel::testDefaults() {
    QCOMPARE(m_model->hexColor(), QString("#FFFFFF"));
    QCOMPARE(m_model->alphaValue(), 1.0);
    QCOMPARE(m_model->visible(), false);
    QVERIFY(m_model->clusters().isEmpty());
    QCOMPARE(m_model->selectedClusterId(), -1);
}

void TestColorInfoModel::testHexColor() {
    QSignalSpy spy(m_model, &ColorInfoModel::hexColorChanged);

    m_model->setHexColor("#ff8800");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->hexColor(), QString("#ff8800"));

    // Setting the same value must not emit.
    m_model->setHexColor("#ff8800");
    QCOMPARE(spy.count(), 1);
}

void TestColorInfoModel::testAlphaValue() {
    QSignalSpy spy(m_model, &ColorInfoModel::alphaValueChanged);

    m_model->setAlphaValue(0.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->alphaValue(), 0.5);

    m_model->setAlphaValue(0.5);
    QCOMPARE(spy.count(), 1);
}

void TestColorInfoModel::testVisible() {
    QSignalSpy spy(m_model, &ColorInfoModel::visibleChanged);

    m_model->setVisible(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->visible(), true);

    m_model->setVisible(true);
    QCOMPARE(spy.count(), 1);
}

void TestColorInfoModel::testSetPickedColor() {
    m_model->setPickedColor(QColor(255, 0, 0, 128));

    QCOMPARE(m_model->hexColor(), QString("#ff0000"));
    // alphaF() round-trips through a normalized float; allow a tiny drift.
    QVERIFY(qAbs(m_model->alphaValue() - 128.0 / 255.0) < 0.001);
}

void TestColorInfoModel::testSetClustersFromAnalyzer() {
    QList<ColorAnalyzer::ColorCluster> clusters;
    clusters.append({QPointF(1, 2), QColor(10, 20, 30), 100, QPointF(3, 4)});
    clusters.append({QPointF(5, 6), QColor(40, 50, 60), 42, QPointF(7, 8)});

    m_model->setClusters(clusters);

    const QVariantList rows = m_model->clusters();
    QCOMPARE(rows.size(), 2);

    const QVariantMap first = rows.at(0).toMap();
    QCOMPARE(first.value("id").toInt(), 0);
    QCOMPARE(first.value("center").toPointF(), QPointF(1, 2));
    QCOMPARE(first.value("color").value<QColor>(), QColor(10, 20, 30));
    QCOMPARE(first.value("count").toInt(), 100);
    QCOMPARE(first.value("samplePos").toPointF(), QPointF(3, 4));

    const QVariantMap second = rows.at(1).toMap();
    QCOMPARE(second.value("id").toInt(), 1);
    QCOMPARE(second.value("count").toInt(), 42);
}

void TestColorInfoModel::testSetClustersVariantList() {
    QSignalSpy spy(m_model, &ColorInfoModel::clustersChanged);

    QVariantList rows;
    rows.append(QVariantMap{{"id", 0}, {"color", QColor(1, 2, 3)}});
    m_model->setClusters(rows);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->clusters().size(), 1);

    // Same content must not emit.
    m_model->setClusters(rows);
    QCOMPARE(spy.count(), 1);
}

void TestColorInfoModel::testSelection() {
    QSignalSpy spy(m_model, &ColorInfoModel::selectedClusterIdChanged);

    m_model->setSelectedClusterId(3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->selectedClusterId(), 3);

    m_model->setSelectedClusterId(3);
    QCOMPARE(spy.count(), 1);
}

void TestColorInfoModel::testResetSelection() {
    m_model->setSelectedClusterId(2);

    m_model->resetSelection();

    QCOMPARE(m_model->selectedClusterId(), -1);
}

void TestColorInfoModel::testHandleClusterSelected() {
    QSignalSpy selectedSpy(m_model, &ColorInfoModel::clusterSelected);

    QVariantMap data{{"id", 5}, {"color", QColor(9, 8, 7)}};
    m_model->handleClusterSelected(data);

    QCOMPARE(m_model->selectedClusterId(), 5);
    QCOMPARE(selectedSpy.count(), 1);
    QCOMPARE(selectedSpy.first().first().toMap().value("id").toInt(), 5);
}

QTEST_GUILESS_MAIN(TestColorInfoModel)
#include "test_colorinfomodel.moc"
