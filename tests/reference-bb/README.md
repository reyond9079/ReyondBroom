# Reference-codec checks

These scripts drive a separately installed Blockbench 5.2.2 web build, commit
`ec0b2827104d0799c153bfb5f867579c0c44652f`, using Playwright and Chrome.
No upstream application sources are shipped here. See
`tests/fixtures/bb/README.md` for fixture provenance and precision details.

Serve that build at localhost:29321, or set `BLOCKBENCH_URL`. Set
`BLOCKBENCH_ROOT` when generating fixtures. From this repository root, run:

```
node tests/reference-bb/generate-fixtures.cjs
node tests/reference-bb/generate-legacy-fixture.cjs
node tests/reference-bb/generate-gltf-reference.cjs
node tests/reference-bb/check-roundtrip.cjs /path/to/saved.bbmodel
```

The last command opens an actual saved editor project in the reference codec
and checks elements, groups, textures, animations and JavaScript errors. It
does not replace the standalone geometric and animation comparisons.
