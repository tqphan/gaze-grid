# Multilingual Tatoeba models

Select **Settings > Typing & speech > Word prediction > Language**. The choice
persists with Save preferences. Prediction language is independent of the OS
keyboard layout and local IME language. Models run offline. Only the selected
model is held in memory; personal learned counts are stored separately by language.

The languages are English, Spanish, French, German, Portuguese, Italian, Dutch,
Russian, Ukrainian, Polish, Turkish, Vietnamese, Indonesian, Arabic, Chinese
(Mandarin), Japanese, Korean, and Thai.

Chinese, Japanese, and Thai use character sequences, including attached combining
marks. Their suggestions predict the next character cluster, without appending a
space. These are initial character models, not segmented word completion or IME
conversion engines. Korean uses whitespace-delimited words. Vietnamese tokens
are whitespace-delimited and often correspond to syllables rather than full words.
Installing these models does not add gaze keyboard layouts for their scripts.
Local IME composition keeps its own candidate row.

## Data and attribution

Copyright Tatoeba contributors, https://tatoeba.org/ . The new `*-v1.bin` model
files, excluding `eng-v1.bin`, derive solely from Tatoeba sentence exports:
https://tatoeba.org/en/downloads . They are distributed under Creative Commons
Attribution 2.0 France: https://creativecommons.org/licenses/by/2.0/fr/ .

Changes: NFC and apostrophe normalization, lowercase conversion, tokenization,
duplicate removal, deterministic held-out split, vocabulary filtering, aggregate
unigram/bigram/trigram/fourgram counts, pruning, and compression. Sentence IDs in
held-out files link to https://tatoeba.org/en/sentences/show/ID . Each language's
`LANG-manifest.json` records the source URL, hashes, limits, tokenization, and counts.
The code remains under the repository license. `eng-v1.bin` retains its
existing sources and CC BY-SA 4.0 terms in [README.md](README.md).

The established English model is stored as `eng-v1.bin`, using the same naming
convention as the other languages. Its metadata is in `eng-manifest.json`.
Rebuild English with `tools/build_prediction_model.py` as documented in README.md;
the multilingual trainer trains the other 17 languages and preserves English.

## Training

```powershell
python tools/train_tatoeba.py
python tests/test_tatoeba_training.py
```

Python's standard library is sufficient. Exports are downloaded once into
`build/tatoeba`. Use `--languages spa fra`, `--cache PATH`, `--output PATH`, or
`--max-sentences 200000` to change the training run. Downloads are separate from
normal application builds. Keep source files matching the manifest hashes to
reproduce a model after the live Tatoeba exports change.

Training uses at most 200,000 eligible unique sentences per language in export
order. Smaller languages use all available eligible sentences. SHA256 of normalized
tokens assigns 5% to a held-out split; exact duplicates stay together. Up to 1,000
held-out sentences per language are saved under `build/tatoeba`. Vocabulary is
limited to 50,000 tokens occurring at least twice. Context entries need at least
three occurrences, with fixed per-order size caps. This is a small statistical
model, not a neural language model.

## Evaluation

```powershell
python tools/evaluate_tatoeba.py
build/windows-mingw/prediction-eval.exe --language spa build/tatoeba/spa-heldout.jsonl
```

The actual C++ predictor is evaluated on the first 200 held-out sentences with no
personal learning. Reports measure next-token top-three accuracy, word completion
after two UTF-16 units, vocabulary coverage, initialization time, and warm p95
latency. Character models report next-character accuracy and omit word-completion
accuracy. `multilingual-evaluation.json` records the results. English retains its separate historical evaluation in `evaluation.json`;
the English training corpus and held-out split differ from this multilingual run.

These are in-domain tests. Tatoeba contains language-learning examples and similar
sentence variants; exact deduplication does not remove all near-duplicate leakage.
Some language corpora are small. Results do not establish general-domain quality.
External edits and IME conversions still cannot be observed by desktop prediction.

## Recorded results

The 17 non-English training runs used 2,319,586 sentences and produced
11,345,317 bytes of compressed models. English adds 701,650 training sentences
and a 3,084,107-byte model.

| Language | Next-token top 3 | Completion top 3 |
| --- | ---: | ---: |
| Spanish | 31.9% | 68.9% |
| French | 34.3% | 74.1% |
| German | 27.9% | 76.4% |
| Portuguese | 30.2% | 73.1% |
| Italian | 32.8% | 71.3% |
| Dutch | 30.0% | 73.1% |
| Russian | 21.3% | 53.6% |
| Ukrainian | 19.6% | 51.8% |
| Polish | 20.0% | 52.9% |
| Turkish | 17.0% | 45.4% |
| Vietnamese | 35.3% | 85.2% |
| Indonesian | 22.7% | 63.9% |
| Arabic | 15.5% | 46.9% |
| Chinese | 42.0% | n/a |
| Japanese | 51.7% | n/a |
| Korean | 5.7% | 40.6% |
| Thai | 69.8% | n/a |

Chinese, Japanese, and Thai
next-token results describe character clusters; other rows describe word tokens.
These units differ, so percentages should not be compared directly across rows.
