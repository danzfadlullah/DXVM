
## Update (2026-11-21): Clean Architecture Migration Complete
All 9 tasks of the DXVM Clean Architecture plan implemented and reviewed.
10 critical issues from the earlier audit resolved; module `src/dxvk/dxvm_mali/`
now owns all Mali behavior. BCn handled at creation-time with CPU fallback.
HUD revamped to C1. Windows x64/x86 builds verified with all exports present.
See docs/superpowers/specs/2026-11-20-dxvm-clean-architecture-design.md.
