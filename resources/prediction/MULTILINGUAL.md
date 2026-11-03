# Multilingual Tatoeba models

Select **Settings > Typing & speech > Word prediction > Language**. The choice
persists with Save preferences. Prediction language is independent of the OS
keyboard layout and local IME language. Models run offline. Only the selected
model is held in memory; personal learned counts are stored separately by language.

The languages are English, Spanish, French, German, Portuguese, Italian, Dutch,
Russian, Ukrainian, Polish, Turkish, Vietnamese, Indonesian, Arabic, Chinese
(Mandarin), Japanese, Korean, and Thai.

Chinese and Thai use lexicon-segmented word models. Typing without spaces offers
both completions of the current lexical prefix and continuations after committed
words. Japanese retains a character model alongside its local IME conversion
candidates. None of these three languages appends completion spaces. Korean uses
whitespace-delimited words. Vietnamese tokens often correspond to syllables;
context-aware mode additionally offers observed two-token phrases.
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

## Language rules

`src/PredictionLanguage.h` shares prefix boundaries, Unicode normalization,
case conversion, and completion spacing between prediction and acceptance.
Turkish uses dotted/dotless I casing for dictionary lookup, suggestions, and
personal learning. The Turkish trainer uses the same casing rules, and its bundled
model has been rebuilt from the cached source recorded in the manifest. Other languages retain Unicode default casing. Completion
preserves the text already typed, including combining accents and curly
apostrophes, and inserts only a matching suffix. Chinese, Japanese, and Thai
continue to omit completion spaces.

The rules layer is shared by the word and character models. German noun
capitalization uses the surface forms described below.

Runtime dictionary validation and context tokenization operate on Unicode scalar
values. Supplementary-plane letters (including CJK Extension B) remain intact;
character-model tokens attach combining marks to the preceding letter, matching
the trainer. This prevents valid model entries from being discarded merely
because they require a UTF-16 surrogate pair.

## Vietnamese phrase suggestions

Context-aware Vietnamese prediction also offers two-token continuations from
observed bigrams, including after a complete syllable has been typed. It expands
at most 32 leading syllables, selecting one continuation per syllable when it
accounts for at least 25% of the recorded continuations. Both tokens must be in
the active dictionary. Phrase scores use the syllable score times continuation
share, discounted by 0.9; ordinary single-token suggestions remain available.
Frequency mode retains single-token behavior. This reuses the bundled offline
model and adds no settings or downloads. These are corpus-based suggestions,
not linguistic word segmentation. The Vietnamese evaluation now also reports
exact two-token matches in the top three suggestions. Token accuracy checks the
first token of each suggestion; phrase accuracy requires both tokens to match.
These offline metrics do not establish gaze savings or native-speaker usability.

On the recorded 200-sentence Vietnamese holdout, two-token completion top-three
accuracy is 9.79% (1,266 eligible events); next-phrase accuracy is 1.04% (1,439
events). Phrases compete for the same slots: token completion accuracy changes
from the previous recorded 85.17% to 83.78%, and next-token accuracy from 35.27%
to 34.72%. The model and held-out checksums are unchanged. This is an explicit
slot-allocation tradeoff, not an across-the-board prediction accuracy gain.

## Caret and composition

In the app-owned scratchpad, prediction reads committed text before the caret.
Accepting a completion replaces the remaining suffix of a space-delimited word,
preserves following text, and avoids inserting a duplicate space before existing
spacing or punctuation. Predictions are suppressed while text is selected or
composition is active. Cursor, selection, and composition changes invalidate old
dwell targets. IME candidates own the row while available or composing; committed
predictions return afterwards, and accepting them commits text directly without
feeding it back through the composition engine. Desktop injection still has only
best-effort context; it does not observe arbitrary applications' committed edits.

## Chinese and Thai word models

`segmentation/README.md` documents the pinned lexicons, licenses, integer-cost
algorithm, and rebuild commands. Segmentation is identical in the trainer and
runtime; unknown letters retain attached marks. Prefix lookup considers lexical
suffixes, so a partial word can span an earlier provisional boundary. Without an
explicit separator, the row interleaves word completions and continuations. A
continuation label includes the current prefix; acceptance inserts its suffix.
At a caret inside unspaced Chinese or Thai text, acceptance uses the same lexical
segmentation to replace the remaining suffix of the word containing the caret.

The rebuilt holdouts use segmented words and different deterministic splits;
their scores cannot be compared directly with the old character-model reports.
The evaluation uses actual inserted suffixes and preserves unspaced context.
Chinese currently has 15,022 vocabulary entries; Thai has 1,892. The small Thai
training corpus remains a material coverage limitation.

## Local composition languages

Changing the local IME locale selects the corresponding prediction model when
one is bundled. A later manual prediction-language choice remains until the IME
locale changes again. Korean local input maps gaze QWERTY keys to two-set Hangul
jamo, including shifted consonants and vowels; those keys show jamo labels.
Personal keyboards keep literal input. The installed Qt engines are covered by
real Pinyin, Japanese conversion/edit/cancel, and Korean Hangul composition tests.

## German casing and attested word forms

German retains frequent surface spellings observed away from sentence starts.
A form needs two observations and at least a 60% share to override lowercase.
All-capital prefixes stay uppercase. Acceptance can replace the typed prefix
when canonical capitalization differs; local edits use one input-method commit.

German and Turkish retain up to 100,000 attested vocabulary entries, including
single observations, to cover more compounds and inflected forms. Their rebuilt
vocabularies contain 64,244 and 75,301 entries. This increases coverage while also
admitting rare corpus errors; it does not invent unseen grammatical forms.
Model and holdout checksums and updated scores are in multilingual-evaluation.json.
