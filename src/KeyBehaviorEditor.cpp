#include "KeyBehaviorEditor.h"
#include "KeyNames.h"
#include "ActionCatalog.h"
#include "ActionRunner.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
using Choice = QPair<QString, QString>; // display name, stored value
QList<Choice> physicalKeys() {
    QList<Choice> result{{"Enter", "RETURN"}, {"Space", "SPACE"}, {"Tab", "TAB"},
        {"Backspace", "BACK"}, {"Delete", "DELETE"}, {"Escape", "ESCAPE"},
        {"Arrow up", "UP"}, {"Arrow down", "DOWN"}, {"Arrow left", "LEFT"}, {"Arrow right", "RIGHT"},
        {"Home", "HOME"}, {"End", "END"}, {"Page up", "PRIOR"}, {"Page down", "NEXT"},
        {"Insert", "INSERT"}, {"Print screen", "SYSRQ"}, {"Pause / Break", "PAUSE"},
        {"Caps Lock", "CAPITAL"}};
    for (QChar c = 'A'; c <= QChar('Z'); c = QChar(c.unicode() + 1)) result.append({QString(c), QString(c)});
    for (int i = 0; i < 10; ++i) result.append({QString::number(i), QString::number(i)});
    for (int i = 1; i <= 24; ++i) result.append({QString("F%1").arg(i), QString("F%1").arg(i)});
    result.append({{"Left Ctrl", "LCONTROL"}, {"Right Ctrl", "RCONTROL"},
        {"Left Shift", "LSHIFT"}, {"Right Shift", "RSHIFT"}, {"Left Alt", "LMENU"},
        {"Right Alt", "RMENU"}, {"Left Super / Windows", "LWIN"}, {"Right Super / Windows", "RWIN"},
        {"Minus (−)", "MINUS"}, {"Equals (=)", "EQUAL"}, {"Left bracket ([)", "LEFTBRACE"},
        {"Right bracket (])", "RIGHTBRACE"}, {"Semicolon (;)", "SEMICOLON"},
        {"Apostrophe (')", "APOSTROPHE"}, {"Backtick (`)", "GRAVE"}, {"Backslash (\\)", "BACKSLASH"},
        {"Comma (,)", "COMMA"}, {"Period (.)", "DOT"}, {"Slash (/)", "SLASH"}});
    return result;
}
QList<Choice> availableActions() {
    QList<Choice> result{
        {"Keyboard · Pause / resume input", "Sleep"}, {"Keyboard · Open keyboard menu", "MenuKeyboard"},
        {"Keyboard · Move mode", "MoveModeToggle"}, {"Keyboard · Resize mode", "SizeModeToggle"}, {"Keyboard · Go back", "BackFromKeyboard"}, {"Keyboard · Cycle key mode", "CycleKeyMode"},
        {"Speech · Previous profile", "PreviousSpeechProfile"}, {"Speech · Next profile", "NextSpeechProfile"},
        {"Speech · Speak scratchpad", "Speak"}, {"Speech · Speak this key’s label", "SpeakLabel"},
        {"Typing · Clear scratchpad", "ClearScratchpad"}, {"Typing · Delete previous word", "BackMany"},
        {"Typing · Next suggestions", "NextSuggestions"}, {"Typing · Previous suggestions", "PreviousSuggestions"},
        {"Mouse · Click left button", "MouseLeftClick"}, {"Mouse · Choose point and left click", "MouseMoveAndLeftClick"},
        {"Mouse · Choose point and double click", "MouseMoveAndLeftDoubleClick"},
        {"Mouse · Choose point and right click", "MouseMoveAndRightClick"},
        {"Mouse · Choose point and middle click", "MouseMoveAndMiddleClick"},
        {"Mouse · Move to chosen point", "MouseMoveTo"}, {"Mouse · Drag between two points", "MouseDrag"},
        {"Mouse · Toggle left button", "MouseLeftDownUp"}, {"Mouse · Toggle middle button", "MouseMiddleDownUp"},
        {"Mouse · Toggle right button", "MouseRightDownUp"}, {"Mouse · Move up", "MouseMoveToTop"},
        {"Mouse · Move down", "MouseMoveToBottom"}, {"Mouse · Move left", "MouseMoveToLeft"},
        {"Mouse · Move right", "MouseMoveToRight"}, {"Mouse · Scroll up", "MouseScrollToTop"},
        {"Mouse · Scroll down", "MouseScrollToBottom"}, {"Mouse · Repeat last action", "RepeatLastMouseAction"},
        {"Mouse · Toggle magnifier", "MouseMagnifier"}, {"Mouse · Toggle magnetic cursor", "MouseMagneticCursor"},
        {"Mouse · Toggle look to scroll", "LookToScrollActive"}, {"Mouse · Cycle button mode", "CycleButtonMode"},
        {"Gamepad · Release all controls", "GamepadReset"}};
    const QList<Choice> buttons{{"A", "A"}, {"B", "B"}, {"X", "X"}, {"Y", "Y"},
        {"Start", "Start"}, {"Back", "Back"}, {"Guide", "Guide"}, {"Left shoulder", "LeftShoulder"},
        {"Right shoulder", "RightShoulder"}, {"Left stick", "LeftStick"}, {"Right stick", "RightStick"},
        {"D-pad up", "DpadUp"}, {"D-pad down", "DpadDown"}, {"D-pad left", "DpadLeft"}, {"D-pad right", "DpadRight"}};
    for (const auto &button : buttons)
        for (const auto &mode : {QString("Tap"), QString("Press"), QString("Release"), QString("Toggle")}) {
            QString value = "Gamepad" + button.second + mode;
            if (ActionCatalog::supported(value)) result.append({"Gamepad · " + mode + " " + button.first, value});
        }
    for (const auto &side : {QString("Left"), QString("Right")})
        for (int value : {0, 1})
            result.append({QString("Gamepad · %1 %2 trigger").arg(value ? "Press" : "Release", side.toLower()),
                           "Gamepad" + side + "Trigger:" + QString::number(value)});
    return result;
}
void populate(QComboBox *box, const QList<Choice> &choices) {
    for (const auto &choice : choices) box->addItem(choice.first, choice.second);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(22);
    box->setMaxVisibleItems(16);
}
QString displayName(const QList<Choice> &choices, const QString &value) {
    for (const auto &choice : choices) if (choice.second == value) return choice.first;
    return value;
}
void selectValue(QComboBox *box, const QString &value) {
    int index = box->findData(value);
    if (index < 0) { box->addItem("Existing: " + value, value); index = box->count() - 1; }
    box->setCurrentIndex(index);
}
}

bool KeyBehaviorEditor::isCommand(const QString &tag) {
    return QStringList{"Text", "Action", "KeyDown", "KeyUp", "KeyToggle", "Wait", "ChangeKeyboard"}.contains(tag);
}
KeyBehaviorEditor::KeyBehaviorEditor(QWidget *parent) : QWidget(parent) {
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto title = new QLabel("What happens when this key is selected");
    title->setWordWrap(true); outer->addWidget(title);
    auto hint = new QLabel("Steps run from top to bottom. Add one step for a simple key, or combine steps for a shortcut or phrase.");
    hint->setWordWrap(true); outer->addWidget(hint);
    list = new QListWidget; list->setObjectName("behaviorSteps");
    list->setMinimumHeight(90); list->setMaximumHeight(145); outer->addWidget(list);
    auto buttons = new QHBoxLayout;
    auto add = new QToolButton; add->setText("Add step"); add->setPopupMode(QToolButton::InstantPopup);
    auto menu = new QMenu(add);
    const QList<Choice> types{{"Type text or a phrase", "Text"}, {"Press a keyboard key", "ActionKey"},
        {"App, mouse, or gamepad action", "Action"}, {"Switch keyboard", "ChangeKeyboard"},
        {"Wait", "Wait"}, {"Hold a key down", "KeyDown"}, {"Release a key", "KeyUp"}, {"Toggle a key", "KeyToggle"}};
    for (const auto &type : types) {
        auto entry = menu->addAction(type.first);
        connect(entry, &QAction::triggered, this, [this, type] {
            QString value = type.second == "Text" ? "" : type.second == "Action" ? "Sleep" :
                type.second == "Wait" ? "250" : type.second == "ChangeKeyboard" ? destination->itemData(0).toString() : "RETURN";
            addStep(type.second == "ActionKey" ? "Action" : type.second, value);
        });
    }
    add->setMenu(menu); buttons->addWidget(add);
    auto shortcut = new QPushButton("Add shortcut…"); buttons->addWidget(shortcut);
    connect(shortcut, &QPushButton::clicked, this, &KeyBehaviorEditor::addShortcut);
    for (auto pair : {qMakePair(QString("↑"), -1), qMakePair(QString("↓"), 1)}) {
        auto button = new QPushButton(pair.first); button->setFixedWidth(32);
        button->setAccessibleName(pair.second < 0 ? "Move step up" : "Move step down");
        button->setToolTip(button->accessibleName()); buttons->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, pair] { moveStep(pair.second); });
    }
    auto remove = new QPushButton("Remove"); buttons->addWidget(remove);
    connect(remove, &QPushButton::clicked, this, [this] {
        int row = list->currentRow(); if (row < 0) return;
        steps.removeAt(row); refresh(qMin(row, int(steps.size()) - 1)); changed();
    });
    outer->addLayout(buttons);
    description = new QLabel; description->setWordWrap(true); outer->addWidget(description);
    fields = new QStackedWidget; fields->setMaximumHeight(110); outer->addWidget(fields);
    // Page order: empty, text, key, action, wait, navigation.
    fields->addWidget(new QWidget);
    text = new QPlainTextEdit; text->setObjectName("typedText"); text->setPlaceholderText("Enter exactly what this key should type…");
    text->setMaximumHeight(100); fields->addWidget(text);
    keyChoice = new QComboBox; keyChoice->setObjectName("physicalKey"); populate(keyChoice, physicalKeys()); fields->addWidget(keyChoice);
    actionChoice = new QComboBox; actionChoice->setObjectName("applicationAction"); populate(actionChoice, availableActions()); fields->addWidget(actionChoice);
    delay = new QSpinBox; delay->setObjectName("waitDuration"); delay->setRange(0, 10000); delay->setSuffix(" ms"); delay->setSingleStep(50); fields->addWidget(delay);
    auto navigation = new QWidget; auto navForm = new QVBoxLayout(navigation); navForm->setContentsMargins(0, 0, 0, 0);
    destination = new QComboBox; destination->setObjectName("keyboardDestination"); navForm->addWidget(destination);
    auto browse=new QPushButton("Choose keyboard file…");navForm->addWidget(browse);
    connect(browse,&QPushButton::clicked,this,[this]{
        auto file=QFileDialog::getOpenFileName(this,"Choose a keyboard from your pack",{},"Keyboard XML (*.xml)");
        if(file.isEmpty())return;
        QString name=QFileInfo(file).fileName();selectValue(destination,name);changeValue(name);
    });
    returnHere = new QCheckBox("Back returns to this keyboard"); navForm->addWidget(returnHere); fields->addWidget(navigation);
    connect(list, &QListWidget::currentRowChanged, this, &KeyBehaviorEditor::inspect);
    connect(text, &QPlainTextEdit::textChanged, this, [this] { changeValue(text->toPlainText()); });
    for (auto box : {keyChoice, actionChoice, destination})
        connect(box, &QComboBox::activated, this, [this, box] { changeValue(box->currentData().toString()); });
    connect(delay, &QSpinBox::valueChanged, this, [this](int value) { changeValue(QString::number(value)); });
    connect(returnHere, &QCheckBox::toggled, this, [this](bool checked) {
        if (loading || list->currentRow() < 0) return;
        auto step = steps[list->currentRow()];
        auto attributes = step.attributes();
        for (int i = attributes.size() - 1; i >= 0; --i)
            if (attributes.item(i).nodeName().compare("BackReturnsHere", Qt::CaseInsensitive) == 0)
                step.removeAttribute(attributes.item(i).nodeName());
        step.setAttribute("BackReturnsHere", checked ? "true" : "false"); changed();
    });
}
void KeyBehaviorEditor::load(const QDomElement &key, const QString &keyboardPath) {
    loading = true; steps.clear(); storage = QDomDocument();
    for (auto child = key.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        if (isCommand(child.tagName())) steps.append(storage.importNode(child, true).toElement());
    destination->clear();
    QDir directory(QFileInfo(keyboardPath).absolutePath());
    if (!keyboardPath.isEmpty()) for (const auto &file : directory.entryList({"*.xml"}, QDir::Files, QDir::Name))
        destination->addItem(QFileInfo(file).completeBaseName(), file);
    refresh(steps.isEmpty() ? -1 : 0); modified = false; loading = false;
}
void KeyBehaviorEditor::writeTo(QDomDocument &document, QDomElement key) const {
    if (!modified) return;
    // A key changed into an action or macro no longer represents a typing position.
    if(steps.size()!=1||steps.front().tagName()!="Text"){
        for(auto mapping=key.firstChildElement("PhysicalKey");!mapping.isNull();mapping=key.firstChildElement("PhysicalKey"))key.removeChild(mapping);
    }
    QDomNode marker;
    for (auto child = key.firstChild(); !child.isNull();) {
        auto next = child.nextSibling();
        if (child.isElement() && isCommand(child.toElement().tagName())) {
            if (marker.isNull()) { marker = document.createComment("commands"); key.insertBefore(marker, child); }
            key.removeChild(child);
        }
        child = next;
    }
    for (const auto &step : steps) {
        auto copy = document.importNode(step, true);
        if (marker.isNull()) key.appendChild(copy); else key.insertBefore(copy, marker);
    }
    if (!marker.isNull()) key.removeChild(marker);
}
void KeyBehaviorEditor::addStep(const QString &tag, const QString &value) {
    auto step = storage.createElement(tag); step.appendChild(storage.createTextNode(value));
    int row = list->currentRow() + 1; steps.insert(row, step); refresh(row); changed();
}
void KeyBehaviorEditor::moveStep(int offset) {
    int row = list->currentRow(), target = row + offset;
    if (row < 0 || target < 0 || target >= steps.size()) return;
    steps.move(row, target); refresh(target); changed();
}
QString KeyBehaviorEditor::summary(const QDomElement &step) const {
    auto tag = step.tagName(), value = step.text();
    if (tag == "Text") {
        if (value == " ") return "Type a space";
        if (value.isEmpty()) return "Type text — enter a phrase below";
        value.replace('\n', " ↵ "); value.replace('\t', " ⇥ ");
        return "Type “" + value.left(70) + (value.size() > 70 ? "…" : "") + "”";
    }
    if (tag == "Wait") return "Wait " + value + " ms";
    if (tag == "ChangeKeyboard") return "Switch to " + QFileInfo(value).completeBaseName();
    if (tag == "Action" && KeyNames::functionKey(value).isEmpty()) return displayName(availableActions(), value.trimmed());
    QString verb = tag == "KeyDown" ? "Hold " : tag == "KeyUp" ? "Release " : tag == "KeyToggle" ? "Toggle " : "Press ";
    return verb + displayName(physicalKeys(), KeyNames::canonical(value));
}
void KeyBehaviorEditor::refresh(int selection) {
    const bool previous = loading; loading = true;
    list->clear();
    for (int i = 0; i < steps.size(); ++i) list->addItem(QString::number(i + 1) + ".  " + summary(steps[i]));
    list->setCurrentRow(selection); loading = previous; inspect();
}
void KeyBehaviorEditor::inspect() {
    const bool previous = loading; loading = true;
    int row = list->currentRow();
    if (row < 0) { fields->setCurrentIndex(0); description->setText("No steps. Add a step to give this key a behavior."); loading = previous; return; }
    auto step = steps[row]; auto tag = step.tagName(), value = step.text();
    if (tag == "Text") { fields->setCurrentIndex(1); text->setPlainText(value); description->setText("Text to type · Spaces and line breaks are kept exactly as entered."); }
    else if (tag == "Wait") { fields->setCurrentIndex(4); delay->setValue(value.toInt()); description->setText("Delay before the next step (up to 10 seconds)."); }
    else if (tag == "ChangeKeyboard") {
        fields->setCurrentIndex(5); selectValue(destination, value);
        bool back = true; auto attributes = step.attributes();
        for (int i = 0; i < attributes.size(); ++i)
            if (attributes.item(i).nodeName().compare("BackReturnsHere", Qt::CaseInsensitive) == 0)
                back = attributes.item(i).nodeValue().compare("false", Qt::CaseInsensitive) != 0;
        returnHere->setChecked(back); description->setText("Choose a keyboard from the same folder. Save new keyboards in your pack folder to make them available here.");
    } else if (tag == "Action" && KeyNames::functionKey(value).isEmpty()) {
        fields->setCurrentIndex(3); selectValue(actionChoice, value.trimmed());
        description->setText(ActionCatalog::supported(value.trimmed()) ? "Choose what this step should do." : "This imported action is not supported by this version. Choose a replacement, or leave it unchanged to preserve it.");
    } else {
        fields->setCurrentIndex(2); selectValue(keyChoice, KeyNames::canonical(value));
        description->setText(tag == "KeyDown" ? "Hold this key until a Release step. For a shortcut, use Add shortcut instead." :
            tag == "KeyUp" ? "Release a key held by an earlier step." : tag == "KeyToggle" ? "Alternate between holding and releasing this key." : "Press this physical key. Modifier keys cycle their latched/locked state.");
    }
    loading = previous;
}
void KeyBehaviorEditor::changeValue(const QString &value) {
    if (loading || list->currentRow() < 0) return;
    int row = list->currentRow(); auto step = steps[row];
    while (!step.firstChild().isNull()) step.removeChild(step.firstChild());
    step.appendChild(storage.createTextNode(value));
    list->item(row)->setText(QString::number(row + 1) + ".  " + summary(step)); changed();
}
void KeyBehaviorEditor::changed() { if (!loading) { modified = true; emit edited(); } }
void KeyBehaviorEditor::addShortcut() {
    QDialog dialog(this); dialog.setWindowTitle("Add keyboard shortcut");
    auto form = new QFormLayout(&dialog);
    auto explanation = new QLabel("Choose modifiers and a key. The editor adds the press and release steps in the correct order.");
    explanation->setWordWrap(true); form->addRow(explanation);
    auto modifiers = new QWidget; auto row = new QHBoxLayout(modifiers); row->setContentsMargins(0, 0, 0, 0);
    QList<QCheckBox *> boxes;
    for (const auto &name : {"Ctrl", "Alt", "Shift", "Super"}) { auto box = new QCheckBox(name); boxes.append(box); row->addWidget(box); }
    form->addRow("Hold", modifiers);
    auto key = new QComboBox; populate(key, physicalKeys()); form->addRow("Press", key);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    QStringList held; const QStringList values{"LCONTROL", "LMENU", "LSHIFT", "LWIN"};
    for (int i = 0; i < boxes.size(); ++i) if (boxes[i]->isChecked()) held.append(values[i]);
    for (const auto &value : held) addStep("KeyDown", value);
    // Explicit down/up avoids modifier latching and faithfully represents a shortcut.
    addStep("KeyDown", key->currentData().toString()); addStep("KeyUp", key->currentData().toString());
    for (auto it = held.crbegin(); it != held.crend(); ++it) addStep("KeyUp", *it);
}

QString KeyBehaviorEditor::validationError() const {
    if(!modified)return {};
    QVariantList commands;
    for(const auto &step:steps){
        if(step.tagName()=="ChangeKeyboard"&&step.text().isEmpty())return "Choose a destination keyboard. Save this keyboard in your pack folder, reopen it, then select a keyboard from that folder.";
        commands.append(QVariantMap{{"tag",step.tagName()},{"text",step.text()}});
    }
    return ActionRunner::validate(commands,true);
}
