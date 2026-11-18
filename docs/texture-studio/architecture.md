# Texture Studio architecture decision

Use one versioned, JSON-backed material graph for all three editing modes. Quick adjusts validated parameter bindings. Layers rewires the Base Color blend chain, including modes, visibility and opacity. The graph editor edits the same links and can group connected selections while preserving their parameter bindings and random streams. Layout and favorites do not affect the render hash.

Use a CPU reference evaluator with an explicit seed algorithm, ordered graph traversal and integer-generated analytic test fixtures. Large graphs with bounded local sampling use 128-pixel tiles with propagated halos. Coordinate remaps and histogram operations use full-image passes. Outputs carry channel semantics and derive from an immutable snapshot. The editor uploads completed previews on the render thread and discards stale revisions.

Keep game profiles and codecs separate from graph execution. Each profile is enabled only for implemented outputs. Keep the editable source, runtime material and per-file digest in the asset manifest. Map Apply captures the map epoch, history and face set, verifies them immediately before the existing transaction, and preserves projection unless the user selects a different UV policy.

Follow the owner's instruction on 18 November 2026 to perform QA without human acceptance. The evidence verifier must still require the automated suite for the exact clean source and build, plus separate authorization to publish. Development evidence and packages use TEST ONLY status.
