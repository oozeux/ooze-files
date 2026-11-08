pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

MenuPanel {
    submenuComponent: nested

    Component {
        id: nested

        MenuPanel {
            submenuComponent: nested
        }
    }
}
