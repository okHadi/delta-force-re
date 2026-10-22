#include "dfre/animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

// Port of the original DF1 animation controller. Addresses are Df.exe
// (sha256 abbdf55a...) code offsets; instruction citations live in
// docs/animation-runtime.md and .local/reports.
namespace dfre {
namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}

// Arithmetic (sign preserving) right shift, matching the original SAR.
std::int32_t arithmetic_shift(std::int32_t value, unsigned bits) {
    if (bits == 0) return value;
    const auto raw = std::uint32_t(value);
    if (value >= 0) return std::int32_t(raw >> bits);
    const auto fill = ~std::uint32_t(0) << (32 - bits);
    return std::int32_t((raw >> bits) | fill);
}

// Original 25-bit angle wrap (neg/shl 7/sar 7 in 0x43a8a2 and friends).
std::int32_t wrap25(std::uint32_t value) {
    value &= 0x1ffffff;
    return value < 0x1000000 ? std::int32_t(value) : std::int32_t(value) - 0x2000000;
}

std::int32_t trunc_i32(double value) {
    const auto truncated = std::trunc(value);
    if (!(truncated > double(std::numeric_limits<std::int32_t>::min())))
        return std::numeric_limits<std::int32_t>::min() + 1;
    if (!(truncated < double(std::numeric_limits<std::int32_t>::max())))
        return std::numeric_limits<std::int32_t>::max();
    return std::int32_t(truncated);
}

std::int32_t signed_byte(std::uint8_t value) {
    return value < 0x80 ? std::int32_t(value) : std::int32_t(value) - 0x100;
}

// 0x42f1d0/0x42f3d0: 512 steps per full turn, amplitude 2^31, truncated and
// clamped to +-2^31-1 (verified against the executable tables).
template <class Function> const std::array<std::int32_t, 512>& sine_cosine_table(Function function) {
    static const auto table = [&function] {
        std::array<std::int32_t, 512> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const auto angle = double(index) * (2.0 * 3.14159265358979323846 / 512.0);
            auto value = std::int64_t(function(angle) * 2147483648.0);
            if (value > 2147483647) value = 2147483647;
            if (value < -2147483647) value = -2147483647;
            values[index] = std::int32_t(value);
        }
        return values;
    }();
    return table;
}
const std::array<std::int32_t, 512>& sine_table() {
    static const auto& table = sine_cosine_table([](double angle) { return std::sin(angle); });
    return table;
}
const std::array<std::int32_t, 512>& cosine_table() {
    static const auto& table = sine_cosine_table([](double angle) { return std::cos(angle); });
    return table;
}

// 0x43a730: rotate the x/z pair by the instance heading.
void rotate_frame_motion(std::array<std::int32_t, 3>& motion, std::int32_t heading) {
    const auto index = (std::uint32_t(heading) & 0x1ffffff) >> 16;
    const auto sine = sine_table()[index];
    const auto cosine = cosine_table()[index];
    const auto high_word = [](std::int64_t product) {
        // imul then add/adc: high 32 bits of the doubled 64-bit product.
        return std::int32_t(std::uint32_t(std::uint64_t(product) << 1 >> 32));
    };
    const auto x_cos = high_word(std::int64_t(motion[0]) * cosine);
    const auto z_sin = high_word(std::int64_t(motion[2]) * sine);
    const auto z_cos = high_word(std::int64_t(motion[2]) * cosine);
    const auto x_sin = high_word(std::int64_t(motion[0]) * sine);
    motion[0] = std::int32_t(std::uint32_t(x_cos) - std::uint32_t(z_sin));
    motion[2] = std::int32_t(std::uint32_t(x_sin) + std::uint32_t(z_cos));
}

bool token_separator(char value) {
    return value == ' ' || value == '\t' || value == ',';
}

// 0x4565f0: separators are space/tab/comma; '"' starts or ends a token and is
// removed; a token pointer is registered at its first character.
std::vector<std::string> tokenize_line(std::string_view line) {
    std::string buffer(line.substr(0, std::min<std::size_t>(line.size(), 1000)));
    std::vector<std::size_t> starts;
    bool in_quotes = false, pending = true;
    for (std::size_t position = 0; position < buffer.size(); ++position) {
        const auto value = buffer[position];
        if (value == '"') {
            in_quotes = !in_quotes;
            buffer[position] = '\0';
            pending = true;
            if (starts.size() >= 30) break;
            continue;
        }
        if (!in_quotes && token_separator(value)) {
            buffer[position] = '\0';
            pending = true;
            if (starts.size() >= 30) break;
            continue;
        }
        if (pending) {
            if (starts.size() >= 30) break;
            starts.push_back(position);
            pending = false;
        }
    }
    std::vector<std::string> tokens;
    tokens.reserve(starts.size());
    for (const auto start : starts) tokens.emplace_back(buffer.c_str() + start);
    return tokens;
}

// 0x49afa0 atoi: optional spaces and sign, digits, stop at the first
// non-digit; out of range values saturate instead of wrapping.
std::int32_t parse_atoi(std::string_view text) {
    std::size_t position = 0;
    while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) ++position;
    bool negative = false;
    if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
        negative = text[position] == '-';
        ++position;
    }
    std::int64_t value = 0;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
        value = value * 10 + (text[position] - '0');
        if (value > 0x7fffffffLL) value = 0x7fffffffLL;
        ++position;
    }
    return std::int32_t(negative ? -value : value);
}

// 0x42d0a7: trunc(atof(token) * 65536) / 30.
std::int32_t velocity_per_tick(double velocity) {
    return trunc_i32(velocity * 65536.0) / 30;
}

constexpr std::uint32_t kPoseOffset = 0x34; // [instance+0x58] for CHR1 (0x439e48)
constexpr std::int32_t kHeadingStep = 0x18000;
constexpr std::int32_t kHeadingLimit = 0x180000;

std::uint32_t raw_header_word(const std::array<std::uint8_t, 20>& header, std::size_t offset) {
    return std::uint32_t(header[offset]) | (std::uint32_t(header[offset + 1]) << 8) |
        (std::uint32_t(header[offset + 2]) << 16) | (std::uint32_t(header[offset + 3]) << 24);
}

} // namespace

const AnimationAction* AnimationDefinition::find(std::string_view name) const {
    for (auto action = actions.rbegin(); action != actions.rend(); ++action)
        if (action->name == name) return &*action;
    return nullptr;
}

AnimationDefinition decode_animation_definition(std::string_view text) {
    AnimationDefinition definition;
    std::size_t position = 0;
    while (position <= text.size()) {
        const auto newline = text.find('\n', position);
        auto line = text.substr(position,
            newline == std::string_view::npos ? text.size() - position : newline - position);
        position = newline == std::string_view::npos ? text.size() + 1 : newline + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const auto tokens = tokenize_line(line);
        if (tokens.empty() || tokens[0].front() == '/') continue; // 0x4568aa comment line
        if (tokens.size() < 2) continue;                         // name plus number required
        AnimationAction action;
        action.name = tokens[0];
        action.velocity_present = false;
        action.number = parse_atoi(tokens[1]);
        if (tokens.size() >= 3 && tokens[2].front() != '/') {
            action.velocity_present = true;
            action.velocity = std::strtod(tokens[2].c_str(), nullptr);
            action.velocity_per_tick = velocity_per_tick(action.velocity);
        }
        if (tokens.size() >= 4 && tokens[3].front() != '/' && tokens[3] == "override")
            action.override_flag = true;
        definition.actions.push_back(std::move(action));
    }
    return definition;
}

bool check_animation_condition(std::uint8_t opcode, bool carry, const AnimationConditionState& state) {
    (void)carry; // reserved for the 0x28/0x29 pair, unreachable in this build
    const auto bit = [&](std::uint32_t mask) { return (state.state_flags & mask) != 0; };
    const auto anim_bit = [&](std::uint32_t mask) { return (state.anim_flags & mask) != 0; };
    switch (opcode) {
        case 0x00: return true;
        case 0x01: return bit(0x01);
        case 0x02: return bit(0x02);
        case 0x03: return bit(0x04);
        case 0x04: return bit(0x08);
        case 0x05: return bit(0x20);
        case 0x06: return bit(0x40);
        case 0x08: return (state.anim_flags & 0x78) == 0;
        case 0x09: return anim_bit(0x08);
        case 0x0a: return !anim_bit(0x08);
        case 0x0b: return anim_bit(0x10);
        case 0x0c: return !anim_bit(0x10);
        case 0x0d: return anim_bit(0x20);
        case 0x0e: return !anim_bit(0x20);
        case 0x0f: return anim_bit(0x40);
        case 0x10: return !anim_bit(0x40);
        case 0x11: return (state.anim_flags & 0x18) == 0x18;
        case 0x12: return (state.anim_flags & 0x18) == 0;
        case 0x13: return (state.anim_flags & 0x60) == 0x60;
        case 0x14: return (state.anim_flags & 0x60) == 0;
        case 0x15: return (state.anim_flags & 0x28) == 0x28;
        case 0x16: return (state.anim_flags & 0x50) == 0x50;
        case 0x17: return (state.anim_flags & 0x78) != 0;
        case 0x18: case 0x19: case 0x1a: case 0x1b:
        case 0x1c: case 0x1d: case 0x1e: case 0x1f:
            return state.stance == std::uint8_t(opcode - 0x18);
        case 0x20: return bit(0x10);
        case 0x21: return state.extra_4c + state.motion_y < -0x3000;
        case 0x22: return state.switch_flag != 0;
        case 0x23: return bit(0x80);
        case 0x24: return state.extra_4c + state.motion_y < -0xc00;
        case 0x25: return state.aim_a != 0;
        case 0x26: return state.aim_b != 0;
        case 0x27: return (state.byte79 & 1) == 0;
        // The `cmp al, 0x28/0x29` in front of both cases always clears CF, so
        // the branches fail for every caller in this build.
        case 0x28: case 0x29: return false;
        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x34: case 0x35: case 0x36: case 0x37: {
            std::int32_t target = 0;
            switch (opcode) {
                case 0x35: target = 7; break;
                case 0x36: target = 6; break;
                case 0x37: target = 5; break;
                default: target = std::int32_t(opcode - 0x30); break;
            }
            auto value = state.heading - (std::int32_t(state.byte78) << 23) + 0x200000;
            value = arithmetic_shift(value, 0x16) & 7;
            return value == target;
        }
        case 0x38: return signed_byte(state.pitch) > -10 && signed_byte(state.pitch) < 10;
        case 0x39: return signed_byte(state.pitch) >= 10 && signed_byte(state.pitch) < 31;
        case 0x3a: return signed_byte(state.pitch) > -31 && signed_byte(state.pitch) <= -10;
        case 0x3b: return signed_byte(state.pitch) >= 31;
        case 0x3c: return signed_byte(state.pitch) <= -31;
        case 0x40: return true;  // 0x43b2b0 is a zero-returning stub in this build
        default: return true;    // 0x43b2a7
    }
}

std::uint8_t animation_random_selector(std::int32_t heading, std::int32_t accumulator,
    std::uint32_t random_a, std::uint32_t random_b) {
    auto value = std::int32_t(std::uint32_t(accumulator) + std::uint32_t(heading));
    value = arithmetic_shift(value, 16);
    value = arithmetic_shift(value + 0x20, 6);
    const auto group = std::uint8_t(std::uint32_t(value) & 7);
    const auto high = std::uint8_t(std::uint32_t(random_a & 3) << 3);
    const auto low = std::uint8_t(random_b & 7);
    return std::uint8_t((group << 5) | high | low);
}

AnimationController::AnimationController(const CharacterModel& prepared) : model_(&prepared) {
    require(prepared.runtime_prepared, "animation controller requires a prepared character");
    require(!prepared.bone_map.empty(), "animation controller requires a bone map");
    bone_count_ = prepared.bone_map.size();
    root_capacity_ = 8; // 0x60-byte root region per 0x439e48
    for (const auto& animation : prepared.animations) {
        require(animation.raw_header.size() == 20, "animation header is truncated");
        root_capacity_ = std::max(root_capacity_, std::size_t(
            raw_header_word(animation.raw_header, 8)));
    }
    restart();
}

const CharacterAnimation* AnimationController::animation() const {
    if (animation_index_ < 0) return nullptr;
    return &model_->animations[std::size_t(animation_index_)];
}
const CharacterFrame* AnimationController::frame() const {
    const auto* current = animation();
    if (!current || frame_index_ < 0) return nullptr;
    return &current->frames[std::size_t(frame_index_)];
}
std::int32_t AnimationController::frame_duration(const CharacterFrame& frame) const {
    return frame.raw_header[8] | (std::int32_t(frame.raw_header[9]) << 8);
}
std::int32_t AnimationController::frame_flags(const CharacterFrame& frame) const {
    return std::int32_t(frame_word(frame, 4));
}
std::uint8_t AnimationController::frame_byte(const CharacterFrame& frame, std::size_t offset) const {
    require(offset < frame.raw_header.size(), "character frame field is outside the header");
    return frame.raw_header[offset];
}
std::uint32_t AnimationController::frame_word(const CharacterFrame& frame, std::size_t offset) const {
    require(offset + 4 <= frame.raw_header.size(), "character frame word is outside the header");
    return std::uint32_t(frame.raw_header[offset]) |
        (std::uint32_t(frame.raw_header[offset + 1]) << 8) |
        (std::uint32_t(frame.raw_header[offset + 2]) << 16) |
        (std::uint32_t(frame.raw_header[offset + 3]) << 24);
}
std::uint32_t AnimationController::raw_u32(std::uint32_t offset) const {
    if (std::size_t(offset) + 4 > model_->raw.size())
        throw std::invalid_argument("character offset outside file: " +
            std::to_string(offset) + " (size " + std::to_string(model_->raw.size()) + ")");
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
        value |= std::uint32_t(model_->raw[std::size_t(offset) + index]) << (8 * index);
    return value;
}

void AnimationController::restart() {
    animation_index_ = -1;
    frame_index_ = -1;
    frame_ticks_ = 0;
    root_count_ = 0;
    pose_.assign(bone_count_, CharacterBonePose{});
    pose_delta_.assign(bone_count_, CharacterBonePose{});
    root_base_.assign(root_capacity_, std::array<std::int32_t, 3>{});
    root_delta_.assign(root_capacity_, std::array<std::int32_t, 3>{});
    motion_ = {0, 0, 0};
    accumulator_ = 0;
    condition_current_ = condition_previous_ = condition_latched_ = 0;
    event_bits_ = 0;
    random_a_ = random_b_ = 0;
    switch_flag_ = 0;
    last_events_ = 0;
    heading_ = 0;
    last_correction_ = 0;
    move_extra_ = 0;
    aim_ = {0, 0, 0};
    inputs_ = AnimationInputs{};
    // 0x43a0e0(0, 0): seed the pose buffer from animation slot 0 frame 0.
    for (const auto& animation : model_->animations)
        if (animation.slot == 0 && !animation.frames.empty()) {
            pose_ = character_frame_pose(animation.frames.front());
            require(pose_.size() == bone_count_, "seeded character pose count differs from the map");
            break;
        }
    select_action(0, 0);
}

std::int32_t AnimationController::animation_slot() const {
    const auto* current = animation();
    return current ? std::int32_t(current->slot) : -1;
}

bool AnimationController::select_action(std::uint32_t animation_slot, std::uint32_t frame_index) {
    const CharacterAnimation* found = nullptr;
    for (const auto& animation : model_->animations) if (animation.slot == animation_slot) {
        found = &animation;
        break;
    }
    if (!found) return false; // 0x43a159: carry set, instance untouched
    const auto* previous = frame(); // [esi+0x10] before the animation changes
    animation_index_ = int(found - model_->animations.data());
    root_count_ = std::size_t(raw_header_word(found->raw_header, 8));
    require(root_count_ <= root_capacity_, "character root count exceeds the instance region");
    require(!found->frames.empty(), "character animation has no frames");
    // 0x43a166: walk frame strides; the count stays on the last frame.
    std::size_t index = 0;
    auto remaining = frame_index;
    if (remaining != 0) {
        do {
            const auto& step = found->frames[index];
            if (step.next_stride != 0 && index + 1 < found->frames.size()) ++index;
        } while (--remaining > 0);
    }
    switch_flag_ = 0; // 0x43a162
    set_frame(index, previous);
    return true;
}

bool AnimationController::select_action(const AnimationDefinition& definition,
    std::string_view action_name, std::uint32_t frame_index) {
    const auto* action = definition.find(action_name);
    if (!action) return false;
    return select_action(std::uint32_t(action->number), frame_index);
}

std::uint32_t AnimationController::frame_mask() const {
    const auto* current = frame();
    return current ? std::uint32_t(frame_flags(*current)) : 0u;
}

void AnimationController::set_frame(std::size_t new_index, const CharacterFrame* previous) {
    const auto* current = animation();
    require(current != nullptr, "character frame selection requires an animation");
    require(new_index < current->frames.size(), "character frame index is outside the animation");
    const auto& next = current->frames[new_index];
    std::array<std::int32_t, 3> step{0, 0, 0};
    if (previous) {
        // 0x43a690: (previous roots - next roots) + next frame motion, rotated.
        const auto index = frame_byte(next, 0x18);
        const auto previous_table = frame_word(*previous, 0x20);
        const auto next_table = frame_word(next, 0x20);
        for (std::size_t component = 0; component < 3; ++component) {
            const auto previous_root = raw_u32(previous_table + std::uint32_t(index) * 12 +
                std::uint32_t(component) * 4);
            const auto next_root = raw_u32(next_table + std::uint32_t(index) * 12 +
                std::uint32_t(component) * 4);
            step[component] = std::int32_t(std::uint32_t(previous_root) -
                std::uint32_t(next_root) + frame_word(next, 0x0c + component * 4));
        }
        rotate_frame_motion(step, heading_);
        for (std::size_t bone = 0; bone < root_count_; ++bone)
            for (std::size_t component = 0; component < 3; ++component)
                root_base_[bone][component] = std::int32_t(
                    raw_u32(previous_table + std::uint32_t(bone) * 12 + std::uint32_t(component) * 4));
    }
    frame_index_ = int(new_index);
    frame_ticks_ = std::uint32_t(frame_duration(next));
    require(frame_ticks_ != 0, "character frame duration is zero");
    compute_pose_delta(next, step, frame_ticks_ != 1);
}

void AnimationController::compute_pose_delta(const CharacterFrame& next,
    const std::array<std::int32_t, 3>& step, bool timed) {
    const auto* current = animation();
    require(current != nullptr, "character deltas require an animation");
    const auto ticks = std::int32_t(frame_ticks_);
    require(ticks > 0, "character delta divisor is zero");
    require(next.packed_angles.size() == bone_count_, "character frame pose count differs from the map");
    // 0x43a2c7 / 0x43a4dc scale targets.
    const auto scale_table = frame_word(next, 0x28);
    const bool use_scale_table = kPoseOffset >= 0x29 && scale_table != 0;
    for (std::size_t bone = 0; bone < bone_count_; ++bone) {
        for (std::size_t component = 0; component < 3; ++component) {
            std::int32_t target = 0x10000;
            if (use_scale_table)
                target = std::int32_t((raw_u32(scale_table + std::uint32_t(bone) * 4) >>
                    (8 * component)) & 0xff) * 0x1000;
            auto difference = std::int32_t(std::uint32_t(target) -
                std::uint32_t(pose_[bone].scale[component]));
            if (timed) difference /= ticks;
            pose_delta_[bone].scale[component] = difference;
        }
    }
    // Frame pose bytes start at frame + [instance+0x58] (0x34 for CHR1).
    for (std::size_t bone = 0; bone < bone_count_; ++bone)
        for (std::size_t component = 0; component < 3; ++component) {
            auto difference = std::int32_t(std::uint32_t(next.packed_angles[bone][component]) << 17) -
                pose_[bone].angles[component];
            difference = arithmetic_shift(std::int32_t(std::uint32_t(difference) << 7), 7);
            if (timed) difference /= ticks;
            pose_delta_[bone].angles[component] = difference;
        }
    // 0x43a414: frames flagged 0x80 bake the pitch byte into bones 1 and 2.
    if (frame_flags(next) & 0x80) {
        require(bone_count_ >= 3, "pitch adjustment requires three bones");
        const auto pitch = std::uint32_t(inputs_.pitch);
        auto bone_two = std::int32_t(std::uint32_t(pitch) << 17) - pose_[2].angles[1];
        bone_two = arithmetic_shift(std::int32_t(std::uint32_t(bone_two) << 7), 7);
        if (timed) bone_two /= ticks;
        pose_delta_[2].angles[1] = bone_two;
        auto bone_one = std::int32_t(std::uint32_t(pitch >> 2) << 17);
        bone_one = arithmetic_shift(std::int32_t(std::uint32_t(bone_one) << 7), 7);
        if (timed) bone_one /= ticks;
        pose_delta_[1].angles[1] = std::int32_t(std::uint32_t(pose_delta_[1].angles[1]) +
            std::uint32_t(bone_one));
    }
    // 0x43a489: per-tick root positions and root motion.
    const auto root_table = frame_word(next, 0x20);
    for (std::size_t bone = 0; bone < root_count_; ++bone)
        for (std::size_t component = 0; component < 3; ++component) {
            auto difference = std::int32_t(std::uint32_t(raw_u32(root_table +
                std::uint32_t(bone) * 12 + std::uint32_t(component) * 4)) -
                std::uint32_t(root_base_[bone][component]));
            if (timed) difference /= ticks;
            root_delta_[bone][component] = difference;
        }
    for (std::size_t component = 0; component < 3; ++component) {
        auto value = step[component];
        if (timed) value /= ticks;
        motion_[component] = value;
    }
}

void AnimationController::advance_state() {
    // 0x43a7da: latch newly set condition bits into 0x40.
    const auto previous_conditions = condition_previous_;
    condition_previous_ = condition_current_;
    condition_latched_ |= condition_current_ & ~previous_conditions;
    // 0x43a7ea: pose and root advance.
    pose_ = advance_character_pose(pose_, pose_delta_);
    for (std::size_t bone = 0; bone < root_count_; ++bone)
        for (std::size_t component = 0; component < 3; ++component)
            root_base_[bone][component] = std::int32_t(std::uint32_t(root_base_[bone][component]) +
                std::uint32_t(root_delta_[bone][component]));
    // 0x43a874: heading servo; the frame byte 0x19 branch is degenerate (3).
    const auto* current = animation();
    const auto correction = current ? std::int32_t(raw_header_word(current->raw_header, 12)) : 0;
    auto difference = wrap25(std::uint32_t(0) -
        (accumulator_ + std::uint32_t(heading_)));
    difference = std::int32_t(std::uint32_t(difference) & 0xffff0000u);
    if (difference > 0) {
        if (difference > kHeadingLimit) difference = kHeadingLimit;
        heading_ = std::int32_t(std::uint32_t(heading_) + std::uint32_t(difference));
    } else if (difference < 0) {
        if (difference < -kHeadingLimit) difference = -kHeadingLimit;
        heading_ = std::int32_t(std::uint32_t(heading_) + std::uint32_t(difference));
    }
    difference = wrap25(std::uint32_t(0) - (accumulator_ + std::uint32_t(heading_)));
    if (difference > kHeadingStep) {
        if (correction == 0) accumulator_ += std::uint32_t(kHeadingStep);
    } else if (difference < -kHeadingStep) {
        if (correction == 0) accumulator_ -= std::uint32_t(kHeadingStep);
    }
}

AnimationTickResult AnimationController::tick(const AnimationInputs& inputs) {
    inputs_ = inputs;
    // 0x42ce40: object angles and heading conversions.
    const auto convert = [](std::int32_t value) {
        const auto product = std::int64_t(value) * 0x8c6d;
        return std::int32_t(std::uint32_t(std::uint64_t(product) >> 16));
    };
    aim_[0] = arithmetic_shift(-convert(inputs.object_angles[1]), 4); // [object+0x28]
    aim_[1] = arithmetic_shift(convert(inputs.object_angles[2]), 4);  // [object+0x2c]
    aim_[2] = arithmetic_shift(-convert(inputs.object_angles[0]), 4); // [object+0x24]
    accumulator_ = std::uint32_t(inputs.object_heading) >> 7;
    heading_ = std::int32_t((std::uint32_t(0) - std::uint32_t(inputs.object_heading)) >> 7);
    last_correction_ = 0;
    last_events_ = 0;
    event_bits_ = 0;
    switch_flag_ = inputs.switch_flag;
    if (inputs.write_random_selector) {
        random_a_ = inputs.random_a;
        random_b_ = inputs.random_b;
    }
    if (inputs.write_condition_state) condition_current_ = inputs.condition_state;
    require(has_animation() && frame() != nullptr, "animation controller tick requires an animation");
    advance_state();
    const auto& current = *frame();
    if (frame_flags(current) & 0x40) event_bits_ |= 1;
    --frame_ticks_; // original `dec [esi+0x18]` also wraps after a failed action select
    if (frame_ticks_ != 0) {
        run_conditions(0);
        std::uint16_t events = 0;
        if (frame_flags(current) & 0x10) events |= frame_byte(current, 0x0a);
        if (frame_flags(current) & 0x20) events |= std::uint16_t(frame_byte(current, 0x0b)) << 8;
        return {events, false, heading_};
    }
    last_events_ = std::uint16_t(frame_byte(current, 0x0a) |
        (std::uint16_t(frame_byte(current, 0x0b)) << 8));
    if (frame_flags(current) & 0x04) {
        random_a_ = 0;
        random_b_ = 0;
    }
    const auto extra = frame_word(current, 0x24);
    if (extra != 0) move_extra_ = std::int32_t(extra);
    const auto stride = frame_word(current, 0x00);
    const auto* current_animation = animation();
    if (stride == 0 && current_animation) {
        const auto correction = std::uint32_t(raw_header_word(current_animation->raw_header, 12));
        for (auto& pose : pose_)
            pose.angles[0] = std::int32_t((std::uint32_t(pose.angles[0]) + correction) & 0x1ffffff);
        heading_ = std::int32_t((std::uint32_t(heading_) - correction) & 0x1ffffff);
        last_correction_ = std::int32_t(correction);
    }
    if (run_conditions(1)) return {last_events_, true, heading_};
    if (stride != 0) {
        require(current_animation != nullptr && std::size_t(frame_index_ + 1) < current_animation->frames.size(),
            "character frame stride leaves the animation");
        set_frame(std::size_t(frame_index_ + 1), &current);
    } else {
        select_action(0, 0);
        condition_current_ &= 8;
        condition_latched_ = 0;
    }
    return {last_events_, false, heading_};
}

bool AnimationController::run_conditions(std::uint8_t mode) {
    const auto* current = frame();
    if (!current) return false;
    const auto count = frame_byte(*current, 0x1b);
    const auto offset = frame_word(*current, 0x1c);
    for (std::size_t entry = 0; entry < count; ++entry) {
        const auto base = offset + std::uint32_t(entry) * 8;
        const auto first_word = raw_u32(base);
        const auto second_word = raw_u32(base + 4);
        const auto e0 = std::uint8_t(first_word & 0xff);
        const auto e1 = std::uint8_t((first_word >> 8) & 0xff);
        const auto e2 = std::uint8_t((first_word >> 16) & 0xff);
        const auto e3 = std::uint8_t((first_word >> 24) & 0xff);
        const auto e4 = std::uint8_t(second_word & 0xff);
        const auto e5 = std::uint8_t((second_word >> 8) & 0xff);
        const auto e6 = std::uint8_t((second_word >> 16) & 0xff);
        if (((e3 & 0x20) | mode) == 0) continue;
        if (e3 & 0x01) {
            const auto value = random_selector_value();
            std::uint8_t mask = 0;
            if (e3 & 0x04) mask |= 0x07;
            if (e3 & 0x08) mask |= 0x18;
            if (e3 & 0x10) mask |= 0xe0;
            if ((value & mask) != std::uint8_t(e0 & mask)) continue;
        }
        // entry[1] state bits: a macro bit selects 0x38, otherwise latched 0x40.
        bool state_ok = true;
        for (std::size_t group = 0; group < 4 && state_ok; ++group) {
            const auto value_bit = std::uint8_t(1u << group);
            const auto macro_bit = std::uint8_t(0x10u << group);
            if (e1 & macro_bit) {
                const bool wanted = (e1 & value_bit) != 0;
                const bool active = ((condition_current_ >> group) & 1) != 0;
                state_ok = wanted == active;
            } else if (e1 & value_bit) {
                state_ok = ((condition_latched_ >> group) & 1) != 0;
            }
        }
        if (!state_ok) continue;
        const bool first = evaluate_condition(e2, inputs_.condition_carry);
        bool passed = false;
        switch (e3 & 0xc0) {
            case 0x00: passed = first; break;
            // The original leaves CF from the first test for the second call.
            case 0x40: passed = first && evaluate_condition(e6, !first); break;
            case 0x80: passed = first || evaluate_condition(e6, !first); break;
            default: passed = first != evaluate_condition(e6, !first); break;
        }
        if (!passed) continue;
        select_action(e4, std::uint32_t(e5) - 1); // 0x43abe8, failures still report the change
        condition_current_ &= 8;
        condition_latched_ = 0;
        return true;
    }
    return false;
}

bool AnimationController::evaluate_condition(std::uint8_t opcode, bool carry) const {
    return check_animation_condition(opcode, carry, condition_state());
}

AnimationConditionState AnimationController::condition_state() const {
    AnimationConditionState state;
    state.heading = heading_;
    state.accumulator = std::int32_t(accumulator_);
    state.motion_y = motion_[1];
    state.extra_4c = inputs_.extra_4c;
    state.state_flags = inputs_.state_flags;
    state.anim_flags = inputs_.anim_flags;
    state.stance = inputs_.stance;
    state.pitch = inputs_.pitch;
    state.byte78 = inputs_.byte78;
    state.byte79 = inputs_.byte79;
    state.aim_a = inputs_.aim_a;
    state.aim_b = inputs_.aim_b;
    state.switch_flag = switch_flag_;
    return state;
}

std::uint8_t AnimationController::random_selector_value() const {
    return animation_random_selector(heading_, std::int32_t(accumulator_), random_a_, random_b_);
}

AnimationStatus AnimationController::status() const {
    AnimationStatus result;
    const auto* current = animation();
    const auto* current_frame = frame();
    if (!current || !current_frame) return result;
    result.animation_slot = std::int32_t(current->slot);
    result.frame_index = frame_index_;
    result.frame_ticks_elapsed = frame_duration(*current_frame) - std::int32_t(frame_ticks_);
    return result;
}

} // namespace dfre
