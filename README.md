# cpp-corvus-json-schema

A [Bowtie](https://github.com/bowtie-json-schema/bowtie) test harness for the C++ wrapper of the corvus-json-schema C
library, part of [Corvus.JsonSchema](https://github.com/corvus-dotnet/Corvus.JsonSchema): a C ABI, with a
header-only C++17 wrapper, over the corvus-json-schema Rust crate.

Its image is published to `ghcr.io/bowtie-json-schema/cpp-corvus-json-schema` and run via
`bowtie run -i cpp-corvus-json-schema`.

The harness compiles each case's schema with the case's `registry` as the document resolver and validates each
instance. For `annotations` output it evaluates through a verbose collector and reports each annotation with its
instance location and `#…` keyword location. JSON is read and written with [nlohmann/json](https://github.com/nlohmann/json).

The image links the library statically from its latest release (the musl package for the platform, tagged
`capi-v<version>`); the `IMPLEMENTATION_VERSION` build argument pins another.
