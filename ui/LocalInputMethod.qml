import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings

Item {
    property bool requiresFocus: true
    readonly property bool ready: InputContext.inputEngine.inputMethod !== null
                                 && (!VirtualKeyboardSettings.locale || InputContext.locale === VirtualKeyboardSettings.locale)
    readonly property list<string> availableLocales: VirtualKeyboardSettings.availableLocales
    readonly property string locale: InputContext.locale
    readonly property QtObject candidateModel: InputContext.inputEngine.wordCandidateListModel
    signal candidatesChanged()
    property bool candidateNotificationPending: false
    function notifyCandidatesChanged() {
        if(candidateNotificationPending) return
        candidateNotificationPending=true
        // Some engines notify while replacing their internal candidate list.
        // Read it after the mutation returns, once for the whole notification burst.
        Qt.callLater(function() {
            candidateNotificationPending=false
            candidatesChanged()
        })
    }
    onAvailableLocalesChanged:notifyCandidatesChanged()
    onLocaleChanged:notifyCandidatesChanged()
    onCandidateModelChanged:notifyCandidatesChanged()
    Connections {
        target:candidateModel
        function onRowsInserted(){notifyCandidatesChanged()}
        function onRowsRemoved(){notifyCandidatesChanged()}
        function onRowsMoved(){notifyCandidatesChanged()}
        function onModelReset(){notifyCandidatesChanged()}
        function onDataChanged(){notifyCandidatesChanged()}
        function onLayoutChanged(){notifyCandidatesChanged()}
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
    function cancel() { InputContext.clear() }
}
