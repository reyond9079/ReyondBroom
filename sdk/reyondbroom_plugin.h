// SPDX-License-Identifier: MIT
#ifndef REYONDBROOM_PLUGIN_H
#define REYONDBROOM_PLUGIN_H
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#define RB_PLUGIN_EXPORT __declspec(dllexport)
#else
#define RB_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
#define RB_PLUGIN_API_VERSION 1u
// All callbacks run on the editor thread. No STL, ImGui or engine structs cross
// this boundary. Buffers and strings remain owned by their caller.
typedef struct RBSettleOptionsV1 {
    double friction, bounce, impulse, seconds;
    uint32_t seed;
} RBSettleOptionsV1;
typedef struct RBViewportStateV1 {
    uint32_t struct_size;
    int hovered, view_type; // 0 perspective, 1 XY, 2 XZ, 3 YZ
    float ray_origin[3], ray_dir[3], camera_pos[3], camera_forward[3];
    int buttons_down, buttons_pressed, buttons_released; // 1 LMB, 2 RMB, 4 MMB
    int modifiers; // 1 Shift, 2 Ctrl, 4 Alt
    uint32_t digit_keys; // bit n = digit n held
} RBViewportStateV1;
typedef struct RBHostV1 {
    uint32_t api_version, struct_size;
    double (*number)(const char *name);
    int (*string)(const char *name, char *out, size_t capacity);
    void (*set_number)(const char *name, double value);
    void (*set_string)(const char *name, const char *value);
    void (*command)(const char *text);
    void (*log)(const char *text);
    // Imports a MAP fragment at its original coordinates, with editor Undo.
    int (*import_map)(const char *text, size_t length);
    // 0=start, 1=poll, 2=apply, 3=cancel. Result: -1=error, 0=pending, 1=ready.
    int (*settle)(int operation, const RBSettleOptionsV1 *options);
    void (*text)(const char *text);
    void (*separator)(const char *label);
    int (*input_int)(const char *label, int *value);
    int (*input_float)(const char *label, float *value);
    int (*checkbox)(const char *label, int *value);
    int (*combo)(const char *label, int *value, const char *zero_separated_items);
    int (*button)(const char *label);
    void (*same_line)(void);
    void (*disabled)(int begin);
    void (*progress)(float fraction, const char *label);
    // Optional tail extension: test struct_size before accessing these callbacks.
    // Paths are relative to the editor's data directory. Read size with out=NULL.
    int (*read_data)(const char *path, char *out, size_t capacity);
    // Writes generated OBJ/MTL/TGA under models or textures/reyond-plugins only.
    int (*write_asset)(const char *path, const char *bytes, size_t length);
    int (*input_text)(const char *label, char *value, size_t capacity);
    // Non-destructive world-space line preview: six floats per segment; NULL clears it.
    void (*preview_lines)(const float *xyz, size_t segments);
    int (*section)(const char *label, int default_open);
    // Optional: searchable picker from the mounted Material browser catalogue.
    // Empty selection means the plugin's generated/default material.
    int (*material_picker)(const char *label, char *value, size_t capacity);
    // Optional: export a static FTEBROOM_MESH v1 snapshot with native codecs.
    // Relative models/reyond-plugins/*.{mdl,md2,md3} path; returns 0 on error.
    // Textures resolve from the current material catalogue or generated assets.
    int (*write_model)(const char *path, const char *mesh, size_t length);
    // Optional: plugin-owned settings/MAP snapshots under data/plugin-* only.
    int (*write_data)(const char *path, const char *bytes, size_t length);
    // Explicit replacement of the current selection in one Undo transaction.
    int (*replace_selection)(const char *text, size_t length);
    // Further optional tail fields. All buffers are copied before return.
    // Lines: 6 floats + one 0xAABBGGRR color per segment, maximum 16384.
    void (*preview_colored_lines)(const float *xyz, const uint32_t *rgba, size_t segments);
    // Translucent AABBs, 6 floats min/max + color per box, maximum 32768.
    void (*preview_boxes)(const float *minmax, const uint32_t *rgba, size_t count);
    // World labels, maximum 4096, each text at most 1024 bytes.
    void (*preview_labels)(const float *xyz, const char *const *texts, size_t count);
    int (*viewport_state)(RBViewportStateV1 *out);
    // Captures viewport LMB/RMB; Alt+RMB continues camera look. Escape, window
    // close and unload release capture. Returns 0 if unavailable or released.
    int (*viewport_capture)(int enable);
    int (*selection_bounds)(float out_minmax[6]);
} RBHostV1;
typedef struct RBPluginV1 {
    uint32_t api_version, struct_size;
    const char *id;
    void (*draw)(void);
    void (*tick)(void);
    int (*busy)(void);
    void (*shutdown)(void);
} RBPluginV1;
#ifdef __cplusplus
extern "C" {
#endif
// A plugin exports exactly this entry point and returns static metadata.
RB_PLUGIN_EXPORT const RBPluginV1 *ReyondBroomPluginInit(const RBHostV1 *host);
#ifdef __cplusplus
}
#endif
#endif
