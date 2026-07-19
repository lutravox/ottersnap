#pragma once

#include <QDoubleSpinBox>
#include <QPushButton>
#include <QWidget>

/// @brief status bar with controls.
class StatusBar : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct the status bar.
    /// @param parent Optional parent widget.
    explicit StatusBar(QWidget *parent = nullptr);

    /// @brief Get the current zoom percentage.
    /// @return Zoom percentage (100.0 = 1:1).
    double zoom() const;

    /// @brief Set the zoom percentage.
    /// @param pct Zoom percentage (100.0 = 1:1).
    /// @param emitSignal If true, emits zoomChanged after setting.
    void setZoom(double pct, bool emitSignal = false);

  signals:
    /// @brief Emitted when the user changes the zoom via the spinbox.
    /// @param pct The new zoom percentage.
    void zoomChanged(double pct);

    /// @brief Emitted when the user clicks the fit button.
    void fitRequested();

  private:
    QDoubleSpinBox *spinbox;
    QPushButton    *btnFit;
};
