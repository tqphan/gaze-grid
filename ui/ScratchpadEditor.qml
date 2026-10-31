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
    Component.onCompleted: cursorPosition = length
    function prepareInput() {
        forceActiveFocus()
        if (Window.window) Window.window.requestActivate()
    }
    onTextChanged: if (!readOnly && text !== committedText) committedTextEdited(text)
}
