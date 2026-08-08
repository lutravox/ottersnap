#pragma once

#include <QIcon>
#include <QKeySequence>
#include <QObject>
#include <QString>

/**
 * @brief Interface for a toolbar tool in the viewer.
 */
class IViewerTool : public QObject {
    Q_OBJECT

  public:
    virtual ~IViewerTool() = default;

    /// @brief Returns the display name of the tool.
    virtual QString name() const = 0;

    /// @brief Returns the tooltip text for the tool.
    virtual QString tooltip() const = 0;

    /// @brief Returns the icon path for the tool.
    virtual QString iconPath() const = 0;

    /// @brief Whether the tool is a toggle (checkable).
    virtual bool isCheckable() const = 0;

    /// @brief Returns the keyboard shortcut for the tool, if any.
    virtual QKeySequence shortcut() const {
        return QKeySequence();
    }

    /// @brief Returns the formatted tooltip including the shortcut.
    QString fullTooltip() const;

    /// @brief Whether the tool is currently enabled.
    virtual bool isEnabled() const {
        return true;
    }

    /// @brief Called when the tool is toggled or triggered.
    virtual void onToggled(bool checked) = 0;

    /// @brief Updates the tool's visual state (e.g., checkbox) from the controller.
    virtual void syncState(bool state) = 0;

    /// @brief Initialize the tool with the required controllers.
    virtual void setup(class EffectsController *effects, class ViewerController *viewer) = 0;
};
