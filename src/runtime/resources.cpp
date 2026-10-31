#include <charconv>
#include <fstream>
#include <limits>
#include <png.h>
#include <voxelui/resources.hpp>
namespace voxelui {
inline namespace v1 {
std::string trim(std::string_view t) {
    auto a = t.find_first_not_of(" \n\r\t");
    if (a == t.npos)
        return {};
    return std::string(t.substr(a, t.find_last_not_of(" \n\r\t") - a + 1));
}
std::optional<double> number(std::string_view text) {
    double d{};
    auto [p, e] = std::from_chars(text.data(), text.data() + text.size(), d);
    if (e != std::errc{} || p != text.data() + text.size() || !std::isfinite(d) || std::abs(d) > 1e9)
        return {};
    return d;
}
bool validResourceId(std::string_view id) {
    auto c = id.find(':');
    if (c == 0 || c == id.npos || c + 1 == id.size() || id.size() > 512)
        return false;
    for (char v : id)
        if (!((v >= 'a' && v <= 'z') || (v >= 'A' && v <= 'Z') || (v >= '0' && v <= '9') || v == '_' ||
              v == '-' || v == '.' || v == ':' || v == '/'))
            return false;
    if (id.find(':', c + 1) != id.npos)
        return false;
    auto p = id.substr(c + 1);
    if (p.front() == '/' || p.back() == '/')
        return false;
    while (!p.empty()) {
        auto k = p.find('/');
        auto s = p.substr(0, k);
        if (s.empty() || s == "." || s == "..")
            return false;
        if (k == p.npos)
            break;
        p.remove_prefix(k + 1);
    }
    return true;
}
std::size_t Resources::mount(ResourceLoader loader, int priority) {
    auto t = next_++;
    providers_.push_back({t, priority, std::move(loader)});
    std::stable_sort(providers_.begin(), providers_.end(), [](auto &a, auto &b) {
        return a.priority > b.priority || (a.priority == b.priority && a.token > b.token);
    });
    clear();
    return t;
}
std::size_t Resources::mountDirectory(std::string ns, std::filesystem::path directory, int priority) {
    auto root = std::filesystem::weakly_canonical(directory);
    return mount(
        [ns = std::move(ns), root](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
            if (!validResourceId(id) || !id.starts_with(ns + ":"))
                return {};
            std::error_code ec;
            auto path = std::filesystem::weakly_canonical(root / std::string(id.substr(ns.size() + 1)), ec);
            if (ec)
                return {};
            auto rel = path.lexically_relative(root);
            if (rel.empty() || *rel.begin() == ".." || rel.is_absolute())
                return {};
            auto size = std::filesystem::file_size(path, ec);
            if (ec || size > 16 * 1024 * 1024)
                return {};
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return {};
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
            if (size &&
                !stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size)))
                return {};
            return bytes;
        },
        priority);
}
void Resources::unmount(std::size_t t) {
    std::erase_if(providers_, [&](auto &p) { return p.token == t; });
    clear();
}
ResourcePtr Resources::get(std::string_view id, Diagnostics *d) {
    auto key = std::string(id);
    if (!validResourceId(id)) {
        if (d)
            d->push_back({Severity::error, key, 1, 1,
                          "Invalid resource ID; expected namespace:relative/path without traversal"});
        return {};
    }
    if (requested_.size() >= 4096 && !requested_.contains(key)) {
        if (d)
            d->push_back({Severity::error, key, 1, 1, "Resource count budget exceeded"});
        return {};
    }
    requested_.insert(key);
    if (auto i = cache_.find(key); i != cache_.end())
        return i->second;
    for (auto &p : providers_) {
        std::optional<std::vector<std::uint8_t>> b;
        try {
            b = p.load(id);
        } catch (...) {
            if (d)
                d->push_back({Severity::error, key, 1, 1, "Resource provider threw an exception"});
            return {};
        }
        if (b) {
            if (b->size() > 16 * 1024 * 1024 || cacheBytes_ + b->size() > 128 * 1024 * 1024) {
                if (d)
                    d->push_back({Severity::error, key, 1, 1, "Resource byte budget exceeded"});
                return {};
            }
            cacheBytes_ += b->size();
            auto r = std::make_shared<Resource>(Resource{key, std::move(*b), ++revision_});
            cache_[key] = r;
            return r;
        }
    }
    if (d)
        d->push_back({Severity::error, key, 1, 1, "Resource not found in mounted providers"});
    return {};
}
bool Resources::refresh(Diagnostics *diagnostics) {
    bool changed = false;
    for (auto &id : requested_) {
        auto it = cache_.find(id);
        std::optional<std::vector<std::uint8_t>> bytes;
        bool failed = false;
        for (auto &p : providers_) {
            try {
                bytes = p.load(id);
            } catch (...) {
                if (diagnostics)
                    diagnostics->push_back(
                        {Severity::error, id, 1, 1,
                         "Resource provider threw during refresh; previous resource retained"});
                failed = true;
                break;
            }
            if (bytes)
                break;
        }
        if (failed)
            continue;
        auto previous = it == cache_.end() ? 0 : it->second->bytes.size();
        if (!bytes || bytes->size() > 16 * 1024 * 1024 ||
            cacheBytes_ - previous + bytes->size() > 128 * 1024 * 1024) {
            if (it != cache_.end()) {
                cacheBytes_ -= previous;
                cache_.erase(it);
                changed = true;
            }
        } else if (it == cache_.end() || *bytes != it->second->bytes) {
            cacheBytes_ = cacheBytes_ - previous + bytes->size();
            cache_[id] = std::make_shared<Resource>(Resource{id, std::move(*bytes), ++revision_});
            changed = true;
        }
    }
    if (changed)
        ++epoch_;
    return changed;
}
void Resources::invalidate(std::string_view id) {
    if (auto it = cache_.find(std::string(id)); it != cache_.end()) {
        cacheBytes_ -= it->second->bytes.size();
        cache_.erase(it);
    }
    ++epoch_;
}
void Resources::clear() {
    cache_.clear();
    requested_.clear();
    cacheBytes_ = 0;
    ++epoch_;
}
std::shared_ptr<const Image> decodeImage(const Resource &r, Diagnostics &d) {
    png_image p{};
    p.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&p, r.bytes.data(), r.bytes.size())) {
        d.push_back({Severity::error, r.id, 1, 1, "Invalid PNG image"});
        return {};
    }
    if (p.width == 0 || p.height == 0 || p.width > 8192 || p.height > 8192 ||
        std::uint64_t(p.width) * p.height > 16000000) {
        png_image_free(&p);
        d.push_back({Severity::error, r.id, 1, 1, "PNG exceeds image limits"});
        return {};
    }
    p.format = PNG_FORMAT_RGBA;
    auto image = std::make_shared<Image>();
    image->width = int(p.width);
    image->height = int(p.height);
    image->rgba.resize(PNG_IMAGE_SIZE(p));
    if (!png_image_finish_read(&p, nullptr, image->rgba.data(), 0, nullptr)) {
        png_image_free(&p);
        d.push_back({Severity::error, r.id, 1, 1, "Unable to decode PNG pixels"});
        return {};
    }
    png_image_free(&p);
    return image;
}
} // namespace v1
} // namespace voxelui
