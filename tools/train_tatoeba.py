#!/usr/bin/env python3
"""Download Tatoeba exports and train bounded, reproducible multilingual n-grams."""
import argparse, bz2, collections, datetime, hashlib, json, re, struct, unicodedata, urllib.request, zlib
from pathlib import Path
import word_segmentation
LANGUAGES = {"spa":"Spanish", "fra":"French", "deu":"German", "por":"Portuguese", "ita":"Italian", "nld":"Dutch", "rus":"Russian", "ukr":"Ukrainian", "pol":"Polish", "tur":"Turkish", "vie":"Vietnamese", "ind":"Indonesian", "ara":"Arabic", "cmn":"Chinese", "jpn":"Japanese", "kor":"Korean", "tha":"Thai"}
ROOT = Path(__file__).resolve().parents[1]
CHARACTER_LANGUAGES={"jpn"}
def tokens(text, code="eng", normalize_case=True):
    words=[]; word=""
    normalized=unicodedata.normalize("NFC", text.replace("\u2019", "'"))
    if code=="tur" and normalize_case: normalized=normalized.replace("I", "\u0131").replace("\u0130", "i")
    if normalize_case: normalized=normalized.lower()
    for ch in normalized:
        if unicodedata.category(ch)[0] in "LM" or (ch=="'" and word): word+=ch
        else:
            if word.strip("'"): words.append(word.strip("'"))
            word=""
    if word.strip("'"): words.append(word.strip("'"))
    if code in word_segmentation.LANGUAGES:
        return [token for word in words for token in word_segmentation.segment(word,code)]
    if code in CHARACTER_LANGUAGES:
        clusters=[]
        for word in words:
            for ch in word:
                if unicodedata.category(ch)[0]=="L": clusters.append(ch)
                elif unicodedata.category(ch)[0]=="M" and clusters: clusters[-1]+=ch
        return clusters
    return words

def train(code, cache, output, maximum):
    url=f"https://downloads.tatoeba.org/exports/per_language/{code}/{code}_sentences.tsv.bz2"
    source=cache/f"{code}.tsv.bz2"
    if not source.exists():
        temporary=source.with_suffix(".part")
        print(f"Downloading {code}",flush=True)
        with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as target:
            import shutil
            shutil.copyfileobj(response,target)
        temporary.replace(source)
    counts=[collections.Counter() for _ in range(4)]; casing=collections.defaultdict(collections.Counter);seen=set(); held=[]; training=0
    with bz2.open(source,"rt",encoding="utf-8") as stream:
        for line in stream:
            fields=line.rstrip("\n").split("\t",2)
            if len(fields)!=3 or fields[1]!=code: continue
            ident,_,text=fields
            words=tokens(text,code)
            if not 2<=len(words)<=120 or any(len(w)>48 for w in words): continue
            key=" ".join(words); fingerprint=hashlib.sha256(key.encode()).digest()
            if fingerprint in seen: continue
            seen.add(fingerprint)
            if int.from_bytes(fingerprint[:4],"big")%20==0:
                if len(held)<1000: held.append({"id":ident,"words":words})
                continue
            for sentence in re.split(r"[.!?;:\n\u3002\uff01\uff1f\u061b\u061f]+",text):
                history=["<s>"]*3
                surface=tokens(sentence,code,False) if code=="deu" else []
                for position,word in enumerate(tokens(sentence,code)):
                    if code=="deu" and position>0: casing[word][surface[position]]+=1
                    counts[0][word]+=1
                    for n in range(1,4): counts[n][tuple(history[-n:]+[word])]+=1
                    history.append(word)
            training+=1
            if training>=maximum: break
    if not counts[0]: raise ValueError(f"No training data for {code}")
    minimum=1 if code in {"deu","tur"} else 2
    vocabulary={word for word,count in counts[0].most_common(100000 if minimum==1 else 50000) if count>=minimum}
    total=sum(counts[0][w] for w in vocabulary)
    rows=["gaze-grid-prediction-v1"]+[f"u\t{w}\t{counts[0][w]/total:.9g}" for w in sorted(vocabulary)]
    forms=0
    for word,variants in sorted(casing.items()):
        form,count=variants.most_common(1)[0]
        if word in vocabulary and form!=word and count>=2 and count/sum(variants.values())>=0.6:
            rows.append(f"c\t{word}\t{form}");forms+=1
    sizes={"unigrams":len(vocabulary),"case_forms":forms}
    for n,tag,cap in [(1,"b",80000),(2,"t",100000),(3,"q",80000)]:
        selected=sorted(((key,c) for key,c in counts[n].items() if c>=3 and all(w=="<s>" or w in vocabulary for w in key)),key=lambda x:(-x[1],x[0]))[:cap]
        sizes[tag]=len(selected)
        rows += [tag+"\t"+"\t".join(key)+"\t"+str(c) for key,c in sorted(selected)]
    raw=("\n".join(rows)+"\n").encode(); data=struct.pack(">I",len(raw))+zlib.compress(raw,9)
    (output/f"{code}-v1.bin").write_bytes(data)
    (cache/f"{code}-heldout.jsonl").write_text("".join(json.dumps(row,ensure_ascii=False)+"\n" for row in held),encoding="utf-8")
    metadata={"language":code,"tokenization":"segmented-word" if code in word_segmentation.LANGUAGES else "character" if code in CHARACTER_LANGUAGES else "word","name":LANGUAGES[code],"source":url,"license":"CC BY 2.0 FR","attribution":"Tatoeba contributors (https://tatoeba.org)","retrieved_utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),"source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),"model_sha256":hashlib.sha256(data).hexdigest(),"training_sentences":training,"heldout_sentences":len(held),"maximum_training_sentences":maximum,"compressed_bytes":len(data),"counts":sizes,"split":"SHA256 of normalized tokens, first 4 bytes modulo 20; zero held out; exact duplicates removed","selection":"Export order, first maximum eligible training sentences","normalization":"NFC, "+("Turkish lowercase" if code=="tur" else "lowercase")+", letters and combining marks, internal apostrophes; sentence punctuation resets context"}
    metadata["minimum_word_count"]=minimum
    metadata["maximum_vocabulary"]=100000 if minimum==1 else 50000
    if code=="deu": metadata["casing"]="Most common non-sentence-initial surface; minimum 2 observations and 60 percent share"
    if code in word_segmentation.LANGUAGES:
        metadata["lexicon_sha256"]=hashlib.sha256((ROOT/f"resources/prediction/{code}-lexicon-v1.bin").read_bytes()).hexdigest()
        metadata["segmentation"]="Minimum integer-cost lexicon path; unknown letter plus attached marks costs 30000; ties prefer longer token"
    (output/f"{code}-manifest.json").write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"Trained {code}: {training} sentences, {len(vocabulary)} words, {len(data)} bytes",flush=True)
    return metadata

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--languages",nargs="+",choices=LANGUAGES,default=list(LANGUAGES));parser.add_argument("--cache",type=Path,default=ROOT/"build/tatoeba");parser.add_argument("--output",type=Path,default=ROOT/"resources/prediction");parser.add_argument("--max-sentences",type=int,default=200000);args=parser.parse_args()
    if args.max_sentences<1: parser.error("--max-sentences must be positive")
    args.cache.mkdir(parents=True,exist_ok=True);args.output.mkdir(parents=True,exist_ok=True)
    for code in args.languages: train(code,args.cache,args.output,args.max_sentences)
if __name__=="__main__": main()
