from pathlib import Path
import re
import sys
root=Path(sys.argv[1])
log=(root/'qconsole.log').read_text(errors='replace')
assert not re.search(r'FTEBROOM_LINKED_FAIL|CSQC_Abort|runaway loop|stack overflow',log),log[-18000:]
assert log.count('FTEBROOM_LINKED_PASS ')==19,log[-18000:]
document = (root/'maps/linked.map').read_text()
assert '_tb_transformation' in document
# TrenchBroom reads entity properties before geometry. Ignore braces inside
# quoted values and comments so they cannot change the entity depth.
depth, has_geometry = 0, False
for token in re.finditer(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]', document):
    value = token.group()
    if value == '{':
        depth += 1
        if depth == 1:
            has_geometry = False
        elif depth == 2:
            has_geometry = True
    elif value == '}':
        depth -= 1
    elif depth == 1 and value.startswith('"'):
        assert not has_geometry, f'Entity property after geometry at line {document.count(chr(10), 0, token.start()) + 1}: {value}'
assert depth == 0
print('PASS: linked group editing and TrenchBroom-compatible entity property order')
