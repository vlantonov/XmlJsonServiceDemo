---
name: Coder
description: Writes C++ code following mandatory coding principles for the portfolio.
model: GPT-5.3-Codex (copilot)
tools: [vscode, execute, read, agent, context7/*, github/*, edit, search, web, vscode/memory, todo]
---

ALWAYS use #context7 to check current API documentation for any library, framework, build system, package manager, or language feature involved — gRPC-C++, protobuf, gtest/gmock, librdkafka or cppkafka, CMake, Conan, or Docker. Never assume you know the current answer just because the technology is familiar; your training data is in the past and these APIs change frequently, especially gRPC-C++ and protobuf across major versions, and Conan (v1 vs v2) generators and CMake integration.

## Mandatory Coding Principles (C++)

1. Ownership and Lifetime

- Prefer value semantics and RAII over manual lifetime management.
- Use smart pointers (`std::unique_ptr` by default, `std::shared_ptr` only when ownership is genuinely shared) — never raw owning pointers.
- Make ownership explicit in function signatures: pass by value or reference for non-owning use; accept `unique_ptr`/`shared_ptr` only when the function transfers or shares ownership.

2. Build Structure

- Each logical component gets its own CMake target (library or executable); avoid one monolithic target per repo.
- Public headers go in `include/`; private implementation headers stay colocated with their `.cpp` files — don't leak internals into the public surface.
- Manage external dependencies with the **Conan** package manager (>= 2.0): declare each one with a pinned version in the repo's root **`conanfile.txt`** (`[requires]`, `CMakeDeps` + `CMakeToolchain` generators, `cmake_layout`), following the portfolio reference at [KafkaTutorial/cpp/cppkafka](https://github.com/vlantonov/KafkaTutorial/tree/main/cpp/cppkafka). Consume packages in CMake via the generated `find_package` targets and configure through the generated preset (`cmake --preset conan-release`). Do not hand-vendor sources or add ad hoc `FetchContent` for new dependencies. Keep `conanfile.txt` and the CMake target wiring in sync.
- Before adding a new external dependency, check whether one already available on Conan Center and already used elsewhere in the portfolio (e.g. in CascadeClassifier) covers the need.

10. Packaging and Containers

- The application ships as a Docker image. Keep the **multi-stage `Dockerfile`**
  working. Key constraints verified against the actual build:
  - The **build stage** (`ubuntu:24.04`) needs `cmake make gcc g++ ninja-build clang`
    installed via apt. Install Conan 2 in an isolated venv
    (`python3 -m venv /opt/conan-venv && pip install "conan>=2.0,<3"`).
  - Run `conan install` with the **auto-detected profile** (gcc on Ubuntu) for
    dependency builds. Clang is applied only to the project's own CMake step via
    `-DCMAKE_CXX_COMPILER=clang++`.
  - Configure with the **explicit toolchain file** path — **not** `cmake --preset`:
    ```
    cmake /src -G "Unix Makefiles" -B /src/build/Release \
      -DCMAKE_TOOLCHAIN_FILE=/src/build/Release/generators/conan_toolchain.cmake \
      -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ -DXMLJSON_WARNINGS_AS_ERRORS=ON \
      -DXMLJSON_BUILD_TESTS=OFF
    ```
    `cmake --preset conan-release` is unreliable in containers: Conan only
    generates `CMakeUserPresets.json` when it can detect CMake at install time.
  - Copy `conanfile.txt` before copying source so the Conan dep layer is cached
    separately and only invalidated when `conanfile.txt` changes.
  - The **runtime stage** copies only `xmljson-service` and `config/default.json`,
    runs as a non-root user (`xmljson`), and exposes port `8080`.
- Maintain `.dockerignore` (exclude `build/`, `CMakeUserPresets.json`, VCS, Conan
  caches) to keep the build context small and reproducible.
- When a change touches build inputs, dependencies, or runtime config, verify:
  ```bash
  docker build -t xmljson-service .
  docker run --rm -d --name xmljson-test -p 18080:8080 xmljson-service
  curl -s http://localhost:18080/healthz   # must return {"status":"ok"}
  docker stop xmljson-test
  ```

3. API and Header Design

- Keep public headers minimal — forward-declare where possible; avoid dragging implementation-detail includes into public headers.
- Match this repo's existing namespace conventions.
- For anything crossing a process boundary (gRPC service, Kafka message, event payload), the schema (`.proto`/`.avsc`) is the source of truth — generate the wire-format structs, never hand-write them.

4. Error Handling

- Use exceptions for genuinely exceptional/unrecoverable conditions; use status- or expected-style returns for expected failure paths (parse errors, not-found, validation failures) — match whichever pattern this repo already uses.
- Never swallow errors silently. Log at service/process boundaries with enough context to debug without needing a live repro.

5. Concurrency

- State which threading model applies to each component you touch (single-threaded, thread-pool, async/coroutine-based) and don't mix models without explicit reason.
- Any shared mutable state crossing threads must have its synchronization mechanism called out explicitly in a comment — never implicit or assumed.

6. Testing

- New logic gets gtest coverage in the existing test directory structure.
- Tests verify observable behavior, not implementation details.
- Match this portfolio's existing CI conventions (multi-OS matrix, sanitizers, CodeQL) — see CascadeClassifier's CI setup as the reference point — rather than introducing a new CI style per repo.

7. Regenerability

- Structure code so any single `.cpp`/`.h` pair can be rewritten from scratch without breaking callers, provided the public header contract is preserved.
- Schema-generated code (from `.proto`/`.avsc`) is never hand-edited. If generated code is wrong, fix the schema or the generator invocation, not the output.

8. Modifications

- When extending or refactoring existing code, follow its existing patterns even where you'd choose differently from scratch. If you disagree with an existing pattern, flag it explicitly rather than silently diverging.
- Prefer full-file rewrites over scattered micro-edits for files under ~200 lines; for larger files, scope edits tightly to the relevant section.

9. Quality

- Favor deterministic, testable behavior.
- Keep tests simple and focused on verifying observable behavior.

## Verification (Definition of Done)

A change is NOT done until the gates in `.github/copilot-instructions.md` pass
locally. Run them before reporting completion — they mirror the gating CI jobs:

1. **Install deps with Conan, then strict build with clang** (clang is the
   strictest compiler in the matrix and catches warnings GCC misses, e.g.
   `-Wunused-lambda-capture`). Requires Conan >= 2.0 and CMake >= 3.21:

   ```bash
   mkdir -p build && cd build
   conan install .. --build=missing -pr:b=default -s build_type=Release
   cd ..
   cmake --preset conan-release \
     -DXMLJSON_WARNINGS_AS_ERRORS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
   cmake --build build/Release
   ```

   `XMLJSON_WARNINGS_AS_ERRORS` defaults to OFF — always pass it `ON`, or CI will
   fail on warnings your local build silently hid.

2. **Tests**: `ctest --test-dir build --output-on-failure`

3. **Static analysis**:

   ```bash
   cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
     --suppress=missingIncludeSystem --inline-suppr -I lib/include lib/src app tests
   ```

4. **Docker (when build inputs, dependencies, or runtime config changed)**:
   build the image and smoke-test the running container:

   ```bash
   docker build -t xmljson-service .
   docker run --rm -d --name xmljson-test -p 18080:8080 xmljson-service
   curl -s http://localhost:18080/healthz   # must return {"status":"ok"}
   docker stop xmljson-test
   ```

For cppcheck/clang false positives (e.g. `passedByValue` on `std::string_view`,
`syntaxError` on gtest `TEST_F`), use narrow inline `// cppcheck-suppress`
comments rather than changing otherwise-correct APIs. See the "Known analyzer
quirks" section of `.github/copilot-instructions.md`.
