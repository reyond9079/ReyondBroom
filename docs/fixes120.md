# AutoLevel UI fix in alpha 120

The Blocks Repo navigation button and the Level Builder repository selector used
the same Dear ImGui identifier. Hovering or switching pages could therefore show
an ImGui ID conflict. All five page buttons now have distinct, stable identifiers,
including when their active-page marker changes.

The generator's data format and geometry generation are unchanged.

## Validation

In addition to the AutoLevel regression suite, a new test uses the editor's real
Dear ImGui sources. It checks widget IDs and stack balance across 23 frames:

- All five pages, with expanded and collapsed sections, including a narrow window.
- Block Connection, Fill and Layer modes; variant actions and offsets.
- Big Block connections, Builder Selection and Connecting tools.
- Empty repositories, duplicate editable names and disabled controls during a job.

Run it on Windows or Linux with the pinned ImGui source directory:

```text
python scripts/test-autolevel-imgui.py --imgui /path/to/imgui --output /path/to/results
```

Use `--compiler` to select a C++17 compiler. The runner saves its compilation and
test logs in the output directory. It is also included in the Plugin SDK.
