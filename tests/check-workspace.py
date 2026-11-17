from pathlib import Path
import sys
from PIL import Image

root = Path(sys.argv[1])
log = '\n'.join((root / f'{name}.cfg.log').read_text(errors='replace')
                for name in ('workspace', 'workspace-load'))
assert 'FTEBROOM_WORKSPACE_FAIL' not in log, log[-16000:]
checks = ['central_scene', 'central_mouse_passthrough', 'default_tab_group',
          'dock_splitter_resizes_scene', 'floating_panel', 'floating_panel_blocks_scene',
          'title_click_does_not_start_brush', 'scaled_font_raster', 'layout_written',
          'preferences_restored', 'floating_layout_restored', 'reset_redocks_panels',
          'large_font', 'large_font_scene_survives']
for check in checks:
    assert f'FTEBROOM_WORKSPACE_PASS {check}\n' in log, check
# Changing the workspace must not edit the MAP, including its face attributes.
assert (root / 'maps/workspace_before.map').read_bytes() == (root / 'maps/workspace_after.map').read_bytes()
for name in ('workspace-dark.png', 'workspace-large.png', 'workspace-floating.png'):
    image = Image.open(root / name).convert('RGB')
    colors = dict((rgb, count) for count, rgb in image.getcolors(image.width * image.height))
    # Modern Dark paints both viewport-adjacent panels and the floating dock.
    assert colors.get((57, 60, 64), 0) > 10000, f'Missing dark panels: {name}'
    controls = colors.get((50, 52, 56), 0) + colors.get((51, 54, 58), 0)
    assert controls > 1000, f'Missing dark controls: {name}'
print(f'PASS: {len(checks)} workspace checks, persistence across restart, dark palette screenshots and unchanged MAP.')
