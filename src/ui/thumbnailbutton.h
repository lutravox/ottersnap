#pragma once

#include <QLabel>
#include <QMouseEvent>

class ThumbnailButton : public QLabel {
    Q_OBJECT

  public:
    explicit ThumbnailButton(QWidget *parent = nullptr);

    bool isSelected() const;
    void setSelected(bool selected);

  signals:
    void clicked();

  protected:
    void mousePressEvent(QMouseEvent *event) override;

  private:
    bool m_selected = false;
};
