#include "KeyboardPack.h"
#include "KeyboardLayout.h"
#include "ActionRunner.h"
#include "ActionCatalog.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QDir>
#include <QXmlStreamReader>
#include <QSet>
#include <algorithm>
namespace {
bool caching=false;
struct CachedKeyboard { qint64 size; QDateTime modified; QVariantMap root; QByteArray source; };
QHash<QString,CachedKeyboard> cache;
QVariantMap node(QXmlStreamReader &xml) {
    QVariantMap result, attrs; QVariantList children; QString text;
    result["tag"]=xml.name().toString();
    for (const auto &a:xml.attributes()) attrs[a.name().toString().toLower()]=a.value().toString();
    result["attributes"]=attrs;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) break;
        if (xml.isStartElement()) children.append(node(xml));
        else if (xml.isCharacters()) text+=xml.text();
        else if (xml.isDTD() || xml.isEntityReference()) xml.raiseError("DTD/entities are not supported");
    }
    result["text"]=text; result["children"]=children; return result;
}
void inventory(const QVariantMap &n, QSet<QString> &tags, QSet<QString> &actions, QSet<QString> &links, int &keys) {
    auto tag=n["tag"].toString(); tags.insert(tag);
    if(tag=="Action") actions.insert(n["text"].toString().trimmed());
    if(tag=="ChangeKeyboard") links.insert(n["text"].toString().trimmed());
    if(tag=="DynamicKey"||tag=="Joystick") ++keys;
    for(const auto &c:n["children"].toList()) inventory(c.toMap(),tags,actions,links,keys);
}
void details(const QVariantMap &n,QSet<QString> &symbols,QSet<QString> &attributes){
    const auto tag=n["tag"].toString();
    if(tag=="Symbol")symbols.insert(n["text"].toString().trimmed());
    const auto attrs=n["attributes"].toMap();for(auto it=attrs.begin();it!=attrs.end();++it)attributes.insert(tag+"@"+it.key());
    for(const auto &child:n["children"].toList())details(child.toMap(),symbols,attributes);
}
QStringList sorted(const QSet<QString> &set) {auto l=set.values(); l.sort(); return l;}
}
bool KeyboardPack::cacheEnabled(){return caching;}
void KeyboardPack::setCacheEnabled(bool enabled){caching=enabled;if(!enabled)cache.clear();}
void KeyboardPack::invalidate(const QString &path){cache.remove(QFileInfo(path).absoluteFilePath());}
QVariantMap KeyboardPack::load(const QString &path,QByteArray *source) {
    if(source)source->clear();
    const QFileInfo info(path);
    const auto key=info.absoluteFilePath();
    const auto size=caching?info.size():0;
    const auto modified=caching?info.lastModified():QDateTime();
    if(caching){
        const auto it=cache.constFind(key);
        if(info.isFile()&&it!=cache.cend()&&it->size==size&&it->modified==modified){
            if(source)*source=it->source;
            return it->root;
        }
        cache.remove(key);
    }
    QFile f(path); if(!f.open(QIODevice::ReadOnly)) return {{"error",f.errorString()}};
    if(f.size()>4*1024*1024) return {{"error","Keyboard exceeds 4 MiB limit"}};
    const auto bytes=f.readAll();
    QXmlStreamReader xml(bytes); QVariantMap root;
    while(!xml.atEnd()) {
        xml.readNext();
        if(xml.isDTD() || xml.isEntityReference()) {xml.raiseError("DTD/entities are not supported"); break;}
        if(xml.isStartElement()) {root=node(xml); break;}
    }
    while(!xml.atEnd()) xml.readNext();
    if(xml.hasError()) return {{"error",QString("%1:%2: %3").arg(path).arg(xml.lineNumber()).arg(xml.errorString())}};
    if(root["tag"]!="Keyboard") return {{"error","Expected Keyboard root"}};
    if(source)*source=bytes;
    if(caching){
        // Do not cache a read if the file changed while it was being loaded.
        const QFileInfo after(path);
        if(after.isFile()&&after.size()==size&&after.lastModified()==modified&&bytes.size()==size)
            cache.insert(key,{size,modified,root,bytes});
    }
    return root;
}
QVariantMap KeyboardPack::audit(const QString &directory) {
    QDir dir(directory); auto names=dir.entryList({"*.xml"},QDir::Files,QDir::Name);
    QStringList errors; QSet<QString> tags,actions,links,unsupported; int keys=0;QVariantList layouts;
    for(const auto &name:names) {
        auto root=load(dir.filePath(name));
        if(root.contains("error")) errors<<root["error"].toString();
        else {
            inventory(root,tags,actions,links,keys);
            QSet<QString> fileTags,fileActions,fileLinks,symbols,attributes;int fileKeys=0;
            inventory(root,fileTags,fileActions,fileLinks,fileKeys);details(root,symbols,attributes);
            QVariantMap coverage;QStringList missing;
            for(const auto &action:fileActions){coverage[action]=ActionCatalog::implementation(action);if(!ActionCatalog::supported(action)){missing<<action;unsupported.insert(action);}}
            missing.sort();
            layouts<<QVariantMap{{"file",name},{"keyCount",fileKeys},{"actions",coverage},{"unsupportedActions",missing},
                {"symbols",sorted(symbols)},{"attributes",sorted(attributes)},{"links",sorted(fileLinks)}};
            auto layout=KeyboardLayout::fromXml(root);
            if(layout.contains("error"))errors<<name+": "+layout["error"].toString();
            else for(auto v:layout["keys"].toList()){auto error=ActionRunner::validate(v.toMap()["commands"].toList());if(!error.isEmpty())errors<<name+": "+error;}
        }
    }
    for(const auto &link:links) {
        if(link.contains('/') || link.contains('\\') || link.contains("..")) errors<<"Unsafe keyboard link: "+link;
        else if(!dir.exists(link.endsWith(".xml")?link:link+".xml")) errors<<"Missing keyboard link: "+link;
    }
    if(names.isEmpty()) errors<<"No XML keyboards found";
    return {{"files",names},{"fileCount",names.size()},{"keyCount",keys},{"tags",sorted(tags)},
            {"actions",sorted(actions)},{"links",sorted(links)},{"errors",errors},
            {"layouts",layouts},{"unsupportedActions",sorted(unsupported)},{"behavioralParityVerified",false},
            {"scope","XML, layout geometry, command validation and link resolution; advanced mouse actions and full behavioral parity remain pending"}};
}
