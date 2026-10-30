#include <ft2build.h>
#include <voxelui/text.hpp>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include <tuple>
namespace voxelui {
inline namespace v1 {
std::vector<char32_t> decodeUtf8(std::string_view s) {
    std::vector<char32_t> out;
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        std::size_t len = c < 128                    ? 1
                          : (c >= 0xc2 && c <= 0xdf) ? 2
                          : (c >= 0xe0 && c <= 0xef) ? 3
                          : (c >= 0xf0 && c <= 0xf4) ? 4
                                                     : 0;
        char32_t cp = len == 1 ? c : len ? c & ((1u << (7 - len)) - 1) : 0xfffd;
        bool ok = len && i + len <= s.size();
        for (std::size_t j = 1; j < len && ok; ++j) {
            unsigned char b = s[i + j];
            if ((b & 0xc0) != 0x80)
                ok = false;
            else
                cp = (cp << 6) | (b & 63);
        }
        if (ok && ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) ||
                   (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff))
            ok = false;
        out.push_back(ok ? cp : 0xfffd);
        i += ok ? len : 1;
    }
    return out;
}
std::string encodeUtf8(char32_t c) {
    if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        c = 0xfffd;
    std::string s;
    if (c < 128)
        s += char(c);
    else if (c < 2048) {
        s += char(0xc0 | (c >> 6));
        s += char(0x80 | (c & 63));
    } else if (c < 65536) {
        s += char(0xe0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    } else {
        s += char(0xf0 | (c >> 18));
        s += char(0x80 | ((c >> 12) & 63));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
    return s;
}
std::vector<std::size_t> utf8Boundaries(std::string_view text) {
    std::vector<std::size_t> result{0};
    for (std::size_t i = 0; i < text.size();) {
        unsigned char lead = text[i];
        std::size_t length = lead < 128                     ? 1
                             : lead >= 0xc2 && lead <= 0xdf ? 2
                             : lead >= 0xe0 && lead <= 0xef ? 3
                             : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                            : 1;
        length = std::min(length, text.size() - i);
        auto decoded = decodeUtf8(text.substr(i, length));
        char32_t cp = decoded.empty() ? 0xfffd : decoded[0];
        if (decoded.size() != 1 || cp == 0xfffd)
            length = 1;
        if (i && !(cp >= 0x300 && cp <= 0x36f))
            result.push_back(i);
        i += length;
    }
    if (result.back() != text.size())
        result.push_back(text.size());
    return result;
}
struct TextSystem::Impl {
    Resources &resources;
    FT_Library library{};
    struct Face {
        ResourcePtr resource;
        FT_Face face{};
    };
    std::map<std::string, Face> faces;
    std::map<std::tuple<std::string, std::uint64_t, int, int, char32_t>, std::shared_ptr<const GlyphBitmap>>
        glyphs;
    std::map<std::tuple<std::string, std::string, int, int, std::string, int>,
             std::shared_ptr<const TextLayout>>
        layouts;
    std::map<std::string, std::pair<std::uint64_t, bool>> validated;
    std::uint64_t epoch{};
    std::size_t layoutGlyphs{};
    explicit Impl(Resources &r) : resources(r) {
        FT_Init_FreeType(&library);
    }
    void clear() {
        layouts.clear();
        layoutGlyphs = 0;
        glyphs.clear();
        for (auto &[k, f] : faces)
            if (f.face)
                FT_Done_Face(f.face);
        faces.clear();
    }
    ~Impl() {
        clear();
        if (library)
            FT_Done_FreeType(library);
    }
    void invalidate() {
        layouts.clear();
        glyphs.clear();
        layoutGlyphs = 0;
    }
    Face *font(std::string name) {
        auto existing = faces.find(name);
        auto resource = resources.get(name);
        if (existing != faces.end() &&
            (!resource || resource->revision == existing->second.resource->revision))
            return &existing->second;
        if (!resource || !library)
            return existing == faces.end() ? nullptr : &existing->second;
        FT_Face face{};
        if (FT_New_Memory_Face(library, resource->bytes.data(), FT_Long(resource->bytes.size()), 0, &face))
            return existing == faces.end() ? nullptr : &existing->second;
        if (existing != faces.end()) {
            FT_Done_Face(existing->second.face);
            existing->second = {resource, face};
            return &existing->second;
        }
        if (faces.size() >= 32) {
            auto first = faces.begin();
            FT_Done_Face(first->second.face);
            faces.erase(first);
        }
        return &faces.emplace(name, Face{resource, face}).first->second;
    }
    std::shared_ptr<const GlyphBitmap> glyph(std::string name, int size, char32_t cp, int weight) {
        auto *f = font(name);
        if (!f && name != "ui:font") {
            name = "ui:font";
            f = font(name);
        }
        if (f && FT_Get_Char_Index(f->face, FT_ULong(cp)) == 0 && name != "ui:font") {
            name = "ui:font";
            f = font(name);
        }
        auto key = std::make_tuple(name, f ? f->resource->revision : 0, size, weight, cp);
        if (auto i = glyphs.find(key); i != glyphs.end())
            return i->second;
        auto g = std::make_shared<GlyphBitmap>();
        g->advance = size * .6f;
        if (f && !FT_Set_Pixel_Sizes(f->face, 0, FT_UInt(size)) &&
            !FT_Load_Char(f->face, FT_ULong(cp), FT_LOAD_DEFAULT)) {
            if (weight >= 600)
                FT_GlyphSlot_Embolden(f->face->glyph);
            if (FT_Render_Glyph(f->face->glyph, FT_RENDER_MODE_NORMAL))
                return g;
            auto slot = f->face->glyph;
            g->width = int(slot->bitmap.width);
            g->height = int(slot->bitmap.rows);
            g->left = slot->bitmap_left;
            g->top = slot->bitmap_top;
            g->advance = slot->advance.x / 64.f;
            if (g->width > 1024 || g->height > 1024)
                return g;
            g->alpha.resize(std::size_t(g->width) * g->height);
            for (int y = 0; y < g->height; ++y)
                for (int x = 0; x < g->width; ++x) {
                    int row = slot->bitmap.pitch < 0 ? g->height - 1 - y : y;
                    g->alpha[std::size_t(y) * g->width + x] =
                        slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO
                            ? ((slot->bitmap.buffer[row * std::abs(slot->bitmap.pitch) + x / 8] >>
                                (7 - x % 8)) &
                               1) *
                                  255
                        : slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY
                            ? slot->bitmap.buffer[row * std::abs(slot->bitmap.pitch) + x]
                            : 0;
                }
        } else if (cp != ' ') {
            g->width = std::max(1, size / 2);
            g->height = std::max(1, size);
            g->top = size;
            g->alpha.assign(std::size_t(g->width) * g->height, 0);
            for (int y = 0; y < g->height; ++y)
                for (int x = 0; x < g->width; ++x)
                    if (x == 0 || y == 0 || x == g->width - 1 || y == g->height - 1)
                        g->alpha[std::size_t(y) * g->width + x] = 255;
        }
        if (glyphs.size() > 4096)
            glyphs.clear();
        glyphs[key] = g;
        return g;
    }
};
TextSystem::TextSystem(Resources &r) : impl_(std::make_unique<Impl>(r)) {}
TextSystem::~TextSystem() = default;
bool TextSystem::validateFont(std::string_view id) {
    auto resource = impl_->resources.get(id);
    if (!resource || !impl_->library)
        return false;
    auto it = impl_->validated.find(std::string(id));
    if (it != impl_->validated.end() && it->second.first == resource->revision)
        return it->second.second;
    FT_Face face{};
    bool valid = !FT_New_Memory_Face(impl_->library, resource->bytes.data(), FT_Long(resource->bytes.size()),
                                     0, &face);
    if (face)
        FT_Done_Face(face);
    if (impl_->validated.size() >= 256)
        impl_->validated.clear();
    impl_->validated[std::string(id)] = {resource->revision, valid};
    return valid;
}
void TextSystem::clear() {
    impl_->invalidate();
}
std::shared_ptr<const TextLayout> TextSystem::layout(std::string_view text, std::string_view font, float size,
                                                     float width, std::string_view align, int weight) {
    if (impl_->epoch != impl_->resources.epoch()) {
        impl_->invalidate();
        impl_->epoch = impl_->resources.epoch();
    }
    text = text.substr(0, 65536);
    int px = std::isfinite(size) ? std::clamp(int(std::round(size)), 1, 256) : 18;
    width = std::clamp(width, 1.f, 100000.f);
    auto key =
        std::make_tuple(std::string(text), std::string(font), px, int(width), std::string(align), weight);
    if (auto it = impl_->layouts.find(key); it != impl_->layouts.end())
        return it->second;
    auto layout = std::make_shared<TextLayout>();
    auto cps = decodeUtf8(text.substr(0, 1024 * 1024));
    float x = 0, y = 0, lineHeight = px * 1.3f, maxX = 0;
    std::vector<std::pair<std::size_t, float>> lines;
    std::size_t lineStart = 0;
    for (auto cp : cps) {
        auto g = impl_->glyph(std::string(font), px, cp, weight);
        if (cp == '\n' || (x + g->advance > width && x > 0)) {
            lines.emplace_back(lineStart, x);
            lineStart = layout->glyphs.size();
            maxX = std::max(maxX, x);
            x = 0;
            y += lineHeight;
            if (cp == '\n')
                continue;
        }
        layout->glyphs.push_back({x + g->left, y + float(px) - g->top, g});
        x += g->advance;
    }
    lines.emplace_back(lineStart, x);
    maxX = std::max(maxX, x);
    layout->size = {maxX, y + lineHeight};
    for (std::size_t l = 0; l < lines.size(); ++l) {
        auto end = l + 1 < lines.size() ? lines[l + 1].first : layout->glyphs.size();
        float offset = align == "center"  ? (width - lines[l].second) * .5f
                       : align == "right" ? width - lines[l].second
                                          : 0;
        for (auto i = lines[l].first; i < end; ++i)
            layout->glyphs[i].x += offset;
    }
    if (impl_->layouts.size() > 2048 || impl_->layoutGlyphs + layout->glyphs.size() > 1000000) {
        impl_->layouts.clear();
        impl_->layoutGlyphs = 0;
    }
    impl_->layoutGlyphs += layout->glyphs.size();
    impl_->layouts[key] = layout;
    return layout;
}
} // namespace v1
} // namespace voxelui
