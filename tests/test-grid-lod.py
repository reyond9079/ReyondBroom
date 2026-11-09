"""Render production GLSL on EGL to check continuous grid density changes.

Linux CI requires libEGL, libGL and a Mesa driver. No Python packages or window
server are needed. Tests use the actual fragment source, not a CPU reimplementation.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--shader-root', type=Path, default=Path(__file__).resolve().parents[1] / 'runtime/glsl')
p.add_argument('--output', type=Path)
a = p.parse_args()
E = C.CDLL('libEGL.so.1')
G = C.CDLL('libGL.so.1')
u, i, f, ptr = C.c_uint, C.c_int, C.c_float, C.c_void_p


def bind(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = result, args
    return fn


get_proc = bind(E, 'eglGetProcAddress', ptr, C.c_char_p)
get_display = C.CFUNCTYPE(ptr, u, ptr, C.POINTER(i))(get_proc(b'eglGetPlatformDisplayEXT'))
display = get_display(0x31DD, None, None)  # EGL_PLATFORM_SURFACELESS_MESA
initialize = bind(E, 'eglInitialize', u, ptr, C.POINTER(i), C.POINTER(i))
major, minor = i(), i()
assert initialize(display, C.byref(major), C.byref(minor)), 'Cannot initialize surfaceless EGL'
assert bind(E, 'eglBindAPI', u, u)(0x30A2), 'Cannot bind desktop OpenGL'
attrs = (i * 9)(0x3033, 1, 0x3040, 8, 0x3024, 8, 0x3025, 8, 0x3038)
config, count = ptr(), i()
assert bind(E, 'eglChooseConfig', u, ptr, C.POINTER(i), C.POINTER(ptr), i, C.POINTER(i))(
    display, attrs, C.byref(config), 1, C.byref(count)) and count.value, 'No EGL OpenGL config'
context = bind(E, 'eglCreateContext', ptr, ptr, ptr, ptr, C.POINTER(i))(
    display, config, None, (i * 1)(0x3038))
assert context, 'Cannot create OpenGL context'
assert bind(E, 'eglMakeCurrent', u, ptr, ptr, ptr, ptr)(display, None, None, context)
get_string = bind(G, 'glGetString', C.c_char_p, u)
create_shader = bind(G, 'glCreateShader', u, u)
shader_source = bind(G, 'glShaderSource', None, u, i, C.POINTER(C.c_char_p), C.POINTER(i))
compile_shader = bind(G, 'glCompileShader', None, u)
get_shader_iv = bind(G, 'glGetShaderiv', None, u, u, C.POINTER(i))
get_shader_log = bind(G, 'glGetShaderInfoLog', None, u, i, C.POINTER(i), C.c_char_p)
create_program = bind(G, 'glCreateProgram', u)
attach = bind(G, 'glAttachShader', None, u, u)
link = bind(G, 'glLinkProgram', None, u)
get_program_iv = bind(G, 'glGetProgramiv', None, u, u, C.POINTER(i))
get_program_log = bind(G, 'glGetProgramInfoLog', None, u, i, C.POINTER(i), C.c_char_p)
use = bind(G, 'glUseProgram', None, u)
uniform_location = bind(G, 'glGetUniformLocation', i, u, C.c_char_p)
uniform1f = bind(G, 'glUniform1f', None, i, f)
uniform2f = bind(G, 'glUniform2f', None, i, f, f)
uniform3f = bind(G, 'glUniform3f', None, i, f, f, f)
get_error = bind(G, 'glGetError', u)


def checked_status(obj, status_fn, log_fn, label):
    ok = i()
    status_fn(obj, 0x8B81 if label == 'compile' else 0x8B82, C.byref(ok))
    if not ok.value:
        log = C.create_string_buffer(16384)
        log_fn(obj, len(log), None, log)
        raise AssertionError(f'GLSL {label}: {log.value.decode()}')


def shader(kind, source):
    obj = create_shader(kind)
    encoded = C.c_char_p(source.encode())
    shader_source(obj, 1, C.byref(encoded), None)
    compile_shader(obj)
    checked_status(obj, get_shader_iv, get_shader_log, 'compile')
    return obj


# Float framebuffer keeps alpha below 1/255 visible to continuity assertions.
texture, framebuffer, vao = u(), u(), u()
bind(G, 'glGenTextures', None, i, C.POINTER(u))(1, C.byref(texture))
bind(G, 'glBindTexture', None, u, u)(0x0DE1, texture)
bind(G, 'glTexImage2D', None, u, i, i, i, i, i, u, u, ptr)(
    0x0DE1, 0, 0x8814, 16, 16, 0, 0x1908, 0x1406, None)
bind(G, 'glGenFramebuffers', None, i, C.POINTER(u))(1, C.byref(framebuffer))
bind(G, 'glBindFramebuffer', None, u, u)(0x8D40, framebuffer)
bind(G, 'glFramebufferTexture2D', None, u, u, u, u, i)(
    0x8D40, 0x8CE0, 0x0DE1, texture, 0)
assert bind(G, 'glCheckFramebufferStatus', u, u)(0x8D40) == 0x8CD5
bind(G, 'glGenVertexArrays', None, i, C.POINTER(u))(1, C.byref(vao))
bind(G, 'glBindVertexArray', None, u)(vao)
bind(G, 'glViewport', None, i, i, i, i)(0, 0, 16, 16)
clear = bind(G, 'glClear', None, u)
bind(G, 'glClearColor', None, f, f, f, f)(0, 0, 0, 0)
draw = bind(G, 'glDrawArrays', None, u, i, i)
read = bind(G, 'glReadPixels', None, i, i, i, i, u, u, ptr)
vertex = '''#version 130
varying vec2 tc;
varying vec4 vc;
varying vec3 viewdir;
uniform vec2 sampleTc;
uniform vec2 sampleDerivative;
uniform float sampleDistance;
uniform float sampleAlpha;
void main(){
    vec2 pos = gl_VertexID == 0 ? vec2(-1,-1) : gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3);
    tc = sampleTc + ((pos+1.0)*8.0-vec2(8.5))*sampleDerivative;
    vc = vec4(1,1,1,sampleAlpha);
    viewdir = vec3(sampleDistance,0,0);
    gl_Position = vec4(pos,0,1);
}
'''
results = {'renderer': get_string(0x1F01).decode(), 'version': get_string(0x1F02).decode(), 'shaders': []}
for name in ('ftebroom_grid.glsl', 'ftebroom_grid_retained.glsl'):
    source = (a.shader_root / name).read_text()
    body = source.split('#ifdef FRAGMENT_SHADER\n', 1)[1].rsplit('#endif', 1)[0]
    fragment = '#version 130\nvarying vec2 tc;\nvarying vec4 vc;\nvarying vec3 viewdir;\n' + body
    program = create_program()
    attach(program, shader(0x8B31, vertex))
    attach(program, shader(0x8B30, fragment))
    link(program)
    checked_status(program, get_program_iv, get_program_log, 'link')
    use(program)

    def one(key, value):
        uniform1f(uniform_location(program, key.encode()), value)

    def two(key, values):
        uniform2f(uniform_location(program, key.encode()), *values)

    def sample(tc, derivative, distance=128, alpha=1, aa=1.005, fade_end=1024):
        two('sampleTc', tc)
        two('sampleDerivative', derivative)
        one('sampleDistance', distance)
        one('sampleAlpha', alpha)
        one('cvar_ftebroom_grid_aa', aa)
        one('cvar_ftebroom_grid_fade_start', 512)
        one('cvar_ftebroom_grid_fade_end', fade_end)
        uniform3f(uniform_location(program, b'cvar_ftebroom_grid_color'), .2, .4, .8)
        clear(0x4000)
        draw(4, 0, 3)
        pixel = (f * 4)()
        read(8, 8, 1, 1, 0x1908, 0x1406, pixel)
        assert get_error() == 0
        assert all(-.00001 <= v <= 1.00001 for v in pixel), list(pixel)
        return list(pixel)

    # The sample lies on a line that previously disappeared in one frame when
    # projected density crossed a power-of-two boundary. Test X/Y and slopes.
    max_jump = 0
    checks = 0
    for exponent in range(-2, 10):
        d = 2.0 ** exponent
        line = 4*d
        for tc in ((line,line/2), (line/2,line), (line+.37,line/2+.19)):
            for anisotropy in ((1,1), (1,.125), (.125,1)):
                before = sample(tc, tuple(d*.9999*x for x in anisotropy))[3]
                after = sample(tc, tuple(d*1.0001*x for x in anisotropy))[3]
                jump = abs(before-after)
                assert jump < .02, (name, exponent, tc, anisotropy, before, after)
                max_jump = max(max_jump, jump)
                checks += 1
    # Random samples exercise both LOD resets and arbitrary subpixel phases;
    # continuity must also hold away from exact line centers and on negative
    # coordinates. Spatial tests catch parity fades that clip wide AA fringes.
    rng = random.Random(0x5242)
    random_jump = 0
    for trial in range(512):
        d = 2.0 ** rng.randint(-3,10)
        tc = tuple(rng.uniform(-32,32)*d for _ in range(2))
        ratio = 2.0 ** rng.uniform(-4,0)
        axes = (1,ratio) if trial % 2 else (ratio,1)
        aa = (.05,1.005,2)[trial % 3]
        before = sample(tc,tuple(d*.9999*x for x in axes),aa=aa)[3]
        after = sample(tc,tuple(d*1.0001*x for x in axes),aa=aa)[3]
        jump = abs(before-after)
        assert jump < .02, (name,'random LOD',trial,tc,axes,aa,before,after)
        random_jump = max(random_jump,jump)
    for trial in range(128):
        scale = 2.0 ** rng.randint(-2,9)
        tc = [(rng.randint(-16,16)+.5)*scale,rng.uniform(-16,16)*scale]
        if trial % 2:
            tc.reverse()
        direction = trial % 2
        before_tc, after_tc = tc[:], tc[:]
        before_tc[direction] -= scale*.0001
        after_tc[direction] += scale*.0001
        derivative = (scale*rng.uniform(.5,1),scale*rng.uniform(.5,1))
        before = sample(before_tc,derivative,aa=2)[3]
        after = sample(after_tc,derivative,aa=2)[3]
        jump = abs(before-after)
        assert jump < .02, (name,'spatial fringe',trial,tc,derivative,before,after)
        random_jump = max(random_jump,jump)
    # Near grid contrast, selected fine lines, alpha, color and world fade must
    # survive density smoothing. The fine line is not in the next coarse level.
    for derivative in (.125, .3, .49):
        assert sample((1,.5), (derivative,derivative))[3] > .98
    pixel = sample((0,.5), (.125,.125), alpha=.4)
    assert abs(pixel[3]-.4) < .0001 and all(abs(x-y)<.0001 for x,y in zip(pixel[:3],(.2,.4,.8)))
    near = sample((0,.5), (.125,.125), distance=512)[3]
    middle = sample((0,.5), (.125,.125), distance=768)[3]
    far = sample((0,.5), (.125,.125), distance=1024)[3]
    assert abs(near-1)<.0001 and abs(middle-.5)<.0001 and far==0
    assert sample((0,.5), (.125,.125), distance=4096, fade_end=0)[3] > .98
    for aa in (.05, 2):
        assert sample((0,.5), (.125,.125), aa=aa)[3] > .95
    results['shaders'].append({'name': name, 'source_sha256': hashlib.sha256(source.encode()).hexdigest(),
                               'continuity_cases': checks, 'maximum_alpha_jump': max_jump,
                               'random_temporal_cases':512,'random_spatial_cases':128,'maximum_random_alpha_jump':random_jump,
                               'contrast_fine_grid_color_alpha_distance_aa': 'passed'})
    print(f'PASS {name}: {checks} rendered LOD boundary cases; maximum alpha jump {max_jump:.6f}; 640 random temporal/spatial cases max {random_jump:.6f}; fine grid, color, alpha, fade and AA')
results['passed'] = True
if a.output:
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(results, indent=2)+'\n')
print(f'PASS actual GLSL on {results["renderer"]}')
