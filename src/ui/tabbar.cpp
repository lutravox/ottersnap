#include "ui/tabbar.h"

TabBar::TabBar(QWidget *parent) : QTabWidget(parent) {
    setTabsClosable(true);
    setMovable(true);
    setDocumentMode(true);
}
