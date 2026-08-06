#pragma once

#include <QWidget>
#include <QString>

namespace DialogUtils {
    /**
     * @brief Displays a stylized confirmation dialog.
     * @param parent The parent widget.
     * @param title The title of the dialog.
     * @param text The message to display.
     * @param yesText Text for the affirmative button.
     * @param noText Text for the negative button.
     * @return True if the user clicked the affirmative button.
     */
    bool confirm(QWidget *parent,
                 const QString& title,
                 const QString& text,
                 const QString& yesText = "Yes",
                 const QString& noText = "No");
}
