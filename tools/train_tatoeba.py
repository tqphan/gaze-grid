#!/usr/bin/env python3
"""Download Tatoeba exports and train bounded, reproducible multilingual n-grams."""
import argparse, bz2, collections, datetime, hashlib, json, re, struct, unicodedata, urllib.request, zlib
from pathlib import Path
LANGUAGES = {"spa":"Spanish", "fra":"French", "deu":"German", "por":"Portuguese", "ita":"Italian", "nld":"Dutch", "rus":"Russian", "ukr":"Ukrainian", "pol":"Polish", "tur":"Turkish", "vie":"Vietnamese", "ind":"Indonesian", "ara":"Arabic", "cmn":"Chinese", "jpn":"Japanese", "kor":"Korean", "tha":"Thai"}
ROOT = Path(__file__).resolve().parents[1]
CHARACTER_LANGUAGES={"cmn", "jpn", "tha"}
def tokens(text, code="eng"):
    words=[]; word=""
    for ch in unicodedata.normalize("NFC", text.replace("\u2019", "'")).lower():
        if unicodedata.category(ch)[0] in "LM" or (ch=="'" and word): word+=ch
        else:
            if word.strip("'"): words.append(word.strip("'"))
            word=""
    if word.strip("'"): words.append(word.strip("'"))
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
    counts=[collections.Counter() for _ in range(4)]; seen=set(); held=[]; training=0
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
                for word in tokens(sentence,code):
                    counts[0][word]+=1
                    for n in range(1,4): counts[n][tuple(history[-n:]+[word])]+=1
                    history.append(word)
            training+=1
            if training>=maximum: break
    if not counts[0]: raise ValueError(f"No training data for {code}")
    vocabulary={word for word,count in counts[0].most_common(50000) if count>=2}
    total=sum(counts[0][w] for w in vocabulary)
    rows=["gaze-grid-prediction-v1"]+[f"u\t{w}\t{counts[0][w]/total:.9g}" for w in sorted(vocabulary)]
    sizes={"unigrams":len(vocabulary)}
    for n,tag,cap in [(1,"b",80000),(2,"t",100000),(3,"q",80000)]:
        selected=sorted(((key,c) for key,c in counts[n].items() if c>=3 and all(w=="<s>" or w in vocabulary for w in key)),key=lambda x:(-x[1],x[0]))[:cap]
        sizes[tag]=len(selected)
        rows += [tag+"\t"+"\t".join(key)+"\t"+str(c) for key,c in sorted(selected)]
    raw=("\n".join(rows)+"\n").encode(); data=struct.pack(">I",len(raw))+zlib.compress(raw,9)
    (output/f"{code}-v1.bin").write_bytes(data)
    (cache/f"{code}-heldout.jsonl").write_text("".join(json.dumps(row,ensure_ascii=False)+"\n" for row in held),encoding="utf-8")
    metadata={"language":code,"tokenization":"character" if code in CHARACTER_LANGUAGES else "word","name":LANGUAGES[code],"source":url,"license":"CC BY 2.0 FR","attribution":"Tatoeba contributors (https://tatoeba.org)","retrieved_utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),"source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),"model_sha256":hashlib.sha256(data).hexdigest(),"training_sentences":training,"heldout_sentences":len(held),"maximum_training_sentences":maximum,"compressed_bytes":len(data),"counts":sizes,"split":"SHA256 of normalized tokens, first 4 bytes modulo 20; zero held out; exact duplicates removed","selection":"Export order, first maximum eligible training sentences","normalization":"NFC, lowercase, letters and combining marks, internal apostrophes; sentence punctuation resets context"}
    (output/f"{code}-manifest.json").write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"Trained {code}: {training} sentences, {len(vocabulary)} words, {len(data)} bytes",flush=True)
    return metadata

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--languages",nargs="+",choices=LANGUAGES,default=list(LANGUAGES));parser.add_argument("--cache",type=Path,default=ROOT/"build/tatoeba");parser.add_argument("--output",type=Path,default=ROOT/"resources/prediction");parser.add_argument("--max-sentences",type=int,default=200000);args=parser.parse_args()
    if args.max_sentences<1: parser.error("--max-sentences must be positive")
    args.cache.mkdir(parents=True,exist_ok=True);args.output.mkdir(parents=True,exist_ok=True)
    for code in args.languages: train(code,args.cache,args.output,args.max_sentences)
if __name__=="__main__": main()
