# Third-Party Notices

## Code Dependencies

- Rust crates declared in `sidecar/Cargo.toml`
- Microsoft C++ build toolchain and Windows SDK, redistributable under their own terms

## Data and Model Policy

- Base Chinese lexicon strategy is split from source code licensing.
- CC-CEDICT may be used for dictionary enrichment in generated data pipelines. Its data is licensed under CC BY-SA 3.0 and must remain isolated from Apache-2.0 code artifacts.
- OpenCC may be used later for script conversion and variant normalization. OpenCC is Apache-2.0.
- Model files such as Qwen GGUF are not redistributed by default from this repository; download policy is handled by `build/Download-Model.ps1`.

## Alpha Scaffolding State

This repository currently ships only sample seed lexicon assets and mock completion fixtures. Before public alpha release, update this file with:

- Exact crate dependency notice list
- Exact Windows App SDK and WiX redistribution notice list
- Final dictionary provenance and attribution text
- Final model acquisition URLs and license notes
