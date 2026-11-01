#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QLineEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPlainTextEdit>
#include "KeyboardEditor.h"
#include "KeyBehaviorEditor.h"
#include "ActionCatalog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
class KeyboardEditorTests : public QObject {
    Q_OBJECT
private slots:
    void editSaveUndoAndPreserveWhitespace() {
        QTemporaryDir dir;
        const auto path=dir.filePath("keyboard.xml");
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<Keyboard><Name>Test</Name><Width>100%</Width><Custom>keep</Custom><Canvas><Width>100</Width><Height>100</Height></Canvas><Content><DynamicKey X=\"5\" Y=\"5\" Width=\"20\" Height=\"20\" custom=\"keep\"><Label>Space</Label><Text> </Text><Wait>100</Wait></DynamicKey></Content></Keyboard>");file.close();
        KeyboardEditor editor;QVERIFY(editor.openFile(path));
        QCOMPARE(editor.items()[0].firstChildElement("Text").text(),QString(" "));
        editor.selected=0;editor.inspect();editor.label->setText("Blank");QVERIFY(editor.apply());
        QCOMPARE(editor.items()[0].firstChildElement("Label").text(),QString("Blank"));
        editor.moveKey(0,{30,40});
        QCOMPARE(editor.items()[0].attribute("X"),QString("30"));
        editor.restore(-1);QCOMPARE(editor.items()[0].attribute("X"),QString("5"));
        editor.restore(1);QVERIFY(editor.save(false));
        KeyboardEditor reopened;QVERIFY(reopened.openFile(path));
        QCOMPARE(reopened.items()[0].attribute("custom"),QString("keep"));
        QCOMPARE(reopened.document.documentElement().firstChildElement("Custom").text(),QString("keep"));
        QCOMPARE(reopened.items()[0].firstChildElement("Text").text(),QString(" "));
        QCOMPARE(reopened.items()[0].firstChildElement("Text").nextSiblingElement().tagName(),QString("Wait"));
    }
    void previewSelectionAndPendingEdits() {
        KeyboardEditor editor;editor.show();QTest::qWait(20);
        auto point=editor.view->mapFromScene(QPointF(60,35));
        QTest::mouseClick(editor.view->viewport(),Qt::LeftButton,Qt::NoModifier,point);
        QCOMPARE(editor.selected,0);
        editor.label->setFocus();editor.label->selectAll();QTest::keyClicks(editor.label,"Edited");
        QVERIFY(editor.formDirty);QVERIFY(!editor.view->isInteractive());
        QVERIFY(editor.apply());QVERIFY(editor.view->isInteractive());
        QCOMPARE(editor.items()[0].firstChildElement("Label").text(),QString("Edited"));
        QVERIFY(!editor.grab().isNull());
        editor.grab().save("/tmp/gaze-grid-editor-redesign.png");
    }
    void editBehaviorWithoutMarkup() {
        KeyboardEditor editor;
        auto text=editor.behavior->findChild<QPlainTextEdit *>("typedText");
        QVERIFY(text);text->setPlainText("Hello <world> & friends\n ");
        editor.behavior->addStep("Wait","250");
        auto delay=editor.behavior->findChild<QSpinBox *>("waitDuration");delay->setValue(500);
        editor.behavior->addStep("Action","RETURN");
        auto key=editor.behavior->findChild<QComboBox *>("physicalKey");
        key->setCurrentIndex(key->findData("F5"));key->activated(key->currentIndex());
        editor.behavior->moveStep(-1);
        QVERIFY(editor.apply());
        auto element=editor.items()[0];
        QCOMPARE(element.firstChildElement("Text").text(),QString("Hello <world> & friends\n "));
        auto action=element.firstChildElement("Text").nextSiblingElement();
        QCOMPARE(action.tagName(),QString("Action"));QCOMPARE(action.text(),QString("F5"));
        QCOMPARE(action.nextSiblingElement().text(),QString("500"));
        editor.restore(-1);QCOMPARE(editor.items()[0].firstChildElement("Text").text(),QString("a"));
    }
    void preserveMetadataWhileEditingSequence() {
        QTemporaryDir dir;auto path=dir.filePath("source.xml");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<Keyboard><Canvas><Width>100</Width><Height>100</Height></Canvas><Content><DynamicKey X=\"0\" Y=\"0\" Width=\"20\" Height=\"20\"><Label>Menu</Label><Symbol>MenuIcon</Symbol><KeyGroup>Special</KeyGroup><ChangeKeyboard BackReturnsHere=\"false\" custom=\"keep\">other.xml</ChangeKeyboard><Custom>retained</Custom></DynamicKey></Content></Keyboard>");file.close();
        KeyboardEditor editor;QVERIFY(editor.openFile(path));editor.selected=0;editor.inspect();
        auto back=editor.behavior->findChild<QCheckBox *>();QVERIFY(back);QVERIFY(!back->isChecked());
        editor.behavior->addStep("Text"," ");QVERIFY(editor.apply());QVERIFY(editor.save(false));
        KeyboardEditor reopened;QVERIFY(reopened.openFile(path));auto key=reopened.items()[0];
        QCOMPARE(key.firstChildElement("Symbol").text(),QString("MenuIcon"));
        QCOMPARE(key.firstChildElement("KeyGroup").text(),QString("Special"));
        QCOMPARE(key.firstChildElement("Custom").text(),QString("retained"));
        QCOMPARE(key.firstChildElement("ChangeKeyboard").attribute("BackReturnsHere"),QString("false"));
        QCOMPARE(key.firstChildElement("ChangeKeyboard").attribute("custom"),QString("keep"));
        QCOMPARE(key.firstChildElement("Text").text(),QString(" "));
    }
    void physicalMappingSurvivesTextEditsButNotMacros() {
        QDomDocument doc;QVERIFY(doc.setContent(QString("<DynamicKey><Label>q</Label><PhysicalKey>Q</PhysicalKey><Text>q</Text></DynamicKey>")));
        KeyBehaviorEditor behavior;behavior.load(doc.documentElement(),{});
        auto text=behavior.findChild<QPlainTextEdit *>("typedText");QVERIFY(text);text->setPlainText("a");
        behavior.writeTo(doc,doc.documentElement());
        QCOMPARE(doc.documentElement().firstChildElement("PhysicalKey").text(),QString("Q"));
        behavior.addStep("Wait","10");behavior.writeTo(doc,doc.documentElement());
        QVERIFY(doc.documentElement().firstChildElement("PhysicalKey").isNull());
    }
    void shortcutBuilderAndActionChoices() {
        KeyBehaviorEditor behavior;QDomDocument doc;doc.setContent(QString("<DynamicKey/>"));behavior.load(doc.documentElement(),{});
        auto choices=behavior.findChild<QComboBox *>("applicationAction");QVERIFY(choices);
        for(int i=0;i<choices->count();++i)QVERIFY2(ActionCatalog::supported(choices->itemData(i).toString()),qPrintable(choices->itemData(i).toString()));
        QTimer::singleShot(0,[] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            auto checks=dialog->findChildren<QCheckBox *>();checks[0]->setChecked(true);
            auto key=dialog->findChild<QComboBox *>();key->setCurrentIndex(key->findData("C"));
            dialog->accept();
        });
        for(auto button:behavior.findChildren<QPushButton *>())if(button->text()=="Add shortcut…")button->click();
        behavior.writeTo(doc,doc.documentElement());
        auto node=doc.documentElement().firstChildElement();
        QCOMPARE(node.tagName(),QString("KeyDown"));QCOMPARE(node.text(),QString("LCONTROL"));node=node.nextSiblingElement();
        QCOMPARE(node.tagName(),QString("KeyDown"));QCOMPARE(node.text(),QString("C"));node=node.nextSiblingElement();
        QCOMPARE(node.tagName(),QString("KeyUp"));QCOMPARE(node.text(),QString("C"));node=node.nextSiblingElement();
        QCOMPARE(node.tagName(),QString("KeyUp"));QCOMPARE(node.text(),QString("LCONTROL"));
        QVERIFY(behavior.validationError().isEmpty());
    }
    void gridMoveSnapsAndClamps() {
        QTemporaryDir dir;auto path=dir.filePath("grid.xml");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Keyboard><Grid><Rows>3</Rows><Cols>4</Cols></Grid><Content><DynamicKey Row=\"0\" Col=\"0\"><Label>A</Label><Text>a</Text></DynamicKey></Content></Keyboard>");file.close();
        KeyboardEditor editor;QVERIFY(editor.openFile(path));editor.moveKey(0,{2.3,50});QCOMPARE(editor.items()[0].attribute("Col"),QString("2"));QCOMPARE(editor.items()[0].attribute("Row"),QString("2"));
    }
};
QTEST_MAIN(KeyboardEditorTests)
#include "KeyboardEditorTests.moc"
