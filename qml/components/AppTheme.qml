pragma Singleton
import QtQuick

QtObject {
    property bool light: true
    readonly property color background: light ? "#f3f3f3" : "#181818"
    readonly property color surface: light ? "#ffffff" : "#1f1f1f"
    readonly property color surfaceAlt: light ? "#f8f8f8" : "#252526"
    readonly property color navigation: light ? "#f3f3f3" : "#181818"
    readonly property color border: light ? "#d6d6d6" : "#2b2b2b"
    readonly property color divider: light ? "#e5e5e5" : "#313131"
    readonly property color text: light ? "#242424" : "#cccccc"
    readonly property color textMuted: light ? "#616161" : "#9d9d9d"
    readonly property color textFaint: light ? "#8a8a8a" : "#707070"
    readonly property color accent: light ? "#0078d4" : "#4daafc"
    readonly property color accentSoft: light ? "#e6f2fb" : "#15364d"
    readonly property color positive: light ? "#16834f" : "#34d399"
    readonly property color negative: light ? "#c83b32" : "#fb7185"
    readonly property color warning: light ? "#ad6200" : "#f59e0b"
    readonly property color input: light ? "#ffffff" : "#f8fafc"
    readonly property color inputText: "#172033"
    readonly property color button: light ? "#ffffff" : "#e5e7eb"
}
