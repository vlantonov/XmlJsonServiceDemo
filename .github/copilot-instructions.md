# Copilot Instructions — XmlJsonServiceDemo

Repo-wide guidance for all agents (including the default agent). Agent-specific
files under `.github/agents/` build on top of this; this file always applies.

## Definition of Done (C++ changes)

A C++ change is **not complete** until all of the following pass locally. These
mirror the gating CI jobs exactly — run them before reporting work as done.

Use **clang** for the build gate: it is the strictest compiler in the matrix and
catches warnings GCC does not (e.g. `-Wunused-lambda-capture`).

1. **Install dependencies with Conan**, then **strict build** (matches
   `.github/workflows/ubuntu.yml`):

   ```bash
   conan install . --output-folder=build --build=missing -s build_type=Release
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
     -DXMLJSON_WARNINGS_AS_ERRORS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
   cmake --build build
   ```

   `XMLJSON_WARNINGS_AS_ERRORS` defaults to **OFF**, so a plain `cmake -B build`
   will silently hide warnings that CI treats as errors. Always pass it `ON`.

2. **Tests** (matches `.github/workflows/ubuntu.yml`):

   ```bash
   ctest --test-dir build --output-on-failure
   ```

3. **Static analysis** (matches `.github/workflows/static_check.yml`):

   ```bash
   cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
     --suppress=missingIncludeSystem --inline-suppr -I lib/include lib/src app tests
   ```

## Dependency management (Conan)

External dependencies are managed with the **Conan** package manager — not
hand-vendored, and not pulled ad hoc via CMake `FetchContent`.

- Declare every third-party dependency in the repo's `conanfile` (`conanfile.py`
  or `conanfile.txt`) with a pinned version, and consume it in CMake through the
  generated `conan_toolchain.cmake` (`CMakeToolchain`) and `CMakeDeps` targets.
  Link against the Conan-provided `find_package` targets (e.g.
  `nlohmann_json::nlohmann_json`, `pugixml::pugixml`, `spdlog::spdlog`).
- Before adding a new dependency, prefer one already available on Conan Center
  and already used elsewhere in the portfolio.
- Pin versions explicitly; never float on `latest`. Run `conan install` with
  `--build=missing` so missing binaries are built from source deterministically.
- Keep the `conanfile` and the CMake target wiring in sync — a dependency added
  to one must appear in the other, or the strict build gate above will fail.

## Docker (application packaging)

The service ships as a container image. Any change that affects how the app is
built, configured, or run must keep the Docker setup working.

- Maintain a **multi-stage `Dockerfile`**: a build stage that runs
  `conan install` + the strict CMake build, and a slim runtime stage that copies
  only the resulting `xmljson-service` binary and its runtime config
  (`config/default.json`).
- Keep a `.dockerignore` that excludes `build/`, VCS metadata, and local Conan
  caches so build context stays small and reproducible.
- Prefer a pinned base image and a non-root runtime user; expose the service
  port and document it. Provide a `docker-compose.yml` when the app needs to be
  run together with its config for local verification.
- After a change that touches build inputs, dependencies, or runtime config,
  verify the image builds (`docker build -t xmljson-service .`) and the container
  starts before reporting the work done.



These are verified false positives or strictness traps in this repo. Handle them
the documented way — do not change otherwise-correct APIs to silence a tool.

- **clang `-Werror` flags unused lambda captures** (`-Wunused-lambda-capture`).
  Capture only what the lambda body uses; drop an unused `this` capture rather
  than leaving it in.
- **cppcheck false-positives `passedByValue` on `std::string_view`** parameters.
  `string_view` is intentionally passed by value (it is cheap). Suppress with a
  narrow `// cppcheck-suppress passedByValue` on the line above — do not switch
  to `const std::string_view&`.
- **cppcheck reports `syntaxError` on gtest `TEST_F` fixtures** it cannot parse.
  Add a narrow `// cppcheck-suppress syntaxError` above the first affected
  `TEST_F` in the file. Do not restructure the test.
- Prefer STL algorithms (`std::transform`, `std::copy_if`) over raw accumulation
  loops in tests; cppcheck's `useStlAlgorithm` style check flags them.

Inline suppressions are honored because CI runs cppcheck with `--inline-suppr`.
Keep suppressions narrow (single line) and only for genuine false positives.
