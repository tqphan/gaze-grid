# Fee-free, cross-platform speech for Gaze Grid

Research date: November 12, 2026. Requirements: no service, subscription, or per-use fees; Windows, Linux, and macOS. Engines must run locally after assets are installed. This review did not include audio auditions or hardware benchmarks.

## Recommendation

Start with **Kokoro through sherpa-onnx**. Compare it with **Pocket TTS** before choosing a default. Keep **Piper** as a third candidate when its available voices better match a required language or device.

Azure, ElevenLabs, and Windows-only SAPI adapters do not satisfy the revised requirements and are excluded. A cloud free tier is insufficient because it imposes quotas and may require payment later.

## Shortlist

| Candidate | Windows / Linux / macOS | Fees | Integration approach | Main qualification |
| --- | --- | --- | --- | --- |
| Kokoro + sherpa-onnx | Supported by runtime; macOS includes Intel and Apple Silicon | No hosted service needed; Apache-2.0 model weights | Persistent native helper, or native C API with compatible toolchain | sherpa explicitly excludes MinGW; pin the model pack and languages |
| Pocket TTS | Upstream advertises standalone binaries for all three | No hosted service needed | Persistent helper or local HTTP service | Check exact model/voice terms and CPU requirements |
| Piper | Upstream documents builds/wheels for all three | No hosted service needed | Native API or persistent helper | Voice quality and model provenance vary by voice |

These are upstream compatibility claims, not successful Gaze Grid builds or playback tests on all three systems.

## Kokoro: first prototype

Kokoro has 82 million parameters and Apache-2.0 weights. Its [model card includes a listening demo](https://huggingface.co/hexgrad/Kokoro-82M). Sherpa-onnx offers an [offline C API](https://k2-fsa.github.io/sherpa/onnx/c-api/html/tts.html) and [pretrained Kokoro packs](https://k2-fsa.github.io/sherpa/onnx/tts/pretrained_models/kokoro.html). Languages depend on the selected pack; do not infer support from the model family alone.

Sherpa documents [Linux, Windows, and macOS installation](https://k2-fsa.github.io/sherpa/onnx/install/index.html), including [Intel and Apple Silicon Macs](https://k2-fsa.github.io/sherpa/onnx/install/macos.html). Its [Windows instructions](https://k2-fsa.github.io/sherpa/onnx/install/windows.html) explicitly exclude MinGW and recommend Visual Studio 2022 or newer.

Gaze Grid's default Windows build uses MinGW. My proposed solution is a separate MSVC-built speech helper on Windows, with corresponding Linux and macOS helpers behind the same protocol. Keeping inference outside the application avoids coupling its compiler to the speech runtime. This design has not been implemented or tested here.

Choose and pin one runtime version, model pack, and voice set. Ship their notices and inspect frontend dependencies too. Keep the model loaded between utterances; restarting the engine for every phrase would add unnecessary delay.

## Pocket TTS: compare for deployment simplicity

The [upstream project](https://github.com/kyutai-labs/pocket-tts) advertises CPU inference, streaming, and standalone binaries for Windows, Linux, and macOS without Python/PyTorch. It also provides a local HTTP server that retains the model between requests. Its x86 binaries have CPU instruction requirements, with an alternative build for older CPUs. These features make a bundled helper worth evaluating against Kokoro.

Code is MIT; the [referenced model repository](https://huggingface.co/kyutai/pocket-tts) lists CC BY 4.0 and gates downloads behind acceptance of conditions. Voice recordings have separate licenses. Pin and inspect the exact model and voice assets before bundling so users do not unexpectedly need an account or online download at first use.

## Piper: alternative voices

The maintained [OHF Piper project](https://github.com/OHF-Voice/piper1-gpl) provides local neural synthesis and a C/C++ API. Its [build documentation](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/BUILDING.md) explicitly discusses Linux, Mac, and Windows wheels.

Its code is GPL-3.0; Gaze Grid already carries GPLv3. Individual voice assets need separate review. Upstream's [licensing guide](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/LICENSING.md) says to retain each MODEL_CARD and inspect the voice's fine-tuning ancestry. Select an exact voice and audition it rather than treating all Piper voices as equivalent.

## Gaze Grid integration

The existing speech layer is a useful starting point:

- src/SpeechOutput.h exposes open, say, cancel, and close.
- src/SpeechOutput.cpp dispatches these operations to a worker thread and rejects stale requests before submission.
- Windows uses SAPI and Linux uses Speech Dispatcher.
- Stop is queued on the same worker as say, so a long blocking inference call would delay cancellation.
- Submission success currently does not report playback completion or later asynchronous errors.

Proposed design:

1. Bundle a speech helper and a licensed model/voice pack for each target. Use a persistent process controlled through QProcess, with a small request protocol and returned PCM audio. A local HTTP server is another option, but adds lifecycle and port handling.
2. Use Qt Multimedia playback so the application has one audio path across systems. The current speech target links Qt Core only, so packaging must include the added Qt module and its runtime dependencies.
3. Keep inference asynchronous. Stop must stop playback, empty buffered audio, and reject late output from cancelled requests. If the engine cannot interrupt inference, discard its old output and provide a bounded helper restart path.
4. Add engine/voice selection and asynchronous error/completion reporting. Use short labels such as Engine, Voice, and Speed without descriptions.
5. Bundle assets or provide a one-time download, then verify speech with networking disabled. No API keys or paid accounts should be needed for normal use.

These are proposed implementation choices, not completed changes.

## macOS app support is additional work

A compatible speech engine does not make the whole app macOS-compatible. CMakeLists.txt currently rejects every platform except Windows and Linux, and README.md explicitly lists macOS as unsupported. The roadmap identifies missing native panel, input/output, speech, and display integration.

The speech helper can target all three systems, but full Gaze Grid support on macOS needs a separate application port and platform testing.

## Acceptance before choosing a default

Audition Kokoro and Pocket TTS with the same short conversational phrases, personal names, addresses, numbers, and required languages. Use an intended user's PC and Mac.

Measure cold and warm time to first audible speech, pronunciation, Stop delay, CPU use, memory, and installed size. Verify repeated requests, missing models, helper crashes, audio-device changes, and offline operation on each OS. Include older Windows/Linux PCs and both Mac architectures if both will be distributed.

Recommendation remains provisional until those listening and cancellation checks pass. No application code or release version was changed by this research.
