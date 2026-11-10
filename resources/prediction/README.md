# Bundled English prediction model

`eng-v1.bin` contains frequency priors and bigram/trigram/four-gram counts. It runs
entirely locally, is included in the executable, and needs neither downloads nor
personal training. The existing English dictionary bounds the general vocabulary;
previously learned personal words can also be completed when personalization is enabled.

Settings → Typing & speech offers Context-aware (default), Common words
(frequency ranking without context), and Off. Next-word suggestions and local
personalization have separate switches. Changes apply immediately and persist
with Save preferences. Disabling personalization ignores saved counts and stops
new learning without deleting them.

## Sources and attribution

- **Tatoeba contributors**, English sentence export:
  https://downloads.tatoeba.org/exports/per_language/eng/eng_sentences.tsv.bz2
  Project/download information: https://tatoeba.org/en/downloads
  License: **Creative Commons Attribution 2.0 France (CC BY 2.0 FR)**,
  https://creativecommons.org/licenses/by/2.0/fr/
  Changes: normalized apostrophes/case, dictionary filtering, duplicate removal,
  held-out split, sentence tokenization, aggregate counts, pruning and compression.
- **Hermit Dave / FrequencyWords**, English 2018 top-50,000 frequency list,
  derived from OpenSubtitles 2018:
  https://github.com/hermitdave/FrequencyWords/blob/master/content/2018/en/en_50k.txt
  License for content: **Creative Commons Attribution-ShareAlike 4.0
  International (CC BY-SA 4.0)**,
  https://creativecommons.org/licenses/by-sa/4.0/
  Changes: dictionary filtering, normalization to probabilities and combination
  with the Tatoeba unigram prior. The repository's MIT code license does not cover
  this frequency data.

The combined model data is distributed under **CC BY-SA 4.0**, with the above
attribution retained. The application source code retains its existing license.
`eng-manifest.json` records exact source and output SHA-256 hashes and model sizes.
Source exports were retrieved during this implementation; upstream exports change.
Use files matching the manifest hashes to reproduce this exact artifact.

## Rebuilding and evaluating

Normal application builds use the checked-in model and need no Python packages
or network access. To rebuild, download the two source files above, then run:

```sh
python3 tools/build_prediction_model.py \
  --sentences /tmp/gaze-grid-eng.tsv.bz2 \
  --frequencies /tmp/gaze-grid-frequencies.txt \
  --evaluation build/prediction-heldout.jsonl
./build.sh
./build/prediction-eval build/prediction-heldout.jsonl
./build/prediction-eval --probe 'thank ' 'I would like to dr'
```

The builder excludes a deterministic 5% split from training and writes up to
2,000 held-out sentences for evaluation. Common name/pronoun variants share a
split; exact normalized duplicates are removed. The evaluator uses the first
500 held-out sentences, with **no personal learning**, and compares the actual
C++ predictor against the previous shortest-word ranking. It measures top-three
completion accuracy after two letters, next-word accuracy, simulated key
activations saved (one activation to accept a suggestion including its space),
cold initialization and warm p95 query latency.

## Measured result

The recorded [evaluation.json](evaluation.json) uses 500 held-out sentences,
3,052 completion events and 3,354 next-word events, without learned history:

| Measure | Previous predictor | Bundled context model |
| --- | ---: | ---: |
| Top-three completion accuracy after two letters | 27.2% | 77.8% |
| Top-three next-word accuracy | 0% (unsupported) | 38.3% |
| Simulated key activations saved | 23.4% | 56.1% |

Warm p95 query latency was 3.47 ms in the local Debug build. Cold model/index
initialization took 1.14 seconds and is performed during controller startup,
before typing. The compressed model in that historical evaluation was 3,068,246 bytes; decompressed
runtime memory use is larger. Timings depend on the machine.

Context continuations use compact sorted arrays, with binary search for counts;
the model data and ranking formula are unchanged. Loading parses one decompressed
row at a time to limit temporary allocations. In an isolated Windows Qt 6.10.1
Release predictor probe, peak working set fell from about 386 MiB with separate
per-context hash tables to 125 MiB. This measures the dictionary, predictor and
startup allocations, rather than the complete application's memory use.

This is an in-domain held-out benchmark, not a claim of accuracy for every user
or application. Tatoeba is a language-learning corpus and contains translation
patterns and similar sentence variants; the frequency prior adds subtitle-based
spoken English. No handwritten evaluation examples are added to training.

Prediction uses committed scratchpad text before the caret. Supported Windows
fields and Plasma input can supply caret context when best-effort-only prediction
is disabled; unsupported targets use the keys Gaze Grid sends. External edits can
leave that best-effort record out of sync. See [external text fields](MULTILINGUAL.md#external-text-fields).
Private native input fields suppress prediction and learning.

The format is a Qt-compatible qCompress stream (four-byte big-endian raw length,
then zlib). Its TSV payload starts with `gaze-grid-prediction-v1`, followed by
`u / word / probability`, `b / preceding-word / word / count`,
`t / preceding-word / preceding-word / word / count`, or
`q / preceding-word / preceding-word / preceding-word / word / count` records. `<s>` marks a
sentence boundary. Context counts are interpolated with lower-order probabilities;
personal counts apply a multiplier capped at two.

See [MULTILINGUAL.md](MULTILINGUAL.md) for the additional Tatoeba languages, training, attribution, and evaluation.
