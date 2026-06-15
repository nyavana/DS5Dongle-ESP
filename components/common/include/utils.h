//
// Shared DualSense protocol helpers (ported from legacy-pico/src/utils.h).
//
// The reference utils.h also pulled in transport debug helpers and <iostream>.
// Those are dropped here: opcode_to_str was debug-only, and print_hex is
// reimplemented over printf. What remains is the portable core shared by bt /
// state_mgr / cmd / audio: the DualSense CRC helpers and the SetStateData
// output-report layout.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

inline constexpr uint32_t crc32_table_entry(uint32_t index) {
    for (unsigned bit = 0; bit < 8; ++bit) {
        index = (index >> 1) ^ (0xEDB88320 & -(index & 1));
    }
    return index;
}

inline constexpr auto make_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < table.size(); ++i) {
        table[i] = crc32_table_entry(i);
    }
    return table;
}

inline constexpr auto crc32_lookup_table = make_crc32_table();

inline uint32_t crc32_seeded(const uint8_t *data, size_t size, const uint32_t seed) {
    uint32_t crc = ~seed;

    while (size--) {
        crc = (crc >> 8) ^ crc32_lookup_table[(crc ^ *data++) & 0xff];
    }

    return ~crc;
}

inline uint32_t crc32(const uint8_t *data, size_t size) {
    return crc32_seeded(data, size, 0xEADA2D49); // seed = CRC state after the 0xA2 output-report header
}

inline void fill_output_report_checksum(uint8_t *outputData, size_t len) {
    uint32_t crc = crc32(outputData, len - 4);
    outputData[len - 4] = (crc >> 0) & 0xFF;
    outputData[len - 3] = (crc >> 8) & 0xFF;
    outputData[len - 2] = (crc >> 16) & 0xFF;
    outputData[len - 1] = (crc >> 24) & 0xFF;
}

inline uint32_t crc32_feature(const uint8_t *data, std::size_t size) {
    // https://github.com/rafaelvaloto/Dualsense-Multiplatform/blob/main/Source/Private/GCore/Utils/CR32.cpp
    return crc32_seeded(data, size, 0x2060efc3); // seed = CRC state after the 0x53 feature-report header
}

inline void fill_feature_report_checksum(uint8_t *data, const size_t len) {
    uint32_t crc = crc32_feature(data, len - 4);
    data[len - 4] = (crc >> 0) & 0xFF;
    data[len - 3] = (crc >> 8) & 0xFF;
    data[len - 2] = (crc >> 16) & 0xFF;
    data[len - 1] = (crc >> 24) & 0xFF;
}

namespace MuteLight {
    enum MuteLight : uint8_t {
        Off = 0,
        On,
        Breathing,
        DoNothing,
        NoAction4,
        NoAction5,
        NoAction6,
        NoAction7 = 7
    };
}

namespace LightBrightness {
    enum LightBrightness : uint8_t {
        Bright = 0,
        Mid,
        Dim,
        NoAction3,
        NoAction4,
        NoAction5,
        NoAction6,
        NoAction7 = 7
    };
}

namespace LightFadeAnimation {
    enum LightFadeAnimation : uint8_t {
        Nothing = 0,
        FadeIn,
        FadeOut
    };
}

struct __attribute__((packed)) SetStateData { // 47
    uint8_t EnableRumbleEmulation: 1;
    uint8_t UseRumbleNotHaptics:   1;
    uint8_t AllowRightTriggerFFB: 1;
    uint8_t AllowLeftTriggerFFB: 1;
    uint8_t AllowHeadphoneVolume: 1;
    uint8_t AllowSpeakerVolume: 1;
    uint8_t AllowMicVolume: 1;
    uint8_t AllowAudioControl: 1;
    uint8_t AllowMuteLight: 1;
    uint8_t AllowAudioMute: 1;
    uint8_t AllowLedColor: 1;
    uint8_t ResetLights: 1;
    uint8_t AllowPlayerIndicators: 1;
    uint8_t AllowHapticLowPassFilter: 1;
    uint8_t AllowMotorPowerLevel: 1;
    uint8_t AllowAudioControl2: 1;
    uint8_t RumbleEmulationRight;
    uint8_t RumbleEmulationLeft;
    uint8_t VolumeHeadphones;
    uint8_t VolumeSpeaker;
    uint8_t VolumeMic;
    uint8_t MicSelect: 2;
    uint8_t EchoCancelEnable: 1;
    uint8_t NoiseCancelEnable: 1;
    uint8_t OutputPathSelect: 2;
    uint8_t InputPathSelect: 2;
    MuteLight::MuteLight MuteLightMode;
    uint8_t TouchPowerSave: 1;
    uint8_t MotionPowerSave: 1;
    uint8_t HapticPowerSave: 1;
    uint8_t AudioPowerSave: 1;
    uint8_t MicMute: 1;
    uint8_t SpeakerMute: 1;
    uint8_t HeadphoneMute: 1;
    uint8_t HapticMute: 1;
    uint8_t RightTriggerFFB[11];
    uint8_t LeftTriggerFFB[11];
    uint32_t HostTimestamp;
    uint8_t TriggerMotorPowerReduction : 4;
    uint8_t RumbleMotorPowerReduction : 4;
    uint8_t SpeakerCompPreGain: 3;
    uint8_t BeamformingEnable: 1;
    uint8_t UnkAudioControl2: 4;
    uint8_t AllowLightBrightnessChange: 1;
    uint8_t AllowColorLightFadeAnimation: 1;
    uint8_t EnableImprovedRumbleEmulation: 1;
    uint8_t UNKBITC: 5;
    uint8_t HapticLowPassFilter: 1;
    uint8_t UNKBIT: 7;
    uint8_t UNKBYTE;
    LightFadeAnimation::LightFadeAnimation LightFadeAnimation;
    LightBrightness::LightBrightness LightBrightness;
    uint8_t PlayerLight1 : 1;
    uint8_t PlayerLight2 : 1;
    uint8_t PlayerLight3 : 1;
    uint8_t PlayerLight4 : 1;
    uint8_t PlayerLight5 : 1;
    uint8_t PlayerLightFade: 1;
    uint8_t PlayerLightUNK : 2;
    uint8_t LedRed;
    uint8_t LedGreen;
    uint8_t LedBlue;
};

inline void print_hex(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}
