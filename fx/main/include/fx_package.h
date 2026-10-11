#ifndef AH_RAY_FX_PACKAGE_H
#define AH_RAY_FX_PACKAGE_H

#include <stdint.h>
#include <stdio.h>

#define AH_FX_PACKAGE_NAME "AH-RAY FX Package"
#define AH_FX_PACKAGE_ID "ahray_fx_v2"
#define AH_FX_PACKAGE_VERSION 8
#define AH_FX_DMX_PERSONALITY_NAME "AH-RAY 16CH Show"
#define AH_FX_DMX_CHANNEL_COUNT 16
#define AH_FX_DMX_ACTIVE_CHANNEL_COUNT 12
#define AH_FX_LIGHT_EFFECT_COUNT 47
#define AH_FX_SPATIAL_EFFECT_COUNT 25

#define AH_FX_DMX_MAIN_DIMMER 0
#define AH_FX_DMX_MAIN_COLOR 1
#define AH_FX_DMX_ACCENT_DIMMER 2
#define AH_FX_DMX_ACCENT_COLOR 3
#define AH_FX_DMX_BPM 4
#define AH_FX_DMX_BEAT 5
#define AH_FX_DMX_SPATIAL_EFFECT 6
#define AH_FX_DMX_LIGHT_EFFECT 7
#define AH_FX_DMX_FX_FADE 8
#define AH_FX_DMX_FX_EXPAND 9
#define AH_FX_DMX_FX_WARP 10
#define AH_FX_DMX_ZONE 11

typedef struct {
    uint8_t channel;
    const char *name;
} ah_fx_dmx_channel_def_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t dynamic;
} ah_fx_color_preset_def_t;

static const ah_fx_dmx_channel_def_t AH_FX_DMX_CHANNELS[AH_FX_DMX_CHANNEL_COUNT] = {
    {1, "Main Dimmer"},
    {2, "Main Color"},
    {3, "Accent Dimmer"},
    {4, "Accent Color"},
    {5, "BPM"},
    {6, "Beat"},
    {7, "Spatial Effect"},
    {8, "Light Effect"},
    {9, "FX Fade"},
    {10, "FX Expand"},
    {11, "FX Warp"},
    {12, "Zone"},
    {13, "Reserved"},
    {14, "Reserved"},
    {15, "Reserved"},
    {16, "Reserved"}
};

static inline const char *ah_fx_dmx_channel_name(uint8_t zero_based_index) {
    if (zero_based_index >= AH_FX_DMX_CHANNEL_COUNT) return "Unknown";
    return AH_FX_DMX_CHANNELS[zero_based_index].name;
}

static inline uint8_t ah_fx_dmx_channel_number(uint8_t zero_based_index) {
    if (zero_based_index >= AH_FX_DMX_CHANNEL_COUNT) return zero_based_index + 1;
    return AH_FX_DMX_CHANNELS[zero_based_index].channel;
}

static inline const char *ah_fx_light_effect_name(uint8_t value) {
    static const char *names[] = {
        "Solid", "Pulse", "Wipe", "Gradient", "Chase", "Sparkle",
        "Fire", "Spatial Pulse Ring", "Standing Wave", "Showering Sparks",
        "Rising Sparks", "Split Yaw CW", "Split Yaw CCW", "Split Roll CW",
        "Split Roll CCW", "Split Pitch CW", "Split Pitch CCW", "Strobe",
        "Meteor Sweep", "Bass Bloom", "Lightning Crack", "Prism Split",
        "Ember Drift", "Impact Flash", "Scanner Beam", "Aurora",
        "Negative Pulse", "Gravity Well", "Gravity Repulse", "Floor Wave",
        "Starfield", "Blackout Collapse", "Blackout Expand", "Water Slosh",
        "Lightning", "Rain", "Warp Speed", "Warp Speed Horizontal",
        "Explosion Pre", "Explosion Post", "Explosion Implode",
        "Corkscrew Up", "Corkscrew Down", "Laser Beams", "Laser Beams Random",
        "Particle Field", "Raise Up"
    };
    return value < (uint8_t)(sizeof(names) / sizeof(names[0])) ? names[value] : "Reserved";
}

static inline const char *ah_fx_spatial_effect_name(uint8_t value) {
    static const char *names[] = {
        "None", "Stage Outward", "Stage Inward", "X Distance From Stage",
        "Y Distance From Stage", "Horizontal Rings", "Inverse Horizontal Rings",
        "Diagonal One Way", "Diagonal Other Way", "Vertical Up", "Vertical Down",
        "Stable Random", "Room Snake", "Stage Away", "Stage Toward",
        "Stage Ripple", "Room Collapse", "Left/Right Sweep",
        "Front/Back Sweep", "Ceiling Drop", "Floor Rise", "Random Islands",
        "Spiral Room", "Distance Bands", "Zone Chase"
    };
    return value < (uint8_t)(sizeof(names) / sizeof(names[0])) ? names[value] : "Reserved";
}

static inline uint8_t ah_fx_beat_division_index(uint8_t value) {
    return (uint8_t)(((uint16_t)value * 13U) / 256U);
}

static inline const char *ah_fx_beat_division_name(uint8_t value) {
    static const char *names[] = {
        "8 Beats", "6 Beats", "4 Beats", "3 Beats", "2 Beats", "1 Beat",
        "1/2", "1/3", "1/4", "1/6", "1/8", "1/16", "1/32"
    };
    if (value == 0) return "BEAT RENDERED";
    uint8_t index = ah_fx_beat_division_index(value);
    return index < (uint8_t)(sizeof(names) / sizeof(names[0])) ? names[index] : "1 Beat";
}

static inline uint8_t ah_fx_zone_index(uint8_t value) {
    return (uint8_t)(((uint16_t)value * 12U) / 256U);
}

static inline const char *ah_fx_zone_name(uint8_t value) {
    static const char *names[] = {
        "All Zones",
        "Z Axis Only",
        "Ceiling X/Y",
        "Floor X/Y",
        "Y1 Backwall",
        "Y9 Front",
        "X1 Left",
        "X9 Right",
        "X1 + X9",
        "Stage Row Y",
        "Stage Column X",
        "Stage Position"
    };
    uint8_t index = ah_fx_zone_index(value);
    return index < (uint8_t)(sizeof(names) / sizeof(names[0])) ? names[index] : names[0];
}

static inline const char *ah_fx_color_preset_name(uint8_t value) {
    static const char *names[] = {
        "RGB / Disabled",
        "Red", "Red-Orange", "Orange", "Amber", "Gold", "Yellow", "Yellow-Lime",
        "Lime", "Spring Green", "Green", "Sea Green", "Mint", "Aqua", "Cyan",
        "Sky Blue", "Azure", "Blue", "Deep Blue", "Indigo", "Violet", "Purple",
        "Magenta", "Fuchsia", "Hot Pink", "Pink", "Rose", "Coral", "Salmon",
        "Peach", "Warm Amber", "Orange-Red", "Deep Red", "White", "Warm White",
        "Neutral White", "Cool White", "Black", "UV Purple", "Deep Purple",
        "Lavender", "Pale Blue", "Ice Blue", "Teal", "Emerald", "Forest Green",
        "Chartreuse", "Sodium Amber", "Candle", "Fire Orange", "Crimson",
        "Blood Red", "Rose Gold", "Blush", "Pastel Pink", "Pastel Purple",
        "Pastel Blue", "Pastel Cyan", "Pastel Green", "Pastel Yellow",
        "Champagne", "CTO Warm", "CTB Cool", "Dim White", "Rainbow / Auto Color"
    };

    if (value == 0) return names[0];
    uint8_t index = (uint8_t)(((uint16_t)value - 1U) / 4U) + 1U;
    if (value >= 253) index = 64;
    if (index >= (uint8_t)(sizeof(names) / sizeof(names[0]))) return "Preset Color";
    return names[index];
}

static inline uint8_t ah_fx_color_preset_index(uint8_t value) {
    if (value == 0) return 0;
    if (value >= 253) return 64;
    return (uint8_t)(((uint16_t)value - 1U) / 4U) + 1U;
}

static inline uint8_t ah_fx_color_preset_rgb(uint8_t value, uint8_t *r, uint8_t *g, uint8_t *b) {
    static const ah_fx_color_preset_def_t colors[] = {
        {0, 0, 0, 0},
        {255, 0, 0, 0}, {255, 64, 0, 0}, {255, 128, 0, 0}, {255, 180, 0, 0},
        {255, 210, 0, 0}, {255, 255, 0, 0}, {200, 255, 0, 0}, {128, 255, 0, 0},
        {64, 255, 0, 0}, {0, 255, 0, 0}, {0, 255, 64, 0}, {0, 255, 128, 0},
        {0, 255, 160, 0}, {0, 255, 255, 0}, {0, 200, 255, 0}, {0, 150, 255, 0},
        {0, 0, 255, 0}, {0, 0, 200, 0}, {64, 0, 255, 0}, {128, 0, 255, 0},
        {160, 0, 255, 0}, {255, 0, 255, 0}, {255, 0, 200, 0}, {255, 0, 150, 0},
        {255, 100, 150, 0}, {255, 80, 120, 0}, {255, 120, 80, 0}, {255, 140, 100, 0},
        {255, 180, 120, 0}, {255, 150, 50, 0}, {255, 60, 0, 0}, {200, 0, 0, 0},
        {255, 255, 255, 0}, {255, 210, 160, 0}, {255, 245, 220, 0}, {200, 225, 255, 0},
        {0, 0, 0, 0}, {120, 0, 255, 0}, {80, 0, 160, 0}, {180, 120, 255, 0},
        {150, 200, 255, 0}, {120, 220, 255, 0}, {0, 180, 160, 0}, {0, 200, 80, 0},
        {0, 100, 40, 0}, {180, 255, 0, 0}, {255, 140, 0, 0}, {255, 120, 40, 0},
        {255, 70, 0, 0}, {180, 0, 40, 0}, {120, 0, 20, 0}, {255, 120, 100, 0},
        {255, 160, 180, 0}, {255, 180, 220, 0}, {200, 160, 255, 0}, {170, 220, 255, 0},
        {160, 255, 255, 0}, {180, 255, 180, 0}, {255, 255, 170, 0}, {255, 220, 180, 0},
        {255, 180, 120, 0}, {180, 210, 255, 0}, {80, 80, 80, 0}, {0, 0, 0, 1}
    };

    uint8_t index = ah_fx_color_preset_index(value);
    if (index >= (uint8_t)(sizeof(colors) / sizeof(colors[0]))) return 0;
    if (r) *r = colors[index].r;
    if (g) *g = colors[index].g;
    if (b) *b = colors[index].b;
    return colors[index].dynamic ? 0 : 1;
}

static inline void ah_fx_format_dmx_value_name(uint8_t channel, uint8_t value, char *out, size_t out_size) {
    if (!out || out_size == 0) return;

    switch (channel) {
        case 1:
            if (value == 0) snprintf(out, out_size, "Blackout");
            else snprintf(out, out_size, "%u%%", (unsigned)((value * 100U) / 255U));
            break;
        case 2:
        case 4:
            snprintf(out, out_size, "%s", ah_fx_color_preset_name(value));
            break;
        case 3:
        case 9:
        case 10:
        case 11:
            if (value == 0) snprintf(out, out_size, "0%%");
            else if (value == 255) snprintf(out, out_size, "100%%");
            else snprintf(out, out_size, "%u%%", (unsigned)((value * 100U) / 255U));
            break;
        case 5:
            if (value == 0) snprintf(out, out_size, "Disabled");
            else snprintf(out, out_size, "%u BPM", (unsigned)value);
            break;
        case 6:
            snprintf(out, out_size, "%s", ah_fx_beat_division_name(value));
            break;
        case 7:
            snprintf(out, out_size, "%s", ah_fx_spatial_effect_name(value));
            break;
        case 8:
            snprintf(out, out_size, "%s", ah_fx_light_effect_name(value));
            break;
        case 12:
            snprintf(out, out_size, "%s", ah_fx_zone_name(value));
            break;
        case 13:
        case 14:
        case 15:
        case 16:
            snprintf(out, out_size, "Reserved");
            break;
        default:
            snprintf(out, out_size, "%03u", (unsigned)value);
            break;
    }
}

#endif
