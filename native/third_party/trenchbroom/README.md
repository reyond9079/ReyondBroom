Selected unmodified sources and resources from TrenchBroom 2026.2:
https://github.com/TrenchBroom/TrenchBroom/tree/4c756be1a54a28d921d18c77716a5f170bdc31d0

Original copyright/license notices are retained. LICENSE.txt is GPL-3.0-or-later.

TbElLib supplies the expression evaluator and its upstream tests; KdLib/TbBaseLib/VmLib provide supporting types, argument/path parsing, and contracts. TbFsLib provides disk operations and path matchers. These are linked statically into the host through native/profile.

Selected TbUiLib controls are built only into native/compile_ui with Qt Widgets/Svg. The core has no Qt dependency. The Compile forms and palette are adapted from the same pinned sources, with attribution in compile_ui.cpp. Original SVGs, application icon and base stylesheet are in runtime/ui/compilation. fmt headers are separately pinned to 11.2.0 with their own license.
