# Segmentation lexicons

Chinese source: jieba by Sun Junyi and contributors, MIT. See LICENSE-jieba.txt.
Thai source: PyThaiNLP words_th.txt, CC0-1.0, by the PyThaiNLP project.
Thai license declaration: https://github.com/PyThaiNLP/pythainlp/blob/c598a2ad96a64cbcd4c6e04aa71415a40e408871/pythainlp/corpus/corpus_license.md
CC0: https://creativecommons.org/publicdomain/zero/1.0/

sources.json pins the source revisions and SHA256 checksums. The original source
files are retained here; only the derived compressed lexicons ship as model data.
Both apps and test executables embed this notice and the jieba MIT license.

Rebuild with `python tools/word_segmentation.py`, then
`python tools/train_tatoeba.py --languages cmn tha`.

The builder keeps letter/mark-only words of at most 48 Unicode scalar values.
Chinese token costs are rounded negative log normalized frequencies times 1000.
Thai token costs are 10000. Segmentation minimizes total integer cost; an unknown
letter and attached marks cost 30000. Equal costs prefer a longer next token.
Non-target scripts remain whole. This is deterministic lexicon segmentation,
without HMM or neural inference. It can disagree with linguistic word boundaries.
Python and C++ use shared fixtures in cases.json to check runtime/training parity.
