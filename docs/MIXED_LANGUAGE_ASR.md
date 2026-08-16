# Mixed Chinese/English ASR investigation

## Decision

SAID now uses SenseVoice Small int8 through `sherpa-onnx`, with automatic language selection, built-in inverse text normalization disabled, and a separate CT-Transformer Chinese/English punctuation model. Silero VAD splits only recordings longer than 25 seconds; short recordings stay intact so a language switch is not separated from its surrounding context.

This is a quality-first trade: the model bundle grows from about 82 MB to about 315 MB, but recognition is both more accurate on mixed speech and faster in the local experiment.

## Experiment

The test set was a deterministic sample of 24 held-out mixed Mandarin/English utterances from the ASCEND test split, totaling 112.3 seconds. Whisper tests used `whisper.cpp` v1.9.2 with the published base-Q8 and small-Q5 multilingual models; SenseVoice tests used `sherpa-onnx` 1.13.5 and the published int8 ONNX conversion. Scoring lowercased and Unicode-normalized the text, removed punctuation, treated each Han character and each English word as one unit, and removed recognizer `<unk>` markers. The main metric is mixed error rate (MER); English WER and Chinese CER are diagnostic slices.

| Recognizer | Configuration | MER | English WER | Chinese CER |
| --- | --- | ---: | ---: | ---: |
| Whisper base Q8 | auto language, no prompt | 51.2% | 89.9% | 52.6% |
| Whisper base Q8 | previous shipped bilingual prompt | 41.0% | 81.0% | 43.1% |
| Whisper small Q5 | bilingual prompt | 37.9% | 69.6% | 37.6% |
| SenseVoice Small int8 | auto language, built-in ITN | 17.1% | 46.8% | 11.4% |
| **SenseVoice Small int8** | **auto language, raw tokens** | **13.2%** | **30.4%** | **10.1%** |

On an Intel i9-14900HX under WSL, Whisper base took 25.0 seconds end-to-end for the batch. SenseVoice loaded in about 0.68 seconds and decoded it in 2.53 seconds (real-time factor 0.023, about 44× real time). These timings describe one machine and are not a general hardware guarantee.

The built-in SenseVoice ITN was disabled because it sometimes mutated embedded English in this sample, including fragments of “scenario,” “technology,” and “mobile phone number.” The separate punctuation model preserved those tokens while adding useful boundaries, for example:

```text
因为他们如果要 sell 这个 technology 他们就要 take a responsibility take a risk
因为他们如果要sell这个technology，他们就要take a responsibility，take a risk。
```

A small deterministic postprocessor also joins letter-by-letter acronym output such as `n l p`, `u s`, and `a i` into `NLP`, `US`, and `AI` before punctuation.

## Why this stack

- Code-switching is not the same as running monolingual ASR twice: the recognizer must preserve language changes inside one utterance. The ASCEND corpus was created specifically for spontaneous Mandarin/English code-switching.
- A recent independent spontaneous Mandarin/English benchmark reported SenseVoice Small as the strongest zero-shot system by MER among its tested systems, which agrees with the local directional test.
- SenseVoice direct inference is intended for clips under 30 seconds. SAID keeps short dictation whole and uses VAD with a 20-second segment ceiling for longer recordings.
- Punctuation is deliberately separate from recognition so readability does not come at the cost of altered English tokens.

## Remaining work for another quality step

Rare names, domain jargon, and one-word language switches are still difficult. The next meaningful improvement would be fine-tuning on consented SAID dictation-style Mandarin/English data or adding a carefully evaluated hotword mechanism. Prompt-only Whisper tuning helped, but the experiment shows that it does not close the gap. A larger benchmark—more speakers, microphones, accents, noise levels, and sentence types—should gate any future model change.

## Sources

- [ASCEND Mandarin-English code-switching corpus](https://arxiv.org/abs/2112.06223)
- [CS-Dialogue spontaneous Mandarin-English benchmark](https://arxiv.org/abs/2502.18913)
- [Whisper code-switching adaptation study](https://arxiv.org/abs/2311.17382)
- [SenseVoice project and model documentation](https://github.com/QwenAudio/SenseVoice)
- [sherpa-onnx SenseVoice deployment documentation](https://k2-fsa.github.io/sherpa/onnx/sense-voice/pretrained.html)
- [sherpa-onnx punctuation models](https://k2-fsa.github.io/sherpa/onnx/punctuation/pretrained_models.html)
