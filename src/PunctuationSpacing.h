#pragma once
#include <QString>

namespace PredictionLanguage {
struct PunctuationSpacing {
 bool removeSpace=false;
 QString before,after;
};

// Applies only to an owned completion space, never to user-entered whitespace.
// A language code without a region deliberately retains the existing French
// colon convention rather than guessing European or Canadian typography.
inline PunctuationSpacing punctuationSpacing(const QString &text,const QString &language,
                                             QString before,const QString &after={}){
 if(text.size()!=1)return {};
 const auto ch=text.front();
 if(QStringLiteral("&-/+=*<>|").contains(ch))return {};
 if(ch.category()==QChar::Punctuation_Open||ch.category()==QChar::Punctuation_InitialQuote)return {};
 // Straight double quotes have no reliable opening/closing role here.
 if(ch=='"')return {};
 if(!ch.isPunct()&&!(ch.unicode()<128&&ch.isSymbol()))return {};
 if(before.endsWith(' '))before.chop(1);
 if(before.isEmpty())return {};
 // Do not guess about a decimal point between digits.
 if((ch=='.'||ch==',')&&before.back().isDigit()&&!after.isEmpty()&&after.front().isDigit())return {};
 PunctuationSpacing result;result.removeSpace=true;
 if(language=="fra"&&ch==':')result.before=" ";
 const bool nativePunctuation=(ch.unicode()>=0x3000&&ch.unicode()<=0x303f)
     ||(ch.unicode()>=0xff00&&ch.unicode()<=0xff65);
 const bool closing=ch.category()==QChar::Punctuation_Close||ch.category()==QChar::Punctuation_FinalQuote;
 const bool apostrophe=ch=='\''||ch==QChar(0x2019);
 if(language!="cmn"&&language!="jpn"&&language!="tha"&&!nativePunctuation
    &&!closing&&!apostrophe&&after.isEmpty())result.after=" ";
 return result;
}
}
