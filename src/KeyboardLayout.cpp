#include "KeyboardLayout.h"
#include "DwellGroups.h"
#include "ActionRunner.h"
#include "KeyboardPack.h"
#include "KeyNames.h"
#include <QDomDocument>
#include <QFile>
#include <QSaveFile>
#include "GameController.h"
#include <QSet>
#include <cmath>
namespace {
QString field(const QVariantMap &node,const QString &tag,QString fallback={}){for(auto v:node["children"].toList()){auto n=v.toMap();if(n["tag"]==tag)return n["text"].toString();}return fallback;}
}
QVariantMap KeyboardLayout::fromXml(const QVariantMap &root){
    if(root.contains("error"))return root;
    QVariantMap layout;QVariantList keys,groups;double rows=0,cols=0;bool canvas=false,grid=false;
    for(auto v:root["children"].toList()){auto n=v.toMap();auto tag=n["tag"].toString();
        if(tag=="Canvas"){canvas=true;rows=field(n,"Height").toDouble();cols=field(n,"Width").toDouble();}
        else if(tag=="Grid"){grid=true;rows=field(n,"Rows").toInt();cols=field(n,"Cols").toInt();}
        else if(tag=="KeyGroup")groups<<n["attributes"];
        else if(tag!="Content")layout[tag.toLower()]=n["text"].toString().trimmed();
    }
    if(canvas==grid||!std::isfinite(rows)||!std::isfinite(cols)||rows<=0||cols<=0||(!canvas&&(rows>64||cols>64)))return {{"error","Invalid keyboard Grid or Canvas"}};
    layout["activateoverlappingkeys"]=canvas&&layout["activateoverlappingkeys"].toString().compare("true",Qt::CaseInsensitive)==0;
    QSet<QString> occupied;
    for(auto v:root["children"].toList()){auto content=v.toMap();if(content["tag"]!="Content")continue;
        for(auto entry:content["children"].toList()){
            auto n=entry.toMap(),attr=n["attributes"].toMap();auto tag=n["tag"].toString();
            if(tag!="Joystick"&&tag!="DynamicKey"&&tag!="SuggestionRow"&&tag!="Scratchpad")return {{"error","Unsupported content item: "+tag}};
            if(tag=="Joystick"&&attr.value("stick","Left").toString()!="Left"&&attr.value("stick").toString()!="Right")return {{"error","Joystick stick must be Left or Right"}};
            bool rowOk=false,colOk=false;double row=attr.value("row").toString().toInt(&rowOk),col=attr.value("col").toString().toInt(&colOk);
            double width=attr.value("width",1).toInt(),height=attr.value("height",1).toInt();
            int suggestions=0;
            if(canvas){
                bool widthOk=false,heightOk=false;
                row=attr.value("y").toString().toDouble(&rowOk);
                col=attr.value("x").toString().toDouble(&colOk);
                width=attr.value("width").toString().toDouble(&widthOk);
                height=attr.value("height").toString().toDouble(&heightOk);
                if(attr.contains("row")||attr.contains("col")||!rowOk||!colOk||!widthOk||!heightOk||
                   !std::isfinite(row)||!std::isfinite(col)||!std::isfinite(width)||!std::isfinite(height)||
                   row<0||col<0||width<=0||height<=0||row+height>rows||col+width>cols)
                    return {{"error","Invalid key rectangle in Canvas"}};
                if(tag=="SuggestionRow"){
                    bool ok=false;suggestions=attr.value("count",4).toString().toInt(&ok);
                    if(!ok||suggestions<1||suggestions>64)return {{"error","Invalid suggestion Count"}};
                }
            }else{
                // Unpositioned items flow into the first free rectangle in row-major order.
                if(!attr.contains("row")&&!attr.contains("col")&&width>0&&height>0&&width<=cols&&height<=rows){
                    for(int y=0;y<=rows-height&&!rowOk;++y)for(int x=0;x<=cols-width&&!rowOk;++x){
                        bool free=true;
                        for(int dy=0;dy<height;++dy)for(int dx=0;dx<width;++dx)
                            if(occupied.contains(QString("%1:%2").arg(y+dy).arg(x+dx)))free=false;
                        if(free){row=y;col=x;rowOk=colOk=true;}
                    }
                }
                if(!rowOk||!colOk||row<0||col<0||width<1||height<1||row+height>rows||col+width>cols)return {{"error","Key outside keyboard grid"}};
                for(int y=row;y<row+height;++y)for(int x=col;x<col+width;++x){auto cell=QString("%1:%2").arg(y).arg(x);if(occupied.contains(cell))return {{"error","Overlapping keyboard cells"}};occupied.insert(cell);}
            }
            if(!canvas&&tag=="SuggestionRow")suggestions=int(width)/2;
            QVariantMap style{{"backgroundcolor","#f7f8fa"},{"foregroundcolor","#17212d"},{"foregroundopacity",1.0},{"opacity",1.0}};
            QSet<QString> memberships;for(auto c:n["children"].toList()){auto child=c.toMap();if(child["tag"]=="KeyGroup")memberships.insert(child["text"].toString().trimmed());}
            for(auto group:groups){auto g=group.toMap();if(g["name"].toString().compare("All",Qt::CaseInsensitive)==0||g["name"].toString().compare("Default",Qt::CaseInsensitive)==0||memberships.contains(g["name"].toString()))for(auto it=g.begin();it!=g.end();++it)if(it.key()!="name")style[it.key()]=it.value();}
            for(auto it=attr.begin();it!=attr.end();++it)style[it.key()]=it.value();
            QVariantList commands;
            static const QSet<QString> commandTags{"Action","Text","KeyDown","KeyUp","KeyToggle","Wait","ChangeKeyboard"};
            for(auto c:n["children"].toList()){auto child=c.toMap();if(commandTags.contains(child["tag"].toString())){
                if(child["tag"]=="Action"&&child["text"].toString().trimmed()=="SpeakLabel")child["label"]=field(n,"Label");
                commands<<child;
            }}
            // Physical identity is separate from the personal label and literal text.
            const auto physical=field(n,"PhysicalKey").trimmed();
            int physicalFields=0;for(const auto &child:n["children"].toList())if(child.toMap()["tag"]=="PhysicalKey")++physicalFields;
            if(physicalFields>1||(physicalFields==1&&physical.isEmpty()))return {{"error","PhysicalKey must name one printable US key"}};
            if(!physical.isEmpty()){
                const auto name=KeyNames::canonical(physical);
                QString systemText;
                for(int ch=32;ch<127;++ch)if(KeyNames::usCharacter(QChar(ch))==qMakePair(name,false)){systemText=QChar(ch);break;}
                if(tag!="DynamicKey"||commands.size()!=1||commands.front().toMap()["tag"]!="Text"||systemText.isEmpty())
                    return {{"error","PhysicalKey requires one Text command and a printable US key name"}};
                auto command=commands.front().toMap();command["physicalKey"]=name;command["systemText"]=systemText;commands={command};
            }
            QString mode;
            if(attr.contains("mode")){
                const auto value=attr["mode"].toString().trimmed().toLower();
                const QMap<QString,QString> modes{{"tap","Tap"},{"repeat","Repeat"},{"hold","Hold"},{"toggle","Toggle"}};
                if(!modes.contains(value))return {{"error","Key mode must be Tap, Repeat, Hold or Toggle"}};
                mode=modes.value(value);
                const auto command=commands.size()==1?commands.front().toMap():QVariantMap();
                const bool button=command["tag"]=="Action"&&!GameController::buttonAction(command["text"].toString(),"Press").isEmpty();
                if(tag!="DynamicKey"||(!button&&ActionRunner::dwellKey(commands).first.isEmpty()))
                    return {{"error","Key mode requires a single non-modifier keyboard key or digital gamepad button"}};
            }
            QString dwellGroup;
            for(const auto &name:dwellGroupNames())if(memberships.contains(name))dwellGroup=name;
            int dwell=style.value("completiontimes",0).toString().split(',').first().toInt();
            keys<<QVariantMap{{"dwellGroup",dwellGroup},{"individualDwell",attr.contains("completiontimes")},{"mode",mode},{"suggestionCount",suggestions},{"row",row},{"col",col},{"width",width},{"height",height},{"kind",tag},{"label",field(n,"Label")},{"shiftLabel",field(n,"ShiftDownLabel")},{"symbol",field(n,"Symbol").trimmed()},{"style",style},{"commands",commands},{"dwell",dwell}};
        }
    }
    layout["layoutMode"]=canvas?"canvas":"grid";layout["rows"]=rows;layout["cols"]=cols;layout["keys"]=keys;return layout;
}

bool KeyboardLayout::saveCanvasGeometry(const QString &path,QByteArray &source,const QVariantList &keys,QString &error){
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){error=file.errorString();return false;}
    const auto current=file.readAll();file.close();
    if(current!=source){error="The keyboard XML changed on disk. Reload it before saving.";return false;}
    const auto original=fromXml(KeyboardPack::load(path));
    if(original.contains("error")||original["layoutMode"]!="canvas"||original["keys"].toList().size()!=keys.size()){
        error="The source is not a matching Canvas keyboard.";return false;
    }
    QDomDocument doc;
    if(!doc.setContent(current,QDomDocument::ParseOption::PreserveSpacingOnlyNodes)){error="Cannot parse keyboard XML.";return false;}
    auto content=doc.documentElement().firstChildElement("Content");int index=0;
    for(auto element=content.firstChildElement();!element.isNull();element=element.nextSiblingElement(),++index){
        if(index>=keys.size()){error="Keyboard content no longer matches.";return false;}
        const auto key=keys[index].toMap();
        const double x=key["col"].toDouble(),y=key["row"].toDouble(),w=key["width"].toDouble(),h=key["height"].toDouble();
        if(element.tagName()!=key["kind"].toString()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||
           x<0||y<0||w<=0||h<=0||x+w>original["cols"].toDouble()||y+h>original["rows"].toDouble()){
            error="Invalid key geometry; XML was not changed.";return false;
        }
        const QMap<QString,double> geometry{{"X",x},{"Y",y},{"Width",w},{"Height",h}};
        for(auto it=geometry.begin();it!=geometry.end();++it){
            // Preserve the original attribute spelling when possible.
            QString name=it.key();const auto attributes=element.attributes();
            for(int i=0;i<attributes.size();++i)if(attributes.item(i).nodeName().compare(name,Qt::CaseInsensitive)==0){name=attributes.item(i).nodeName();break;}
            element.setAttribute(name,QString::number(it.value(),'g',17));
        }
    }
    if(index!=keys.size()){error="Keyboard content no longer matches.";return false;}
    const auto bytes=doc.toByteArray(-1);
    QSaveFile output(path);
    if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit()){error=output.errorString();return false;}
    source=bytes;error.clear();return true;
}
