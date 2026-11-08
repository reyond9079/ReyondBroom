"""Camera arithmetic, native input guards, persistence and document immutability."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
log=(root/"navigation.log").read_text(errors="replace")
reopened=(root/"navigation-reopen.log").read_text(errors="replace")
assert "CAMERA_FAIL" not in log+reopened, (log+reopened)[-15000:]
checks=[
    "orbit_picks_cursor_surface","orbit_miss_fallback","orbit_radius_90_degrees",
    "orbit_position_90_degrees","orbit_pole_clamp","orbit_minimum_radius","gesture_end",
    "look_pitch_roll","shift_wheel_zoom","reset_zoom","wheel_flight_speed","speed_max",
    "speed_min","world_up","world_down","forward_includes_height","shift_double_speed",
    "alt_half_speed","ctrl_blocks_flight","zoom_scales_speed","middle_pan",
    "alt_forward_pan","wheel_forward","wheel_inversion","move_toward_cursor",
    "look_inversion","no_3d_motion_in_2d",
]
for name in checks: assert f"FTEBROOM_CAMERA_PASS {name}" in log,name
native=["text_guard","shift_flight","alt_vertical_flight","ctrl_guard","right_capture","right_release","relative_drag_does_not_open_context","flight_tap_does_not_open_context","middle_capture","escape_release","focus_loss","focus_return","right_click_context_menu","out_and_back_drag_not_context_menu"]
for name in native: assert f"FTEBROOM_NATIVE_CAMERA_PASS {name}" in log,name
for name in ("saved_position","saved_angles_zoom"):
    assert reopened.count(f"FTEBROOM_CAMERA_PASS {name}")==2,name
camera=[float(v) for v in (root/"cameras/qa_camera.cam").read_text().split()]
assert camera==[222,-111,444,24,68,0,80],camera
assert (root/"maps/navigation_before.map").read_bytes()==(root/"maps/navigation_after.map").read_bytes()
for name in ("navigation-preferences.png","navigation-restored.png"): assert (root/name).is_file(),name
print(f"PASS: {len(checks)+len(native)+2} navigation checks, restart and per-map persistence, unchanged MAP bytes.")
