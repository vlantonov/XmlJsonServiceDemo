# Copilot Instructions — XmlJsonServiceDemo

Repo-wide guidance for all agents (including the default agent). Agent-specific
files under `.github/agents/` build on top of this; this file always applies.

## Definition of Done (C++ changes)

A C++ change is **not complete** until all of the following pass locally. These
mirror the gating CI jobs exactly — run them before reporting work as done.

Use **clang** for the build gate: it is the strictest compiler in the matrix and
catches warnings GCC does not (e.g. `-Wunused-lambda-capture`).

1. **Strict build** (matches `.github/workflows/ubuntu.yml`):

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
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

## Known analyzer quirks

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
