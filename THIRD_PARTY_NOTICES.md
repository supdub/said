# Third-party notices

SAID links to and bundles `sherpa-onnx` (including ONNX Runtime), `llama.cpp`, and `miniaudio`.
Its default local model bundle contains SenseVoiceSmall, its token table, the
FunASR CT-Transformer punctuation model, and Silero VAD. Qwen3 0.6B
is an optional, separately downloaded Adapt model.

## Model attribution

- **SenseVoiceSmall**, created by FunAudioLLM / Alibaba Group and converted to
  ONNX for sherpa-onnx by Fangjun Kuang. SAID retains the model name, source,
  author, and agreement as required by the FunASR Model Open Source License
  Agreement v1.1. Sources:
  <https://huggingface.co/FunAudioLLM/SenseVoiceSmall> and
  <https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17>.
  The complete agreement is distributed as
  `third_party/FunASR-MODEL-LICENSE-1.1.txt`.
- **CT-Transformer punctuation model**, created by the Institute for
  Intelligent Computing / Alibaba and converted to ONNX for sherpa-onnx by
  Fangjun Kuang. The source model is distributed under Apache-2.0:
  <https://modelscope.cn/models/iic/punc_ct-transformer_zh-cn-common-vocab272727-pytorch>.
- **Silero VAD**, copyright (c) 2020-present Silero Team, distributed under
  the MIT License: <https://github.com/snakers4/silero-vad>.
- **Qwen3 0.6B**, created by the Qwen Team / Alibaba Cloud and
  distributed in GGUF format under Apache-2.0:
  <https://huggingface.co/Qwen/Qwen3-0.6B-GGUF>.

## sherpa-onnx, punctuation model, and Qwen3

`sherpa-onnx` is copyright its contributors and is distributed under the
Apache License 2.0. The CT-Transformer punctuation model is attributed above
and uses the same license. Qwen3 is also distributed under Apache-2.0. The complete license is distributed as
`third_party/Apache-2.0.txt`.

## llama.cpp

`llama.cpp` is copyright (c) 2023-2024 The ggml authors and is distributed
under the MIT License: <https://github.com/ggml-org/llama.cpp>.

## ONNX Runtime, SenseVoice source, Silero VAD, and llama.cpp — MIT License

- ONNX Runtime: Copyright (c) Microsoft Corporation.
- SenseVoice source code: Copyright (c) 2025 FunASR.
- Silero VAD: Copyright (c) 2020-present Silero Team.
- llama.cpp: Copyright (c) 2023-2024 The ggml authors.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## miniaudio — MIT No Attribution

Copyright 2025 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
