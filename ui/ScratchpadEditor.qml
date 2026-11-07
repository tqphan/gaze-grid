import QtQuick

TextEdit {
    property string committedText: ""
    signal committedTextEdited(string value)
    text: committedText
    textFormat: TextEdit.PlainText
    readOnly: true
    selectByMouse: !readOnly
    activeFocusOnPress: !readOnly
    wrapMode: TextEdit.Wrap
    Component.onCompleted: {
        cursorPosition = length
        prepareWhenEditable()
    }
    onVisibleChanged: prepareWhenEditable()
    onReadOnlyChanged: prepareWhenEditable()
    function prepareWhenEditable() {
        // Wait for the keyboard window's focus flags and layout to settle.
        Qt.callLater(function() {
            if (visible && !readOnly && Window.window && Window.window.visible)
                prepareInput()
        })
    }
    function prepareInput() {
        if (!activeFocus) forceActiveFocus()
        if (Window.window && !Window.window.active) Window.window.requestActivate()
    }
    onTextChanged: if (!readOnly && text !== committedText) committedTextEdited(text)
}
