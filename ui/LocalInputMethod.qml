import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings

Item {
    property bool requiresFocus: true
    readonly property bool ready: InputContext.inputEngine.inputMethod !== null
    readonly property list<string> availableLocales: VirtualKeyboardSettings.availableLocales
    readonly property string locale: InputContext.locale
    readonly property QtObject candidateModel: InputContext.inputEngine.wordCandidateListModel
    signal candidatesChanged()
    onAvailableLocalesChanged:candidatesChanged()
    onLocaleChanged:candidatesChanged()
    onCandidateModelChanged:candidatesChanged()
    Connections {
        target:candidateModel
        function onRowsInserted(){candidatesChanged()}
        function onRowsRemoved(){candidatesChanged()}
        function onRowsMoved(){candidatesChanged()}
        function onModelReset(){candidatesChanged()}
        function onDataChanged(){candidatesChanged()}
        function onLayoutChanged(){candidatesChanged()}
    }
    // The engine's input methods are created by its keyboard layouts.
    // Keep that implementation panel hidden; the user's gaze layout stays visible.
    InputPanel { width:600; opacity:0; enabled:false }
    function dispatch(key, text, modifiers) {
        if(!ready) return false
        return InputContext.inputEngine.virtualKeyClick(key, text, modifiers)
    }
    function setLocale(value) {
        if (availableLocales.indexOf(value)<0) return false
        InputContext.clear()
        VirtualKeyboardSettings.locale=value
        return true
    }
    function selectCandidate(index) {
        if(index<0||index>=candidateModel.count) return false
        candidateModel.selectItem(index)
        return true
    }
}
