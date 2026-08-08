#include "ui/viewertools/viewertool.h"

QString IViewerTool::fullTooltip() const {
    QKeySequence s = shortcut();
    if (s.isEmpty())
        return tooltip();

    QString shortcutStr = s.toString();
    shortcutStr.replace("+", " + ");
    return QString("%1\n-\n%2").arg(tooltip(), shortcutStr);
}
