#!/usr/bin/env python3
"""Build the bundled model; downloads are deliberately separate from normal builds.

python3 tools/build_prediction_model.py --sentences /tmp/gaze-grid-eng.tsv.bz2 \
  --frequencies /tmp/gaze-grid-frequencies.txt --evaluation build/prediction-heldout.jsonl
"""
import argparse
import bz2
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
WORD = re.compile(r"[a-z]+(?:'[a-z]+)*")

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sentences', required=True)
    parser.add_argument('--frequencies', required=True)
    parser.add_argument('--evaluation', required=True)
    args = parser.parse_args()
    vocabulary = {w.lower() for w in (ROOT / 'resources/EnglishUS.dic').read_text(encoding='utf-8-sig').splitlines() if WORD.fullmatch(w.lower())}
    frequency = Counter()
    for line in Path(args.frequencies).read_text().splitlines():
        word, count = line.rsplit(' ', 1)
        if word in vocabulary:
            frequency[word] = int(count)
    unigram, bigram, trigram, fourgram = Counter(), Counter(), Counter(), Counter()
    seen = set()
    heldout = []
    training = 0
    with bz2.open(args.sentences, 'rt', encoding='utf-8') as source:
        for line in source:
            ident, language, text = line.rstrip('\n').split('\t', 2)
            text = text.replace('’', "'").lower()
            words = WORD.findall(text)
            if not 3 <= len(words) <= 30 or any(w not in vocabulary for w in words):
                continue
            normalized = ' '.join(words)
            if normalized in seen:
                continue
            seen.add(normalized)
            # Keep common name/pronoun variants in the same split to reduce leakage.
            group = re.sub(r'\b(tom|mary|john|alice|boston|he|she)\b', '_person_', normalized)
            split = int(hashlib.sha256(group.encode()).hexdigest()[:8], 16) % 20
            if split == 0:
                if len(heldout) < 2000:
                    heldout.append({'id': ident, 'words': words})
                continue
            training += 1
            # Sentence delimiters reset context, including internal punctuation.
            for sentence in re.split(r'[.!?;:\n]+', text):
                tokens = WORD.findall(sentence)
                history = ['<s>', '<s>', '<s>']
                for word in tokens:
                    unigram[word] += 1
                    bigram[(history[-1], word)] += 1
                    trigram[(history[-2], history[-1], word)] += 1
                    fourgram[(history[-3], history[-2], history[-1], word)] += 1
                    history.append(word)
    frequency_total = sum(frequency.values())
    corpus_total = sum(unigram.values())
    rows = ['gaze-grid-prediction-v1']
    for word in sorted(vocabulary):
        probability = .8 * frequency[word] / frequency_total + .2 * unigram[word] / corpus_total
        rows.append(f'u\t{word}\t{max(probability, 1e-10):.9g}')
    # Retain repeated evidence, not one-off sentences. Fixed caps bound startup/RAM.
    for tag, counts, cap in [('b', bigram, 180000), ('t', trigram, 240000), ('q', fourgram, 180000)]:
        selected = sorted(((key, n) for key, n in counts.items() if n >= 3), key=lambda item: (-item[1], item[0]))[:cap]
        for key, count in sorted(selected):
            rows.append(tag + '\t' + '\t'.join(key) + '\t' + str(count))
    raw = ('\n'.join(rows) + '\n').encode()
    destination = ROOT / 'resources/prediction/eng-v1.bin'
    destination.write_bytes(struct.pack('>I', len(raw)) + zlib.compress(raw, 9))
    Path(args.evaluation).write_text(''.join(json.dumps(row) + '\n' for row in heldout))
    metadata = {
        'format': 1, 'training_sentences': training, 'heldout_sentences': len(heldout),
        'unigrams': len(vocabulary), 'bigrams': sum(row.startswith('b\t') for row in rows),
        'trigrams': sum(row.startswith('t\t') for row in rows),
        'fourgrams': sum(row.startswith('q\t') for row in rows),
        'source_sha256': {'sentences': digest(args.sentences), 'frequencies': digest(args.frequencies)},
        'model_sha256': digest(destination), 'compressed_bytes': destination.stat().st_size,
        'split': 'SHA256 of normalized sentence with common names/pronouns grouped; first 8 hex digits modulo 20; 0 held out',
    }
    (destination.parent / 'eng-manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))

if __name__ == '__main__':
    main()
