#include "ProfileStore.h"
#include "DwellGroups.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QRegularExpression>

namespace {
QJsonObject read(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool write(const QString &path, const QJsonObject &values, QString &error) {
    QSaveFile file(path);
    const auto bytes=QJsonDocument(values).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
        error="Could not save preferences: "+file.errorString();return false;
    }
    return true;
}
}
ProfileStore::ProfileStore(QString path):path(std::move(path)){}
QJsonObject ProfileStore::defaults(){
    QJsonArray items;
    for(const auto &label:QStringList{"suction","index","thumb","rotate","slide","shoulders","headrest","button","butt","earbud","deflate","inflate"})
        items.append(QJsonObject{{"label",label},{"phrase",label}});
    return {{"cacheKeyboards",false},{"transitionDurationMs",180},{"keyboardTransition","instant"},{"clickDurationMs",0},{"groupDwellTimes",QJsonObject{}},{"clampScrollGaze",true},{"cancelScrollOffscreen",false},{"showScrollZones",false},{"scrollZoneOpacity",30},{"scrollZoneColor","#00bfff"},{"keyMode","Tap"},{"buttonMode","Tap"},{"keyCycleModes",QJsonArray{"Tap","Repeat","Hold","Toggle"}},{"speechProfiles",QJsonArray{QJsonObject{{"name","Default"},{"items",items}}}},{"activeSpeechProfile",0},{"magneticMouseRelative",false},{"processPriority",0},{"predictionTextScale",100},{"scratchpadTextScale",100},{"removeCompletionSpaceBeforePunctuation",true},{"clampGazeForKeyboard",false},{"clampGazeForPointSelection",true},{"dwellGraceMs",0},{"showPausedDwellProgress",true},{"invalidGazeToleranceMs",500},{"scrollSpeed",100},{"scrollDeadzone",50},{"scrollAxis",0},{"gazeEnabled",true},{"repeatIntervalMs",750},{"preserveTogglesBetweenLayouts",false},{"cycleModifiersOnHover",false},{"hideKeyboardDuringMouseActions",false},{"gazeIndicatorSize",18},{"gazeIndicatorOpacity",100},{"gazeIndicatorColor","#00bfff"},{"gazeIndicatorMorph",false},{"version",1},{"restoreCorner",2},{"restoreWidth",64},{"restoreHeight",64},{"restoreOpacity",100},{"gazeFilterType",0},{"gazeFilterGain",0.2},{"gazeSaccadeDistance",0.1},{"gazeSmoothingLevel",1},{"smoothWhenChangingGazeTarget",false},{"dwellMs",750},{"latchStyle","outline"},{"lockStyle","fill"},{"dwellIndicatorStyle","bar"},{"literal",false},{"learning",false},{"typedWordSuggestionMode","fallback"},{"predictionMode","context"},{"bestEffortPredictionOnly",true},{"predictionUnicodeMode","nonAscii"},{"predictionLanguage","eng"},{"predictNextWord",true},{"addSpaceAfterCompletion",true},{"suggestSentenceStarters",true},
            {"systemKeyboard",true},{"startLayout","keyboard-alphabet-14x4-bottom"},{"keyboardScale",100},{"keyboardTheme","system"},{"shortLabelScale",150},{"longLabelScale",100},{"magnifierRefreshHz",30},{"magnifierZoom",4},{"magnifierEnabled",false},{"progressiveZoom",false},{"zoomDurationMs",1500},{"targetDwellEnabled",true},{"targetStartDwellMs",750},{"targetConfirmDwellMs",1000},{"targetProgressStyle","ring"},{"targetProgressSize",56},{"targetProgressThickness",5},{"targetProgressOpacity",100},{"targetProgressColor","#00bfff"},{"targetDwellRadius",40},{"rememberDesktop",false}};
}
bool ProfileStore::valid(const QJsonObject &v){
    if(v.contains("cacheKeyboards")&&!v["cacheKeyboards"].isBool())return false;
    if(v.contains("groupDwellTimes")){
        if(!v["groupDwellTimes"].isObject())return false;
        const auto groups=v["groupDwellTimes"].toObject();
        for(auto it=groups.begin();it!=groups.end();++it){
            const auto value=it.value();
            if(!dwellGroupNames().contains(it.key())||!value.isDouble()||value.toDouble()!=value.toInt()||value.toInt()<250||value.toInt()>3000)return false;
        }
    }
    for(const auto &name:QStringList{"keyMode","buttonMode"}){
        if(v.contains(name)&&(!v[name].isString()||!QStringList{"Tap","Repeat","Hold","Toggle"}.contains(v[name].toString())))return false;
    }
    if(v.contains("keyCycleModes")){
        if(!v["keyCycleModes"].isArray()||v["keyCycleModes"].toArray().isEmpty())return false;
        QStringList seen;
        for(const auto &entry:v["keyCycleModes"].toArray()){
            const auto mode=entry.toString();
            if(!entry.isString()||!QStringList{"Tap","Repeat","Hold","Toggle"}.contains(mode)||seen.contains(mode))return false;
            seen.append(mode);
        }
    }
    if(v.contains("speechProfiles")){
        if(!v["speechProfiles"].isArray()||v["speechProfiles"].toArray().isEmpty())return false;
        const auto profiles=v["speechProfiles"].toArray();
        const auto active=v["activeSpeechProfile"];
        if(!active.isDouble()||active.toDouble()!=active.toInt()||active.toInt()<0||active.toInt()>=profiles.size())return false;
        for(const auto &entry:profiles){
            if(!entry.isObject())return false;
            const auto profile=entry.toObject();
            if(!profile["name"].isString()||profile["name"].toString().trimmed().isEmpty()||!profile["items"].isArray()||profile["items"].toArray().size()>12)return false;
            for(const auto &item:profile["items"].toArray()){
                const auto object=item.toObject();
                if(!item.isObject()||!object["label"].isString()||!object["phrase"].isString()||object["label"].toString().trimmed().isEmpty()||object["phrase"].toString().trimmed().isEmpty())return false;
            }
        }
    }else if(v.contains("activeSpeechProfile"))return false;
    static const QRegularExpression layout("^[A-Za-z0-9_-]+$");
    return v["version"].isDouble()&&v["version"].toDouble()==1
        &&(!v.contains("gazeEnabled")||v["gazeEnabled"].isBool())
        &&(!v.contains("removeCompletionSpaceBeforePunctuation")||v["removeCompletionSpaceBeforePunctuation"].isBool())
        &&(!v.contains("clampGazeForKeyboard")||v["clampGazeForKeyboard"].isBool())
        &&(!v.contains("clampGazeForPointSelection")||v["clampGazeForPointSelection"].isBool())
        &&(!v.contains("invalidGazeToleranceMs")||(v["invalidGazeToleranceMs"].isDouble()&&v["invalidGazeToleranceMs"].toDouble()==v["invalidGazeToleranceMs"].toInt()&&v["invalidGazeToleranceMs"].toInt()>=0&&v["invalidGazeToleranceMs"].toInt()<=1000))
        &&(!v.contains("showPausedDwellProgress")||v["showPausedDwellProgress"].isBool())
        &&(!v.contains("dwellGraceMs")||(v["dwellGraceMs"].isDouble()&&v["dwellGraceMs"].toDouble()==v["dwellGraceMs"].toInt()&&v["dwellGraceMs"].toInt()>=0&&v["dwellGraceMs"].toInt()<=1000))
        &&(!v.contains("gazeIndicatorSize")||(v["gazeIndicatorSize"].isDouble()&&v["gazeIndicatorSize"].toDouble()==v["gazeIndicatorSize"].toInt()&&v["gazeIndicatorSize"].toInt()>=4&&v["gazeIndicatorSize"].toInt()<=120))
        &&(!v.contains("gazeIndicatorOpacity")||(v["gazeIndicatorOpacity"].isDouble()&&v["gazeIndicatorOpacity"].toDouble()==v["gazeIndicatorOpacity"].toInt()&&v["gazeIndicatorOpacity"].toInt()>=5&&v["gazeIndicatorOpacity"].toInt()<=100))
        &&(!v.contains("repeatIntervalMs")||(v["repeatIntervalMs"].isDouble()&&v["repeatIntervalMs"].toDouble()==v["repeatIntervalMs"].toInt()&&v["repeatIntervalMs"].toInt()>=100&&v["repeatIntervalMs"].toInt()<=2000))
        &&(!v.contains("preserveTogglesBetweenLayouts")||v["preserveTogglesBetweenLayouts"].isBool())
        &&(!v.contains("cycleModifiersOnHover")||v["cycleModifiersOnHover"].isBool())
        &&(!v.contains("hideKeyboardDuringMouseActions")||v["hideKeyboardDuringMouseActions"].isBool())
        &&(!v.contains("gazeIndicatorMorph")||v["gazeIndicatorMorph"].isBool())
        &&(!v.contains("gazeIndicatorColor")||(v["gazeIndicatorColor"].isString()&&QRegularExpression("^#[0-9a-fA-F]{6}$").match(v["gazeIndicatorColor"].toString()).hasMatch()))
        &&(!v.contains("latchStyle")||(v["latchStyle"].isString()&&QStringList{"outline","fill","underline","pulse","ripple","sweep","animated-outline"}.contains(v["latchStyle"].toString())))
        &&(!v.contains("lockStyle")||(v["lockStyle"].isString()&&QStringList{"outline","fill","underline","pulse","ripple","sweep","animated-outline"}.contains(v["lockStyle"].toString())))
        &&(!v.contains("dwellIndicatorStyle")||(v["dwellIndicatorStyle"].isString()&&QStringList{"bar","top-bar","dual-bar","center-bars","opposing-bars","fill","fill-down","fill-left","fill-right","outline","ring","pie","collapse","expand"}.contains(v["dwellIndicatorStyle"].toString())))
        &&v["dwellMs"].isDouble()&&v["dwellMs"].toDouble()==v["dwellMs"].toInt()
        &&v["dwellMs"].toInt()>=250&&v["dwellMs"].toInt()<=3000
        &&(!v.contains("gazeFilterType")||(v["gazeFilterType"].isDouble()&&(v["gazeFilterType"].toDouble()==0||v["gazeFilterType"].toDouble()==1)))
        &&(!v.contains("gazeFilterGain")||(v["gazeFilterGain"].isDouble()&&v["gazeFilterGain"].toDouble()>=0.01&&v["gazeFilterGain"].toDouble()<=1))
        &&(!v.contains("gazeSaccadeDistance")||(v["gazeSaccadeDistance"].isDouble()&&v["gazeSaccadeDistance"].toDouble()>=0.01&&v["gazeSaccadeDistance"].toDouble()<=1))
        &&(!v.contains("gazeSmoothingLevel")||(v["gazeSmoothingLevel"].isDouble()&&v["gazeSmoothingLevel"].toDouble()==v["gazeSmoothingLevel"].toInt()&&v["gazeSmoothingLevel"].toInt()>=0&&v["gazeSmoothingLevel"].toInt()<=4))
        &&(!v.contains("smoothWhenChangingGazeTarget")||v["smoothWhenChangingGazeTarget"].isBool())
        &&(!v.contains("restoreCorner")||(v["restoreCorner"].isDouble()&&v["restoreCorner"].toDouble()==v["restoreCorner"].toInt()&&v["restoreCorner"].toInt()>=0&&v["restoreCorner"].toInt()<=3))
        &&(!v.contains("restoreOpacity")||(v["restoreOpacity"].isDouble()&&v["restoreOpacity"].toDouble()==v["restoreOpacity"].toInt()&&v["restoreOpacity"].toInt()>=10&&v["restoreOpacity"].toInt()<=100))
        &&(!v.contains("restoreWidth")||(v["restoreWidth"].isDouble()&&v["restoreWidth"].toDouble()==v["restoreWidth"].toInt()&&v["restoreWidth"].toInt()>=40&&v["restoreWidth"].toInt()<=128))
        &&(!v.contains("restoreHeight")||(v["restoreHeight"].isDouble()&&v["restoreHeight"].toDouble()==v["restoreHeight"].toInt()&&v["restoreHeight"].toInt()>=40&&v["restoreHeight"].toInt()<=128))
        &&(!v.contains("restoreSize")||(v["restoreSize"].isDouble()&&v["restoreSize"].toDouble()==v["restoreSize"].toInt()&&v["restoreSize"].toInt()>=40&&v["restoreSize"].toInt()<=128))
        &&(!v.contains("processPriority")||(v["processPriority"].isDouble()&&v["processPriority"].toDouble()==v["processPriority"].toInt()&&v["processPriority"].toInt()>=0&&v["processPriority"].toInt()<=2))
        &&v["literal"].isBool()&&v["learning"].isBool()
        &&(!v.contains("systemKeyboard")||v["systemKeyboard"].isBool())
        &&(!v.contains("localImeLocale")||(v["localImeLocale"].isString()&&(v["localImeLocale"].toString().isEmpty()||QRegularExpression("^[A-Za-z]{2,3}([_-][A-Za-z0-9]{2,8})*$").match(v["localImeLocale"].toString()).hasMatch())))
        &&(!v.contains("typedWordSuggestionMode")||(v["typedWordSuggestionMode"].isString()&&QStringList{"always","fallback","never"}.contains(v["typedWordSuggestionMode"].toString())))
        &&(!v.contains("predictionLanguage")||(v["predictionLanguage"].isString()&&QStringList{"eng","spa","fra","deu","por","ita","nld","rus","ukr","pol","tur","vie","ind","ara","cmn","jpn","kor","tha"}.contains(v["predictionLanguage"].toString())))
        &&(!v.contains("bestEffortPredictionOnly")||v["bestEffortPredictionOnly"].isBool())
        &&(!v.contains("predictionUnicodeMode")||(v["predictionUnicodeMode"].isString()&&QStringList{"nonAscii","always"}.contains(v["predictionUnicodeMode"].toString())))
        &&(!v.contains("predictionMode")||(v["predictionMode"].isString()&&QStringList{"context","frequency","off"}.contains(v["predictionMode"].toString())))
        &&(!v.contains("suggestSentenceStarters")||v["suggestSentenceStarters"].isBool())
        &&(!v.contains("addSpaceAfterCompletion")||v["addSpaceAfterCompletion"].isBool())
        &&(!v.contains("predictNextWord")||v["predictNextWord"].isBool())
        &&(!v.contains("keyboardScale")||(v["keyboardScale"].isDouble()&&v["keyboardScale"].toDouble()==v["keyboardScale"].toInt()&&v["keyboardScale"].toInt()>=50&&v["keyboardScale"].toInt()<=150))
        &&(!v.contains("transitionDurationMs")||(v["transitionDurationMs"].isDouble()&&v["transitionDurationMs"].toDouble()==v["transitionDurationMs"].toInt()&&v["transitionDurationMs"].toInt()>=50&&v["transitionDurationMs"].toInt()<=1000))
        &&(!v.contains("keyboardTransition")||(v["keyboardTransition"].isString()&&QStringList{"instant","fade","lift","slide","zoom","rows"}.contains(v["keyboardTransition"].toString())))
        &&(!v.contains("keyboardTheme")||(v["keyboardTheme"].isString()&&QStringList{"system","midnight","ocean","forest","paper","pink","purple","orange","yellow","high-contrast"}.contains(v["keyboardTheme"].toString())))
        &&(!v.contains("highContrast")||v["highContrast"].isBool())
        &&(!v.contains("liveZoom")||v["liveZoom"].isBool())
        &&(!v.contains("progressiveZoom")||v["progressiveZoom"].isBool())
        &&(!v.contains("targetDwellEnabled")||v["targetDwellEnabled"].isBool())
        &&(!v.contains("targetStartDwellMs")||(v["targetStartDwellMs"].isDouble()&&v["targetStartDwellMs"].toDouble()==v["targetStartDwellMs"].toInt()&&v["targetStartDwellMs"].toInt()>=250&&v["targetStartDwellMs"].toInt()<=3000))
        &&(!v.contains("targetConfirmDwellMs")||(v["targetConfirmDwellMs"].isDouble()&&v["targetConfirmDwellMs"].toDouble()==v["targetConfirmDwellMs"].toInt()&&v["targetConfirmDwellMs"].toInt()>=250&&v["targetConfirmDwellMs"].toInt()<=3000))
        &&(!v.contains("targetProgressSize")||(v["targetProgressSize"].isDouble()&&v["targetProgressSize"].toDouble()==v["targetProgressSize"].toInt()&&v["targetProgressSize"].toInt()>=24&&v["targetProgressSize"].toInt()<=160))
        &&(!v.contains("targetProgressThickness")||(v["targetProgressThickness"].isDouble()&&v["targetProgressThickness"].toDouble()==v["targetProgressThickness"].toInt()&&v["targetProgressThickness"].toInt()>=1&&v["targetProgressThickness"].toInt()<=12))
        &&(!v.contains("targetProgressOpacity")||(v["targetProgressOpacity"].isDouble()&&v["targetProgressOpacity"].toDouble()==v["targetProgressOpacity"].toInt()&&v["targetProgressOpacity"].toInt()>=5&&v["targetProgressOpacity"].toInt()<=100))
        &&(!v.contains("targetProgressStyle")||(v["targetProgressStyle"].isString()&&QStringList{"ring","pie","shrink"}.contains(v["targetProgressStyle"].toString())))
        &&(!v.contains("targetProgressColor")||(v["targetProgressColor"].isString()&&QRegularExpression("^#[0-9a-fA-F]{6}$").match(v["targetProgressColor"].toString()).hasMatch()))
        &&(!v.contains("targetDwellRadius")||(v["targetDwellRadius"].isDouble()&&v["targetDwellRadius"].toDouble()==v["targetDwellRadius"].toInt()&&v["targetDwellRadius"].toInt()>=10&&v["targetDwellRadius"].toInt()<=100))
        &&(!v.contains("zoomDurationMs")||(v["zoomDurationMs"].isDouble()&&v["zoomDurationMs"].toDouble()==v["zoomDurationMs"].toInt()&&v["zoomDurationMs"].toInt()>=500&&v["zoomDurationMs"].toInt()<=5000))
        &&(!v.contains("enlargeShortLabels")||v["enlargeShortLabels"].isBool())
        &&(!v.contains("magnifierRefreshHz")||(v["magnifierRefreshHz"].isDouble()&&(v["magnifierRefreshHz"].toDouble()==0||v["magnifierRefreshHz"].toDouble()==15||v["magnifierRefreshHz"].toDouble()==30||v["magnifierRefreshHz"].toDouble()==45||v["magnifierRefreshHz"].toDouble()==60)))
        &&(!v.contains("magnifierEnabled")||v["magnifierEnabled"].isBool())
        &&(!v.contains("clickDurationMs")||(v["clickDurationMs"].isDouble()&&v["clickDurationMs"].toDouble()==v["clickDurationMs"].toInt()&&v["clickDurationMs"].toInt()>=0&&v["clickDurationMs"].toInt()<=250))
        &&(!v.contains("magneticMouseRelative")||v["magneticMouseRelative"].isBool())
        &&(!v.contains("showScrollZones")||v["showScrollZones"].isBool())
        &&(!v.contains("scrollZoneOpacity")||(v["scrollZoneOpacity"].isDouble()&&v["scrollZoneOpacity"].toDouble()==v["scrollZoneOpacity"].toInt()&&v["scrollZoneOpacity"].toInt()>=5&&v["scrollZoneOpacity"].toInt()<=100))
        &&(!v.contains("scrollZoneColor")||(v["scrollZoneColor"].isString()&&QRegularExpression("^#[0-9a-fA-F]{6}$").match(v["scrollZoneColor"].toString()).hasMatch()))
        &&(!v.contains("scrollSpeed")||(v["scrollSpeed"].isDouble()&&v["scrollSpeed"].toDouble()==v["scrollSpeed"].toInt()&&v["scrollSpeed"].toInt()>=10&&v["scrollSpeed"].toInt()<=300))
        &&(!v.contains("scrollDeadzone")||(v["scrollDeadzone"].isDouble()&&v["scrollDeadzone"].toDouble()==v["scrollDeadzone"].toInt()&&v["scrollDeadzone"].toInt()>=10&&v["scrollDeadzone"].toInt()<=200))
        &&(!v.contains("clampScrollGaze")||v["clampScrollGaze"].isBool())
        &&(!v.contains("cancelScrollOffscreen")||v["cancelScrollOffscreen"].isBool())
        &&(!v.contains("scrollAxis")||(v["scrollAxis"].isDouble()&&v["scrollAxis"].toDouble()==v["scrollAxis"].toInt()&&v["scrollAxis"].toInt()>=0&&v["scrollAxis"].toInt()<=2))
        &&(!v.contains("magnifierZoom")||(v["magnifierZoom"].isDouble()&&v["magnifierZoom"].toDouble()==v["magnifierZoom"].toInt()&&v["magnifierZoom"].toInt()>=2&&v["magnifierZoom"].toInt()<=8))
        &&(!v.contains("shortLabelScale")||(v["shortLabelScale"].isDouble()&&v["shortLabelScale"].toDouble()==v["shortLabelScale"].toInt()&&v["shortLabelScale"].toInt()>=50&&v["shortLabelScale"].toInt()<=200))
        &&(!v.contains("longLabelScale")||(v["longLabelScale"].isDouble()&&v["longLabelScale"].toDouble()==v["longLabelScale"].toInt()&&v["longLabelScale"].toInt()>=50&&v["longLabelScale"].toInt()<=200))
        &&(!v.contains("rememberDesktop")||v["rememberDesktop"].isBool())
        &&(!v.contains("predictionTextScale")||(v["predictionTextScale"].isDouble()&&v["predictionTextScale"].toDouble()==v["predictionTextScale"].toInt()&&v["predictionTextScale"].toInt()>=50&&v["predictionTextScale"].toInt()<=300))
        &&(!v.contains("scratchpadTextScale")||(v["scratchpadTextScale"].isDouble()&&v["scratchpadTextScale"].toDouble()==v["scratchpadTextScale"].toInt()&&v["scratchpadTextScale"].toInt()>=50&&v["scratchpadTextScale"].toInt()<=300))
        &&v["startLayout"].isString()&&layout.match(v["startLayout"].toString()).hasMatch();
}
ProfileStore::Loaded ProfileStore::load()const{
    auto current=read(path);
    if(valid(current))return {current,{}};
    if(current["version"].toDouble()>1)
        return {defaults(),"Preferences use a newer version. Using defaults; reset preferences to enable saving."};
    auto backup=read(path+".bak");
    if(valid(backup))return {backup,"Recovered preferences from the last known good copy."};
    return {defaults(),QFile::exists(path)||QFile::exists(path+".bak")
        ?"Preferences could not be read. Using defaults.":QString()};
}
bool ProfileStore::save(const QJsonObject &values,QString &error,bool reset)const{
    error.clear();
    if(!valid(values)){error="Invalid preferences";return false;}
    auto current=read(path);
    if(!reset&&current["version"].toDouble()>1){error="Newer preferences were preserved. Reset preferences to enable saving.";return false;}
    if(!QDir().mkpath(QFileInfo(path).absolutePath())){error="Could not create preferences directory";return false;}
    // Never replace a valid backup with corrupt input. On first save, seed recovery.
    auto backup=reset?values:valid(current)?current:read(path+".bak");
    if(!valid(backup))backup=values;
    if(!write(path+".bak",backup,error))return false;
    return write(path,values,error);
}
