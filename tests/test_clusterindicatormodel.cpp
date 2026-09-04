#include <QColor>
#include <QtTest>
#include "core/clusterindicatormodel.h"

class TestClusterIndicatorModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testDefaults();
    void testPosition();
    void testColor();
    void testVisible();
    void testDiameter();

private:
    ClusterIndicatorModel *m_model = nullptr;
};

void TestClusterIndicatorModel::init() {
    m_model = new ClusterIndicatorModel();
}

void TestClusterIndicatorModel::cleanup() {
    delete m_model;
}

void TestClusterIndicatorModel::testDefaults() {
    QCOMPARE(m_model->position(), QPointF(0, 0));
    QCOMPARE(m_model->visible(), false);
    QCOMPARE(m_model->diameter(), 25.0);
}

void TestClusterIndicatorModel::testPosition() {
    QSignalSpy spy(m_model, &ClusterIndicatorModel::positionChanged);

    m_model->setPosition(QPointF(10, 20));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->position(), QPointF(10, 20));

    m_model->setPosition(QPointF(10, 20));
    QCOMPARE(spy.count(), 1);
}

void TestClusterIndicatorModel::testColor() {
    QSignalSpy spy(m_model, &ClusterIndicatorModel::colorChanged);

    m_model->setColor(QColor(1, 2, 3));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->color(), QColor(1, 2, 3));

    m_model->setColor(QColor(1, 2, 3));
    QCOMPARE(spy.count(), 1);
}

void TestClusterIndicatorModel::testVisible() {
    QSignalSpy spy(m_model, &ClusterIndicatorModel::visibleChanged);

    m_model->setVisible(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->visible(), true);

    m_model->setVisible(true);
    QCOMPARE(spy.count(), 1);
}

void TestClusterIndicatorModel::testDiameter() {
    QSignalSpy spy(m_model, &ClusterIndicatorModel::diameterChanged);

    m_model->setDiameter(40.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->diameter(), 40.0);

    m_model->setDiameter(40.0);
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TestClusterIndicatorModel)
#include "test_clusterindicatormodel.moc"
