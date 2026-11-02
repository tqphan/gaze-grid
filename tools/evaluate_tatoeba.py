#!/usr/bin/env python3
"""Evaluate trained models through the production C++ predictor."""
import argparse, hashlib, json, os
from pathlib import Path
import subprocess
from train_tatoeba import LANGUAGES, ROOT

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--executable",type=Path,default=ROOT/"build/windows-mingw/prediction-eval.exe");parser.add_argument("--cache",type=Path,default=ROOT/"build/tatoeba");parser.add_argument("--output",type=Path,default=ROOT/"resources/prediction/multilingual-evaluation.json");args=parser.parse_args()
    reports=[]
    for code in LANGUAGES:
        language=code
        result=subprocess.run([str(args.executable),"--language",language,str(args.cache/f"{code}-heldout.jsonl")],capture_output=True,text=True,encoding="utf-8",check=True)
        report=json.loads(result.stdout);model=ROOT/f"resources/prediction/{code}-v1.bin"
        report["model_sha256"]=hashlib.sha256(model.read_bytes()).hexdigest()
        report["heldout_sha256"]=hashlib.sha256((args.cache/f"{code}-heldout.jsonl").read_bytes()).hexdigest()
        reports.append(report)
        print(f"{code}: next-token top3 {report['next_token_top3_percent']:.1f}%, completion {report['completion_top3_percent']}",flush=True)
    args.output.write_text(json.dumps({"predictor":"production C++ WordPrediction","evaluation":"First 200 held-out sentences per language; no personal learning","reports":reports},indent=2)+"\n",encoding="utf-8")
if __name__=="__main__": main()
