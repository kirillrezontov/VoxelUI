#pragma once
#include "resources.hpp"
namespace voxelui {
inline namespace v1 {
struct GlyphBitmap {
    int width{}, height{}, left{}, top{};
    float advance{};
    std::vector<std::uint8_t> alpha;
};
struct PositionedGlyph {
    float x{}, y{};
    std::shared_ptr<const GlyphBitmap> bitmap;
};
struct TextLayout {
    Vec2 size;
    std::vector<PositionedGlyph> glyphs;
};
std::vector<char32_t> decodeUtf8(std::string_view text);
std::string encodeUtf8(char32_t);
std::vector<std::size_t> utf8Boundaries(std::string_view);
class TextSystem {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit TextSystem(Resources &);
    ~TextSystem();
    TextSystem(const TextSystem &) = delete;
    TextSystem &operator=(const TextSystem &) = delete;
    std::shared_ptr<const TextLayout> layout(std::string_view text, std::string_view font, float size,
                                             float width = 100000, std::string_view align = "left",
                                             int weight = 400);
    bool validateFont(std::string_view id);
    void clear();
};
} // namespace v1
} // namespace voxelui
