# Incremental dictation design brief

Status: **Implemented contract — phrase-stable, layered refinement**

## Feature summary

**Type while I speak** is an optional visible-delivery path. SenseVoice remains
an offline recognizer, so updates occur after VAD closes a natural phrase—not
as unstable token hypotheses. Even when visible delivery is off, Clean and
Adapt can process stable past phrases while recording continues, reducing the
work left at stop.

## Pipeline

```text
audio → stable VAD phrase → recognition repair → spoken cleanup
                                      └──────→ visible Clean revision
                              newest revisable tail → app Adapt worker
stop → settle final phrase → complete Clean draft → bounded final Adapt
```

Each phrase is a versioned speech unit with exact, repaired, cleaned, and
adapted forms. Only the newest two units or 320 Unicode code points are mutable.
Older units become committed read-only context. A result applies only when its
session ID, base revision, stage, and unit IDs still match.

## Delivery contract

1. Capture the foreground window, focused control, app identity, output mode,
   and app-profile override when dictation starts.
2. Decode completed phrases once and in order. Recognition owns its own worker.
3. Clean runs synchronously as bounded deterministic work. Adapt has a separate
   worker, retains its model, cancels live work for final composition, and
   coalesces pending live jobs to the newest revision.
4. The visible revision API receives the exact full suffix SAID currently owns
   and the new full suffix. A changed revision must replace the old suffix;
   replacement failure must never fall through to insertion.
5. Pre-existing text is never read as context, never sent to a model, and never
   included in the tracked suffix.
6. Keyboard/pointer input, focus change, control change, injection failure, or
   caret uncertainty revokes ownership. SAID leaves visible text untouched and
   copies the final result.
7. At stop, Clean settles the entire dictation. Adapt may perform one complete
   app-aware composition. Shell and unknown profiles stop at Clean.

## States and copy

| State | User-facing behavior |
| --- | --- |
| Live off | Processing still starts on stable phrases; insertion happens at finish. |
| Exact live | **Listening · typing live**; each stable phrase is inserted once. |
| Clean live | **Listening · cleaning live**; the owned mutable tail may be replaced. |
| Adapt live | **Listening · adapting for {profile}**; Clean remains the safe visible baseline. |
| Ownership lost | **Live typing paused**; final result goes to the clipboard. |
| Final Adapt | **Finishing structure**; shortcut keeps the complete Clean draft. |
| Unsafe/absent model | Keep Clean; never leave a partial or model-scaffolded result. |

## Regression contract

- raw → delayed Clean → delayed Adapt → newer raw ordering;
- stale session and revision rejection;
- newest-pending Adapt coalescing and final-job priority;
- cross-phrase Chinese and English self-correction;
- prefilled field plus at least three Unicode suffix replacements without
  duplication;
- user typing/click/focus loss and blocked injection;
- streaming off still performs hidden incremental refinement for Clean/Adapt;
- stop during in-flight Adapt and shortcut cancellation;
- final unsafe Adapt falls back to the complete Clean document.

SenseVoice/VAD reference pattern:

- <https://k2-fsa.github.io/sherpa/onnx/sense-voice/pretrained.html#real-time-streaming-speech-recognition-from-a-microphone-with-vad>
- <https://github.com/k2-fsa/sherpa-onnx/blob/master/rust-api-examples/README.md#example-39-simulated-streaming-asr-with-sensevoice-and-vad-from-microphone>
