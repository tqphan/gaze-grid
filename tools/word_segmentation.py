"""Offline lexicon segmentation shared by training and runtime (integer costs)."""
import functools, hashlib, json, math, struct, unicodedata, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/"resources/prediction/segmentation"
LANGUAGES={"cmn","tha"}

def target(ch,code):
    n=ord(ch)
    return (0x0e00<=n<=0x0e7f) if code=="tha" else (0x3400<=n<=0x9fff or 0xf900<=n<=0xfaff or 0x20000<=n<=0x323af)

@functools.cache
def lexicon(code):
    data=(ROOT/f"resources/prediction/{code}-lexicon-v1.bin").read_bytes()
    rows=zlib.decompress(data[4:]).decode().splitlines()
    if rows[0]!="gaze-grid-lexicon-v1": raise ValueError("Invalid lexicon")
    costs={word:int(cost) for word,cost in (line.split("\t") for line in rows[1:])}
    prefixes={word[:i] for word in costs for i in range(1,len(word)+1)}
    return costs,prefixes

def segment_run(text,code):
    costs,prefixes=lexicon(code);size=len(text)
    score=[0]*(size+1);ends=[0]*size
    for start in range(size-1,-1,-1):
        end=start+1
        while end<size and unicodedata.category(text[end])[0]=="M": end+=1
        score[start]=30000+score[end];ends[start]=end
        for stop in range(start+1,min(size,start+48)+1):
            word=text[start:stop]
            if word not in prefixes: break
            if stop<size and unicodedata.category(text[stop])[0]=="M": continue
            if word in costs:
                value=costs[word]+score[stop]
                if value<score[start] or (value==score[start] and stop>ends[start]): score[start]=value;ends[start]=stop
    result=[];start=0
    while start<size:
        result.append(text[start:ends[start]]);start=ends[start]
    return result

def segment(text,code):
    result=[];start=0
    while start<len(text):
        active=target(text[start],code);end=start+1
        while end<len(text) and (target(text[end],code)==active or unicodedata.category(text[end])[0]=="M"): end+=1
        run=text[start:end]
        result.extend(segment_run(run,code) if active else [run]);start=end
    return result

def build():
    sources=json.loads((DATA/"sources.json").read_text())
    for code in sorted(LANGUAGES):
        source=(DATA/f"{code}-source.txt").read_bytes()
        if hashlib.sha256(source).hexdigest()!=sources[code]["sha256"]: raise ValueError("Source checksum mismatch")
        counts={}
        for line in source.decode("utf-8-sig").splitlines():
            fields=line.split();word=fields[0] if code=="cmn" else line.strip()
            if not word or len(word)>48 or not all(target(ch,code) and unicodedata.category(ch)[0] in "LM" for ch in word): continue
            counts[word]=int(fields[1]) if code=="cmn" else 1
        total=sum(counts.values())
        rows=["gaze-grid-lexicon-v1"]
        for word,count in sorted(counts.items()):
            cost=round(-math.log(max(1,count)/total)*1000) if code=="cmn" else 10000
            rows.append(f"{word}\t{cost}")
        raw=("\n".join(rows)+"\n").encode();data=struct.pack(">I",len(raw))+zlib.compress(raw,9)
        (ROOT/f"resources/prediction/{code}-lexicon-v1.bin").write_bytes(data)
        sources[code]["lexicon_sha256"]=hashlib.sha256(data).hexdigest();sources[code]["words"]=len(counts)
        print(code,len(counts),len(data),flush=True)
    (DATA/"sources.json").write_text(json.dumps(sources,indent=2)+"\n",encoding="utf-8")
if __name__=="__main__": build()
