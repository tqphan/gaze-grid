#include "KeyboardEditor.h"
#include "KeyboardPack.h"
#include "KeyboardLayout.h"
#include "KeyBehaviorEditor.h"
#include "ActionRunner.h"
#include <QCloseEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QStyleOptionGraphicsItem>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QScrollArea>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QPainter>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QLabel>
#include <QAction>
#include <cmath>
#include <functional>
namespace {
class EditorView : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;
protected:
    void resizeEvent(QResizeEvent *event) override {
        QGraphicsView::resizeEvent(event);
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
};
class KeyItem : public QGraphicsRectItem {
public:
    std::function<void(QPointF)> moved;
    QPointF pressPosition;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(brush());
        painter->setPen(QPen(option->state & QStyle::State_Selected ? QColor("#4285f4") : QColor("#8d9bab"),
                            option->state & QStyle::State_Selected ? 3 : 1));
        painter->drawRoundedRect(rect().adjusted(2,2,-2,-2),5,5);
    }
    using QGraphicsRectItem::QGraphicsRectItem;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        pressPosition = pos();
        QGraphicsRectItem::mousePressEvent(event);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
        QGraphicsRectItem::mouseReleaseEvent(event);
        if(moved && pos() != pressPosition) moved(pos());
    }
};
void setText(QDomDocument &doc, QDomElement element, const QString &tag, const QString &text) {
    auto child=element.firstChildElement(tag);
    if(child.isNull()){child=doc.createElement(tag);element.appendChild(child);}
    while(!child.firstChild().isNull())child.removeChild(child.firstChild());
    child.appendChild(doc.createTextNode(text));
}
void attribute(QDomElement element, const QString &key, double value) {
    // XML packs use mixed attribute casing; avoid introducing duplicate attributes.
    auto attrs=element.attributes();
    for(int i=attrs.count()-1;i>=0;--i)if(attrs.item(i).nodeName().compare(key,Qt::CaseInsensitive)==0)element.removeAttribute(attrs.item(i).nodeName());
    element.setAttribute(key,QString::number(value,'g',12));
}
}
KeyboardEditor::KeyboardEditor(QWidget *parent):QMainWindow(parent) {
    resize(1240,820);
    setMinimumSize(1000,650);
    auto toolbar=addToolBar("Keyboard editor");toolbar->setMovable(false);
    auto action=[&](QString text,QKeySequence shortcut,auto callback){auto a=toolbar->addAction(text);a->setShortcut(shortcut);connect(a,&QAction::triggered,this,[this,callback]{if(formDirty&&!apply())return;callback();});};
    action("New",QKeySequence::New,[this]{if(discard())newDocument();});
    action("Open…",QKeySequence::Open,[this]{if(!discard())return;auto file=QFileDialog::getOpenFileName(this,"Open keyboard",path,"Keyboard XML (*.xml)");if(!file.isEmpty())openFile(file);});
    action("Save",QKeySequence::Save,[this]{save(false);});
    action("Save as…",QKeySequence::SaveAs,[this]{save(true);});
    toolbar->addSeparator();
    action("Undo",QKeySequence::Undo,[this]{restore(-1);});
    action("Redo",QKeySequence::Redo,[this]{restore(1);});
    toolbar->addSeparator();
    action("Add key",{},[this]{auto next=document.cloneNode(true).toDocument();auto content=next.documentElement().firstChildElement("Content");auto key=next.createElement("DynamicKey");bool canvas=layout["layoutMode"]=="canvas";if(canvas){key.setAttribute("X",0);key.setAttribute("Y",0);key.setAttribute("Width",10);key.setAttribute("Height",10);}setText(next,key,"Label","New key");setText(next,key,"Text","");content.appendChild(key);selected=items().size();commit(next);});
    action("Duplicate",{},[this]{auto list=items();if(selected<0||selected>=list.size())return;auto next=document.cloneNode(true).toDocument();auto content=next.documentElement().firstChildElement("Content");auto copy=next.importNode(list[selected],true).toElement();if(layout["layoutMode"]!="canvas"){copy.removeAttribute("Row");copy.removeAttribute("Col");copy.removeAttribute("row");copy.removeAttribute("col");}content.appendChild(copy);selected=list.size();commit(next);});
    action("Delete",{},[this]{auto next=document.cloneNode(true).toDocument();auto content=next.documentElement().firstChildElement("Content");auto node=content.firstChildElement();for(int i=0;i<selected&&!node.isNull();++i)node=node.nextSiblingElement();if(selected>=0&&!node.isNull()){content.removeChild(node);selected=-1;commit(next);}});
    auto splitter=new QSplitter;setCentralWidget(splitter);
    scene=new QGraphicsScene(this);view=new EditorView(scene);view->setRenderHint(QPainter::Antialiasing);view->setMinimumWidth(480);splitter->addWidget(view);
    auto panel=new QWidget;
    panel->setMinimumWidth(430);
    auto panelLayout=new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18,18,18,18);
    panelLayout->setSpacing(14);
    auto keyboardForm=new QFormLayout;
    name=new QLineEdit;keyboardForm->addRow("Keyboard name",name);panelLayout->addLayout(keyboardForm);
    selectionTitle=new QLabel("Select a key");
    auto headingFont=selectionTitle->font();headingFont.setPointSize(16);headingFont.setBold(true);selectionTitle->setFont(headingFont);
    panelLayout->addWidget(selectionTitle);
    label=new QLineEdit;label->setPlaceholderText("Name shown on the key");
    auto labelForm=new QFormLayout;labelForm->addRow("Key label",label);panelLayout->addLayout(labelForm);
    tabs=new QTabWidget;panelLayout->addWidget(tabs,1);
    auto behaviorPage=new QWidget;auto behaviorLayout=new QVBoxLayout(behaviorPage);
    behaviorLayout->setContentsMargins(12,14,12,12);
    behavior=new KeyBehaviorEditor;behavior->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);behaviorLayout->addWidget(behavior);
    mode=new QComboBox;
    mode->addItem("Follow keyboard mode", "");mode->addItem("Tap once", "Tap");
    mode->addItem("Repeat while selected", "Repeat");mode->addItem("Hold while selected", "Hold");mode->addItem("Toggle held / released", "Toggle");
    auto modeForm=new QFormLayout;modeForm->addRow("Activation",mode);behaviorLayout->addLayout(modeForm);
    auto modeHelp=new QLabel("Fixed activation modes require a single keyboard key or gamepad button.");modeHelp->setWordWrap(true);behaviorLayout->addWidget(modeHelp);behaviorLayout->addStretch();
    tabs->addTab(behaviorPage,"Behavior");
    auto appearancePage=new QWidget;auto appearanceForm=new QFormLayout(appearancePage);appearanceForm->setContentsMargins(12,14,12,12);
    shiftLabel=new QLineEdit;shiftLabel->setPlaceholderText("Optional label when Shift is active");appearanceForm->addRow("Shift label",shiftLabel);
    symbol=new QComboBox;symbol->addItem("No icon", "");symbol->setIconSize({28,28});symbol->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);symbol->setMinimumContentsLength(18);
    QFile icons(":/resources/icons.json");
    if(icons.open(QIODevice::ReadOnly)){
        auto entries=QJsonDocument::fromJson(icons.readAll()).object();
        for(auto it=entries.begin();it!=entries.end();++it){
            QString title=it.key();title.remove(QRegularExpression("Icon$"));title.replace(QRegularExpression("([a-z])([A-Z])"),"\\1 \\2");
            QString path=it.value().toString(),rule="evenodd";if(path.startsWith("F1")){rule="nonzero";path=path.mid(2);}else if(path.startsWith("F0"))path=path.mid(2);
            QSvgRenderer renderer(QString("<svg xmlns='http://www.w3.org/2000/svg'><path id='icon' fill='%1' fill-rule='%2' d='%3'/></svg>").arg(palette().color(QPalette::Text).name(),rule,path.toHtmlEscaped()).toUtf8());
            renderer.setViewBox(renderer.boundsOnElement("icon"));QPixmap pixmap(28,28);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);renderer.render(&painter);painter.end();
            symbol->addItem(QIcon(pixmap),title,it.key());
        }
    }
    appearanceForm->addRow("Icon",symbol);
    auto appearanceHint=new QLabel("Choose a label, an icon, or both. Existing pack colors and key groups are retained.");appearanceHint->setWordWrap(true);appearanceForm->addRow(appearanceHint);
    tabs->addTab(appearancePage,"Appearance");
    auto positionPage=new QWidget;auto positionForm=new QFormLayout(positionPage);positionForm->setContentsMargins(12,14,12,12);
    auto help=new QLabel("Drag a key on the canvas to move it. Use these fields for an exact position or size. Grid layouts snap to whole cells.");help->setWordWrap(true);positionForm->addRow(help);
    const QStringList titles{"X / column","Y / row","Width","Height"};
    for(int i=0;i<4;++i){geometry[i]=new QDoubleSpinBox;geometry[i]->setRange(0,100000);geometry[i]->setDecimals(3);positionForm->addRow(titles[i],geometry[i]);}
    tabs->addTab(positionPage,"Position & size");
    auto applyButton=new QPushButton("Apply key changes");applyButton->setMinimumHeight(38);
    connect(applyButton,&QPushButton::clicked,this,&KeyboardEditor::apply);
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(panel);scroll->setFrameShape(QFrame::NoFrame);auto side=new QWidget;auto sideLayout=new QVBoxLayout(side);sideLayout->setContentsMargins(0,0,12,12);sideLayout->addWidget(scroll,1);sideLayout->addWidget(applyButton);splitter->addWidget(side);splitter->setStretchFactor(0,1);splitter->setSizes({760,460});
    connect(scene,&QGraphicsScene::selectionChanged,this,[this]{auto list=scene->selectedItems();selected=list.isEmpty()?-1:list.first()->data(0).toInt();inspect();});
    for(auto field:{name,label,shiftLabel})connect(field,&QLineEdit::textEdited,this,&KeyboardEditor::markDirty);
    connect(behavior,&KeyBehaviorEditor::edited,this,&KeyboardEditor::markDirty);
    connect(mode,&QComboBox::activated,this,&KeyboardEditor::markDirty);
    connect(symbol,&QComboBox::activated,this,&KeyboardEditor::markDirty);
    for(auto spin:geometry)connect(spin,&QDoubleSpinBox::valueChanged,this,&KeyboardEditor::markDirty);
    newDocument();
}
KeyboardEditor::~KeyboardEditor() {
    disconnect(scene, nullptr, this, nullptr);
}
QList<QDomElement> KeyboardEditor::items() const {
    QList<QDomElement> result;auto content=document.documentElement().firstChildElement("Content");for(auto e=content.firstChildElement();!e.isNull();e=e.nextSiblingElement())result.append(e);return result;
}
bool KeyboardEditor::validate(const QDomDocument &doc,QVariantMap *result) {
    QTemporaryFile file;if(!file.open()){QMessageBox::warning(this,"Cannot validate",file.errorString());return false;}
    auto data=doc.toByteArray();if(file.write(data)!=data.size()||!file.flush())return false;
    auto parsed=KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
    if(parsed.contains("error")){QMessageBox::warning(this,"Invalid keyboard",parsed["error"].toString());return false;}
    if(result)*result=parsed;return true;
}
bool KeyboardEditor::commit(const QDomDocument &next) {
    QVariantMap parsed;if(!validate(next,&parsed))return false;
    document=next;layout=parsed;history=history.mid(0,historyIndex+1);history.append(document.toString());historyIndex=history.size()-1;render();return true;
}
void KeyboardEditor::newDocument() {
    document.setContent(QString("<Keyboard><Name>My keyboard</Name><Width>100%</Width><Height>40%</Height><Position>Bottom</Position><Canvas><Width>100</Width><Height>100</Height></Canvas><Content><DynamicKey X=\"5\" Y=\"5\" Width=\"15\" Height=\"15\"><Label>A</Label><Text>a</Text></DynamicKey></Content></Keyboard>"));
    history.clear();historyIndex=-1;selected=0;path.clear();saved.clear();commit(document);
}
bool KeyboardEditor::openFile(const QString &filePath) {
    // Apply resource limits before building or serializing a DOM tree.
    QByteArray source;auto checked=KeyboardPack::load(filePath,&source);
    if(checked.contains("error")){QMessageBox::warning(this,"Invalid keyboard",checked["error"].toString());return false;}
    QDomDocument next;auto parsed=next.setContent(source,QDomDocument::ParseOption::PreserveSpacingOnlyNodes);if(!parsed){QMessageBox::warning(this,"Invalid XML",QString("Line %1: %2").arg(parsed.errorLine).arg(parsed.errorMessage));return false;}
    if(!validate(next))return false;
    history.clear();historyIndex=-1;selected=-1;path=filePath;saved=next.toString();return commit(next);
}
void KeyboardEditor::render() {
    const int selection=selected;scene->blockSignals(true);scene->clear();
    const double sx=800/layout["cols"].toDouble(),sy=450/layout["rows"].toDouble();scene->setSceneRect(0,0,800,450);
    scene->setBackgroundBrush(QColor("#202936"));
    auto canvas=scene->addRect(0,0,800,450,QPen(QColor("#59697b"),1),QBrush(QColor("#293545")));canvas->setZValue(-1000);canvas->setAcceptedMouseButtons(Qt::NoButton);
    const double columnStep=layout["layoutMode"]=="grid"?sx:80;
    const double rowStep=layout["layoutMode"]=="grid"?sy:45;
    for(double x=columnStep;x<800;x+=columnStep)scene->addLine(x,0,x,450,QPen(QColor("#354355"),1))->setZValue(-999);
    for(double y=rowStep;y<450;y+=rowStep)scene->addLine(0,y,800,y,QPen(QColor("#354355"),1))->setZValue(-999);
    auto keys=layout["keys"].toList();
    for(int i=0;i<keys.size();++i){
        auto k=keys[i].toMap();
        auto item=new KeyItem(0,0,k["width"].toDouble()*sx,k["height"].toDouble()*sy);scene->addItem(item);
        item->setPos(k["col"].toDouble()*sx,k["row"].toDouble()*sy);
        auto style=k["style"].toMap();QColor background(style["backgroundcolor"].toString()),foreground(style["foregroundcolor"].toString());
        item->setBrush(background.isValid()?background:QColor("#e8edf3"));
        item->setFlags(QGraphicsItem::ItemIsSelectable|QGraphicsItem::ItemIsMovable);item->setData(0,i);item->setZValue(i);item->setFlag(QGraphicsItem::ItemClipsChildrenToShape);item->setSelected(i==selection);
        QString caption=k["label"].toString();
        if(caption.isEmpty()&&k["symbol"].toString().isEmpty())caption=k["kind"]=="DynamicKey"?"New key":k["kind"].toString();
        auto text=new QGraphicsSimpleTextItem(caption,item);QFont font;font.setPixelSize(qBound(10,int(item->rect().height()/3),22));text->setFont(font);text->setBrush(foreground.isValid()?foreground:QColor("#17212d"));
        text->setPos(qMax(5.0,(item->rect().width()-text->boundingRect().width())/2),qMax(4.0,(item->rect().height()-text->boundingRect().height())/2));
        int iconIndex=symbol->findData(k["symbol"].toString());
        if(iconIndex>0&&!symbol->itemIcon(iconIndex).isNull()){
            int size=qMax(8,qMin(36,int(qMin(item->rect().width(),item->rect().height())*0.45)));
            auto icon=new QGraphicsPixmapItem(symbol->itemIcon(iconIndex).pixmap(size,size),item);
            icon->setPos((item->rect().width()-size)/2,caption.isEmpty()?(item->rect().height()-size)/2:5);
            if(!caption.isEmpty())text->setY(qMax(double(size+7),item->rect().height()-text->boundingRect().height()-5));
        }
        item->moved=[this,i,sx,sy](QPointF p){QMetaObject::invokeMethod(this,[this,i,p,sx,sy]{moveKey(i,{p.x()/sx,p.y()/sy});},Qt::QueuedConnection);};
    }
    selected=selection;scene->blockSignals(false);name->setText(document.documentElement().firstChildElement("Name").text());inspect();
    setWindowTitle(QString("%1%2 — Keyboard editor").arg(path.isEmpty()?"Untitled":path,document.toString()==saved?"":" *"));
    statusBar()->showMessage(QString("%1 layout • %2 items • Drag to move; use properties to resize").arg(layout["layoutMode"].toString()).arg(keys.size()));
}
void KeyboardEditor::markDirty() {
    if(inspecting)return;
    formDirty=true;view->setInteractive(false);
    statusBar()->showMessage("Changes ready to apply · Click Apply key changes or Save.");
}
void KeyboardEditor::inspect() {
    inspecting=true;
    auto list=items();bool enabled=selected>=0&&selected<list.size();
    label->setEnabled(enabled);tabs->setEnabled(enabled);
    if(!enabled){selectionTitle->setText("Select a key to edit");label->clear();behavior->load({},path);formDirty=false;view->setInteractive(true);inspecting=false;return;}
    auto e=list[selected];label->setText(e.firstChildElement("Label").text());
    selectionTitle->setText(label->text().isEmpty()?"Key properties":label->text());
    selectionTitle->setTextFormat(Qt::PlainText);
    shiftLabel->setText(e.firstChildElement("ShiftDownLabel").text());
    QString symbolName=e.firstChildElement("Symbol").text().trimmed();int symbolIndex=symbol->findData(symbolName);
    if(symbolIndex<0){symbol->addItem(symbolName,symbolName);symbolIndex=symbol->count()-1;}symbol->setCurrentIndex(symbolIndex);
    auto k=layout["keys"].toList()[selected].toMap();
    mode->setCurrentIndex(qMax(0,mode->findData(k["mode"])));
    behavior->load(e,path);behavior->setEnabled(e.tagName()=="DynamicKey");mode->setEnabled(e.tagName()=="DynamicKey");
    QStringList fields{"col","row","width","height"};for(int i=0;i<4;++i)geometry[i]->setValue(k[fields[i]].toDouble());
    formDirty=false;view->setInteractive(true);inspecting=false;
}
bool KeyboardEditor::apply() {
    auto next=document.cloneNode(true).toDocument();setText(next,next.documentElement(),"Name",name->text());
    if(selected>=0&&selected<items().size()){
        auto e=next.documentElement().firstChildElement("Content").firstChildElement();for(int i=0;i<selected;++i)e=e.nextSiblingElement();
        setText(next,e,"Label",label->text());
        if(shiftLabel->text()!=e.firstChildElement("ShiftDownLabel").text())setText(next,e,"ShiftDownLabel",shiftLabel->text());
        if(symbol->currentData().toString()!=e.firstChildElement("Symbol").text().trimmed())setText(next,e,"Symbol",symbol->currentData().toString());
        if(e.tagName()=="DynamicKey"){
            auto error=behavior->validationError();if(!error.isEmpty()){QMessageBox::warning(this,"Check key behavior",error);return false;}
            behavior->writeTo(next,e);
            auto attrs=e.attributes();for(int i=attrs.size()-1;i>=0;--i)if(attrs.item(i).nodeName().compare("mode",Qt::CaseInsensitive)==0)e.removeAttribute(attrs.item(i).nodeName());
            if(!mode->currentData().toString().isEmpty())e.setAttribute("mode",mode->currentData().toString());
        }
        bool canvas=layout["layoutMode"]=="canvas";QStringList attrs{canvas?"X":"Col",canvas?"Y":"Row","Width","Height"};for(int i=0;i<4;++i)attribute(e,attrs[i],canvas?geometry[i]->value():std::round(geometry[i]->value()));
    }
    return commit(next);
}
void KeyboardEditor::moveKey(int index,QPointF p) {
    if(index<0||index>=items().size())return;auto next=document.cloneNode(true).toDocument();auto e=next.documentElement().firstChildElement("Content").firstChildElement();for(int i=0;i<index;++i)e=e.nextSiblingElement();bool canvas=layout["layoutMode"]=="canvas";auto k=layout["keys"].toList()[index].toMap();double x=qBound(0.0,canvas?p.x():std::round(p.x()),layout["cols"].toDouble()-k["width"].toDouble());double y=qBound(0.0,canvas?p.y():std::round(p.y()),layout["rows"].toDouble()-k["height"].toDouble());if(qFuzzyCompare(x+1,k["col"].toDouble()+1)&&qFuzzyCompare(y+1,k["row"].toDouble()+1)){render();return;}attribute(e,canvas?"X":"Col",x);attribute(e,canvas?"Y":"Row",y);if(!commit(next))render();
}
bool KeyboardEditor::save(bool choosePath) {
    if(formDirty&&!apply())return false;
    QString target=path;if(choosePath||target.isEmpty())target=QFileDialog::getSaveFileName(this,"Save keyboard",target.isEmpty()?"keyboard.xml":target,"Keyboard XML (*.xml)");if(target.isEmpty())return false;if(!validate(document))return false;QSaveFile file(target);auto bytes=document.toByteArray(2);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){QMessageBox::warning(this,"Cannot save",file.errorString());return false;}KeyboardPack::invalidate(target);path=target;saved=document.toString();render();return true;
}
bool KeyboardEditor::discard() {
    if(!formDirty&&document.toString()==saved)return true;auto answer=QMessageBox::question(this,"Unsaved keyboard","Save changes to this keyboard?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);return answer==QMessageBox::Discard||(answer==QMessageBox::Save&&save(false));
}
void KeyboardEditor::closeEvent(QCloseEvent *event){if(discard())event->accept();else event->ignore();}
void KeyboardEditor::restore(int offset){int index=historyIndex+offset;if(index<0||index>=history.size())return;historyIndex=index;document.setContent(history[index],QDomDocument::ParseOption::PreserveSpacingOnlyNodes);validate(document,&layout);selected=-1;render();}
