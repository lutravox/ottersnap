#pragma once

#include <QLabel>
#include <QMouseEvent>

class SnapshotTab : public QLabel {
    Q_OBJECT

  public:
    explicit SnapshotTab(QWidget *parent = nullptr);

    bool isSelected() const;
    void setSelected(bool selected);

  signals:
    void clicked();

  protected:
    void mousePressEvent(QMouseEvent *event) override;

  private:
    bool m_selected = false;
};
