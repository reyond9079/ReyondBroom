"""Check first-run filtering defaults, saved overrides, and the large group regression."""
import re
import sys
from pathlib import Path

log = Path(sys.argv[1]).read_text(encoding='utf-8', errors='replace')
assert not re.search(r'HIERARCHY_FAIL|CSQC_Abort|runaway loop error', log), log[-4000:]
expected = {
    'ca_grid': ('16', '16'),
    'vid_multisample': ('8', '2'),
    'ftebroom_grid_aa': ('1.005', '0.75'),
    'gl_texture_anisotropy': ('16', '4'),
    'gl_texturemode': ('GL_NEAREST_MIPMAP_NEAREST', 'GL_LINEAR_MIPMAP_LINEAR'),
}
for name, pair in expected.items():
    values = re.findall(r'"' + name + r'" is "([^"]+)"', log)
    assert len(values) == 2, (name, values)
    assert tuple(values) == pair, (name, values, pair)
assert 'HIERARCHY_PASS large_reverse_chain' in log
assert 'HIERARCHY_PASS cycle_recovered' in log
print('PASS: filtering defaults, saved overrides, 56,603-node reverse hierarchy and cycle recovery')
