# SAID adaptive dictation design brief

Status: **Implemented preview contract — model quality gate remains open**

This brief supersedes the product assumption that every rewrite is only a
conservative grammar pass. It does not supersede SAID's local-processing,
caret-safety, explicit-consent, or reversible-delivery guarantees.

## 1. Feature summary

SAID becomes a local, application-aware dictation pipeline for bilingual
writers, developers, and keyboard-first Windows users. It repairs likely ASR
mistakes, removes speech-only noise and self-corrections, and—when the user
explicitly selects Adapt—organizes the result for the application receiving
the text.

Processing is incremental. Once VAD has finalized a phrase, correction begins
while the user continues speaking. Only transformations that need the complete
thought, such as document-level structure or reordering, wait until dictation
ends.

## 2. Primary user action

Choose an output behavior once, place the caret in any application, and speak.
SAID should make already-completed phrases progressively more useful without
interrupting speech, stealing focus, rewriting pre-existing text, or making the
user think about model architecture.

## 3. Design direction

The feature remains **terse, tactile, controlled**. The target application is
the primary surface; the compact Ink-and-Bone overlay reports whether SAID is
listening, cleaning, adapting, or safely falling back. Rewriting is always
named and never presented as invisible intelligence.

The memorable behavior is that earlier speech quietly settles into dependable
text while the user continues talking. Live text may improve, but it must not
flicker continuously or change outside SAID's owned suffix.

The product promise becomes:

> Say it once. SAID makes it usable where you are. On this PC.

## 4. User-facing behavior

### 4.1 Output modes

Replace the model-oriented Off / Standard / Advanced choices with behavior-
oriented choices:

| Mode | Contract | Model requirement |
| --- | --- | --- |
| **Exact** | Preserve the post-ASR transcript; do not rewrite it. | No rewrite model. |
| **Clean** | Repair high-confidence recognition mistakes, punctuation, fillers, repeated starts, and explicit self-corrections. Do not reorganize ideas. | Bundled rules and dictionaries; a compact correction model may be added only after it passes the quality gate. |
| **Adapt** | Apply Clean, then organize the thought for the current application without adding facts. | Explicit download of a local SAID Rewrite model. |

Clean is the recommended default for new installs. Existing users retain their
effective behavior: Off migrates to Exact; Standard and Advanced migrate to
Clean. Adapt is never enabled silently, even if an older large model is already
present.

### 4.2 Streaming is an independent delivery choice

The pipeline processes finalized phrases incrementally in every mode:

- With **Type while I speak** enabled, accepted revisions appear at the caret.
- With it disabled, the same work happens in the background and SAID inserts
  once at the end.

This keeps output quality consistent while letting the user independently
choose whether intermediate text is visible.

### 4.3 Application profiles

Adapt chooses a conservative local profile from the foreground executable and
window identity captured at dictation start. It does not read the existing
textbox or screen in the first release.

| Profile | Adapt behavior |
| --- | --- |
| Chat | Concise natural message; light punctuation; no invented greeting. |
| Mail | Clear paragraphs and action request; preserve recipients, dates, names, and sign-off; never invent them. |
| Document | Paragraphs, headings, and lists only when content supports them. |
| Developer prompt | State goal, relevant context, constraints, and requested tests; preserve technical tokens exactly. |
| Code editor | Clean prose and comments; preserve code, indentation signals, identifiers, and literals. |
| Shell / ambiguous terminal | Clean by default. Never invent commands, operators, flags, or newlines. |
| Unknown | Clean behavior until the user assigns a profile. |

Windows Terminal, tmux, and other ambiguous hosts need a user override such as
**Treat this app as Developer prompt**. Automatic title heuristics may suggest
a profile, but they may not relax the Shell safety contract by themselves.

## 5. Layout strategy

Keep Setup & settings as the only full configuration surface:

1. **Output** is a single three-choice control: Exact, Clean, Adapt.
2. **Type while I speak** remains a separate checkbox directly below it.
3. Selecting Adapt without its model expands an inline—not modal—download row
   showing exact download size, expected disk use, local-only processing,
   progress, cancel, and retry. Clean remains active until verification and
   atomic activation finish.
4. An **Application behavior** disclosure lists detected mappings and lets the
   user override the current application. Detailed mapping management stays
   collapsed until needed.
5. The tray exposes Output, Type while I speak, and the current application's
   profile. Model management is secondary to behavior selection.

The everyday overlay does not display transcript text or pipeline internals.
Its one-line state changes only when useful:

- **Listening** for Exact.
- **Listening · cleaning live** for visible Clean processing.
- **Listening · adapting for Codex** for a known Adapt destination.
- **Finishing structure** only while the final full-thought pass is running.

## 6. Incremental interaction model

### 6.1 Three text regions

Each dictation session maintains three conceptual regions:

```text
[ committed prefix ][ revisable tail ][ speech not yet finalized ]
       stable          last 2 phrases          audio only
```

- **Speech not yet finalized** is never shown as recognized text. SenseVoice is
  not treated as a token-streaming recognizer.
- **Revisable tail** normally contains the last two finalized phrases, capped
  at 12 seconds or 320 Unicode code points. It can absorb a following spoken
  correction such as “不对，四点” or “actually, Friday.”
- **Committed prefix** is not touched again during live dictation. It may still
  participate as read-only context, and the complete SAID-owned span may be
  replaced once during final structure if ownership remains valid.

A phrase leaves the revisable tail after two newer phrases have arrived or six
seconds have passed without a cross-phrase correction. Terminal punctuation
may shorten the window, but never eliminates the immediately preceding phrase
from correction context.

### 6.2 Processing stages

```text
Audio
  -> VAD-finalized phrase
  -> Recognition repair
  -> Spoken-language cleanup
  -> Local application adaptation
  -> Accepted live revision
  -> Final full-thought composition
  -> Safety gate
  -> Owned replacement or clipboard fallback
```

1. **Recognition repair — phrase level, immediate**
   - Normalize punctuation and bilingual spacing.
   - Apply the user's names, project terms, commands, and abbreviations.
   - Generate Chinese same/near-pinyin candidates and accept only
     high-confidence contextual corrections.
   - Protect English identifiers and use spelling correction only for actual
     nonwords; valid-word ASR substitutions require contextual evidence.

2. **Spoken-language cleanup — phrase window, incremental**
   - Remove fillers and exact false starts.
   - Resolve explicit repair markers such as “不对”, “改成”, “不是 X 是 Y”,
     “no, wait”, and “actually”.
   - Collapse repetitions while preserving intentional emphasis.
   - Never reorder independent claims or add information.

3. **Application adaptation — phrase window, incremental**
   - Run only in Adapt and only when the destination profile permits it.
   - Receive one previous committed phrase as read-only context plus the
     revisable tail.
   - Perform local phrasing and formatting. It may start a list after an
     explicit enumeration, but it does not invent headings or globally reorder
     the thought while speech is still open.

4. **Full-thought composition — dictation end**
   - Flush the final audio and settle all outstanding phrase revisions.
   - In Exact, deliver the canonical recognizer transcript.
   - In Clean, join the settled cleaned units without a global rewrite.
   - In Adapt, run one bounded full-context composition pass to create email,
     document, chat, or developer-prompt structure.
   - Validate the result. If it fails, fall back first to Clean, never to a
     partially hallucinated Adapt result.

### 6.3 Example timeline

```text
User speaks:  “王老师，我明天下午三点到”
Live tail:    王老师，我明天下午三点到。

User speaks:  “不对，四点到，麻烦您告诉大家”
Live tail:    王老师，我明天下午四点到，麻烦您告诉大家。

User finishes; Mail/Chat final pass:
Final:        王老师，我明天下午四点到。麻烦您告诉大家。
```

The second phrase is allowed to revise the immediately preceding phrase. Older
committed content does not churn during speech.

## 7. Technical architecture

### 7.1 Session model

Introduce an immutable-session snapshot and versioned semantic units:

```text
DictationSession
  session_id
  output_mode
  app_profile
  target_window / target_control
  owns_target
  exact_units[]
  settled_units[]
  visible_text
  revision_id

SpeechUnit
  unit_id
  raw_text
  repaired_text
  cleaned_text
  adapted_text
  state: recognized | refining | revisable | committed
```

`exact_units` are append-only and provide the reversible source of truth.
Every worker result carries `session_id`, `revision_id`, and covered unit IDs.
The coordinator discards stale or mismatched results.

### 7.2 Worker separation and backpressure

The current single worker must be split logically so a local LLM can never
block audio recognition:

- **Recognition worker:** VAD, SenseVoice, punctuation; highest priority.
- **Clean worker:** rules, dictionaries, phonetic candidates, compact scorer.
- **Adapt worker:** optional local rewrite model, loaded lazily.
- **Delivery coordinator:** version checks, safety checks, ownership checks,
  exact suffix replacement, clipboard fallback, and overlay state.

Each refinement queue is bounded. There may be one running job and one newest
pending job per session. A newly finalized phrase coalesces obsolete pending
work. Slow refinement reduces live polish frequency; it never delays ASR,
recording stop, Exact delivery, or the ability to keep the current draft.

### 7.3 Rewrite model contract

The target is a task-specific **SAID Rewrite 0.6B** model, with Qwen3 0.6B as
the first base candidate. Generic prompting is not a release-quality engine.
The model must be fine-tuned or distilled on bilingual speech-to-output pairs
for each application profile, with adversarial examples for negation, numbers,
names, paths, commands, code, and self-correction.

The runtime contract is:

- deterministic decoding;
- bounded input and output tokens;
- only the revisable unit range may be returned during live processing;
- protected spans are replaced with opaque sentinels before inference and must
  return exactly once;
- no tool execution, network access, conversation memory, or screen reading;
- lazy download, pinned hash, atomic activation, and safe removal;
- lazy load on first Adapt use, warm for the active dictation and a short idle
  window, then eligible for release.

The final quantization is selected by measured quality, not size alone. The
current candidates are approximately 429 MB for a local Q4 build and 639 MB
for the official Q8 artifact. Adapt is not released if neither meets the
quality gates below.

### 7.4 Safety gate

Before inference, extract and protect:

- numbers, dates, times, quantities, versions, ports, hashes, and addresses;
- negation and restrictive terms such as not, never, only, do not, 不, 没有,
  不要, 只能, and 必须;
- names and user-dictionary entries;
- URLs, email addresses, filesystem paths, shell flags, code identifiers,
  quoted text, and inline code.

After inference, reject output when:

- a protected sentinel is missing, duplicated, reordered illegally, or new;
- numeric or negation polarity changes;
- the output contains prompt scaffolding, commentary, or model markers;
- Shell output introduces an operator, command, newline, or control character;
- Clean crosses its edit or sentence-reordering budget;
- output growth, shrinkage, or unmatched content exceeds the mode-specific
  threshold.

An unsafe incremental result is ignored. An unsafe final Adapt result falls
back to settled Clean. Exact text remains available in memory until the next
dictation or application exit and is never persisted as history.

### 7.5 Text ownership and existing content

Pre-existing textbox content is outside the session and outside all model
input in the first release. SAID tracks exactly what it inserted:

- Live revision replaces only the exact current revisable suffix.
- Final composition may replace the complete SAID-owned dictation span.
- A keyboard event, pointer action, focus/control change, injection failure,
  uncertain caret, or stale session permanently revokes ownership.
- After revocation, SAID leaves all visible text untouched and copies the final
  result.
- Replacement failure never falls through to insertion, preventing the prior
  duplicate-text regression in Codex under tmux and ordinary text boxes.

The injector should compute the longest safe common prefix and replace only the
changed suffix, but correctness is defined by the tracked old visible text,
not by reading the target field.

## 8. Key states

| State | User-visible behavior |
| --- | --- |
| Exact listening | Existing listening state; no rewrite language. |
| Clean listening, no phrase | **Listening · cleaning live** when streaming is visible. |
| Phrase refining | Keep current draft visible; do not show a spinner for every phrase. |
| Adapt listening | Name the detected profile, e.g. **Adapting for Codex**. |
| Refinement behind | Continue recognition; coalesce work; no error unless final quality is affected. |
| Finishing Clean | Settle the final phrase and insert/replace once. |
| Finishing Adapt | **Finishing structure** / **Right Alt keeps the clean draft**. |
| User keeps draft | Cancel outstanding Adapt inference and preserve/deliver Clean immediately. |
| Unsafe model output | Silent fallback during speech; final overlay says **Kept the clean version**. |
| Model unavailable | Remain in Clean; offer explicit download/retry in settings. |
| Ownership lost | **Live typing paused**; finish to clipboard without touching visible text. |
| No speech | Existing no-speech recovery. |

## 9. Content requirements

Core labels:

- **Output**
- **Exact** — “Recognizer text, unchanged”
- **Clean** — “Fix speech mistakes without reorganizing”
- **Adapt** — “Organize for the app you are using”
- **Type while I speak** — “Settle completed phrases at the caret”
- **Download local writing model**
- **Runs only on this PC · about {download_size}**
- **Application behavior**
- **Treat this app as {profile}**
- **Finishing structure**
- **Right Alt keeps the clean draft**
- **Kept the clean version** — “The adapted result did not pass SAID's safety check”

Copy must remain literal, bilingual-ready, and understandable without relying
on color, animation, or model names.

## 10. Performance budgets

These are release targets on the existing Windows reference machine:

| Path | Target |
| --- | --- |
| Recognition queue | Never blocked by Clean or Adapt inference. |
| Rule/dictionary repair | P95 under 20 ms per phrase. |
| Compact Clean model, if adopted | P95 under 250 ms per phrase. |
| Warm Adapt phrase revision | P95 under 1.5 s after recognition. |
| Final Adapt, 200 characters | P95 under 3 s. |
| Final Adapt, 2,000 characters | P95 under 8 s or explicit progressive wait copy. |
| Pending refinement | At most one running and one coalesced pending job. |
| Adapt download | Target under 700 MB; exact size disclosed before download. |

Latency misses degrade to less frequent live revision or Clean fallback, not
lost speech or a blocked finish shortcut.

### 10.1 Measured preview footprint

The current Qwen3 0.6B Q8 Adapt engine peaks at about 1.10 GiB resident memory
and averages 5.6–6.7 logical CPU cores while generating on the 32-thread
reference machine. Combined with the resident speech stack, the estimated warm
application footprint is about 1.65 GiB. The model remains lazy until the first
Adapt request and the Adapt worker blocks without consuming CPU between jobs.
The product recommendation is 16 GB of system RAM. See
[`ADAPT_RESOURCE_PROFILE.md`](ADAPT_RESOURCE_PROFILE.md) for reproducible
measurements, caveats, and the values that must be revisited when the model,
context, or thread policy changes.

## 11. Quality and regression plan

### 11.1 Deterministic unit tests

- phrase state transitions, stability horizon, and mutable-tail limits;
- cross-phrase self-correction in Chinese and English;
- stale result rejection, cancellation, queue coalescing, and session reset;
- app classification and user overrides;
- protected-token extraction/restoration and every rejection reason;
- Exact / Clean / Adapt fallback selection;
- settings migration without silently enabling Adapt;
- Unicode suffix diffs, surrogate pairs, punctuation, and bilingual joining.

### 11.2 Integration tests with fake backends

- ASR continues while Adapt is slow or fails;
- raw phrase, delayed Clean result, delayed Adapt result, and stop ordering;
- an old model response cannot overwrite a newer phrase revision;
- existing textbox prefix remains byte-for-byte untouched;
- final output replaces SAID's live span instead of appending to it;
- user typing, pointer use, focus changes, and blocked injection revoke ownership;
- final Adapt rejection falls back to the complete Clean transcript;
- streaming off still uses incremental background processing but inserts once.

### 11.3 Windows end-to-end tests

- Notepad and a browser textarea with existing prefix/suffix content;
- Windows Terminal, Codex under tmux, and a shell-like input line;
- email/document-style editable fields;
- light, dark, High Contrast, reduced motion, and 100/150/200% scale;
- BITS download cancel/resume, pinned hash failure, atomic activation, removal,
  upgrade, and uninstall;
- no network access during dictation and no focus stealing.

### 11.4 Real-model quality suite

Build a versioned bilingual corpus from consented or synthetic SenseVoice
outputs covering chat, email, documents, developer prompts, code, and terminal
inputs. Every case stores audio or ASR text, Exact, acceptable Clean, acceptable
Adapt, protected spans, and forbidden changes.

Release gates:

- 100% preservation or safe rejection for the protected-token and prompt-
  injection suite;
- no accepted invented command, fact, recipient, date, time, number, or
  negation change;
- at least 95% explicit self-correction accuracy on the held-out suite;
- measurable Chinese character-error reduction with at least 99% precision on
  no-change cases;
- at least 70% human preference over the current grammar baseline for Adapt;
- fallback rate reported by profile and kept below 10% on ordinary held-out
  dictation before Adapt leaves preview.

Model output that fails a gate may still be useful during development, but it
does not ship as accepted user text.

### 11.5 Candidate evaluation — 2026-08-15

The runtime and real-model probe were exercised against two official local
GGUF candidates with the same app-profile prompts and preservation gate:

| Candidate | Artifact | Result |
| --- | ---: | --- |
| [Qwen3 0.6B Q8](https://huggingface.co/Qwen/Qwen3-0.6B-GGUF/blob/main/Qwen3-0.6B-Q8_0.gguf) | 639,446,688 bytes | English developer, mail, and chat fixtures were usable; repeated warm probes were about 0.9–1.25 seconds on the development host. A Chinese document fixture lost clauses and was correctly rejected to Clean. Suitable only as the current preview/base-model candidate. |
| [LFM2 700M Q4_K_M](https://huggingface.co/LiquidAI/LFM2-700M-GGUF/blob/main/LFM2-700M-Q4_K_M.gguf) | 468,624,320 bytes | Faster/smaller, but generic prompting answered dictated instructions, invented code and mail fields, and leaked control text. The gate rejected unsafe English samples. It is not selected. |

These results validate the fallback architecture, not release-quality Adapt.
The next model milestone is task-specific fine-tuning/distillation on the
versioned bilingual corpus; the generic Qwen artifact remains replaceable.

## 12. Delivery sequence

1. **Contracts first:** new mode names, migration rules, session/unit model,
   fake backends, safety gate, and regression fixtures.
2. **Incremental Clean:** separate workers, mutable-tail revision, current rules
   and user dictionary, streaming and non-streaming parity.
3. **Application identity:** local classifier, conservative profiles, per-app
   override, Shell restrictions, and privacy copy.
4. **Adapt runtime:** optional downloader/model abstraction, versioned jobs,
   final composition, cancellation, and Clean fallback.
5. **Model work:** collect/generate training pairs, fine-tune and quantize the
   0.6B candidate, run the real-model benchmark, and choose the artifact only
   after the quality gate.
6. **Preview rollout:** Adapt opt-in, diagnostics limited to local aggregate
   timings and rejection reasons, dogfood in Codex/tmux and common editors.
7. **Release:** update installer, setup, tray, docs, screenshots, release
   checklist, and upgrade tests. Clean remains the default; Adapt remains an
   explicit model download.

## 13. Anti-goals

- Token-by-token unstable ASR hypotheses.
- Reading surrounding textbox or screen content automatically.
- Rewriting text that existed before dictation.
- Inventing commands, facts, greetings, recipients, dates, or sign-offs.
- Executing terminal commands or injecting Enter.
- Cloud inference, transcript history, or background data collection.
- Repeated full-document rewrites during live speech.
- Shipping a small generic model merely because it meets a download-size goal.

## 14. Recommended implementation references

- `impeccable/reference/interaction-design.md` for behavior-mode selection,
  inline download disclosure, focus, and cancellation.
- `impeccable/reference/ux-writing.md` for short overlay and fallback copy.
- `impeccable/reference/motion-design.md` for the single transition from live
  settling to final structure; phrase revisions need no decorative animation.
- `docs/STREAMING_MODE_DESIGN.md` for the existing VAD and ownership contract.
- Chinese phonetic correction research and the SAID model benchmark for the
  correction/model quality strategy.

## 15. Locked product decisions

The implementation locks these product decisions:

1. Clean becomes the recommended default; Adapt is a separate behavior and an
   explicit local-model download.
2. The first release uses application identity only. It does not read existing
   textbox content; selected-text editing is a later, explicitly invoked mode.
3. Ambiguous terminals default to Clean unless the user assigns the Developer
   prompt profile. This protects real shell commands while supporting Codex
   under tmux through an override.
