#pragma once
#include "types.hpp"
#include <filesystem>
#include <functional>
#include <set>
#include <span>
namespace voxelui {
inline namespace v1 {
bool validResourceId(std::string_view id);
struct Resource {
    std::string id;
    std::vector<std::uint8_t> bytes;
    std::uint64_t revision{};
};
using ResourcePtr = std::shared_ptr<const Resource>;
using ResourceLoader = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view)>;
struct Image {
    int width{}, height{};
    std::vector<std::uint8_t> rgba;
};
class Resources {
    struct Provider {
        std::size_t token;
        int priority;
        ResourceLoader load;
    };
    std::vector<Provider> providers_;
    std::map<std::string, ResourcePtr> cache_;
    std::uint64_t revision_{}, epoch_{};
    std::size_t next_{1}, cacheBytes_{};
    std::set<std::string> requested_;

  public:
    std::size_t mount(ResourceLoader loader, int priority = 0);
    std::size_t mountDirectory(std::string nameSpace, std::filesystem::path directory, int priority = 0);
    void unmount(std::size_t token);
    ResourcePtr get(std::string_view id, Diagnostics *diagnostics = nullptr);
    bool refresh(); // content comparison; emits a new epoch only when mounted content changes
    void invalidate(std::string_view id);
    std::uint64_t epoch() const {
        return epoch_;
    }
    void clear();
};
std::shared_ptr<const Image> decodeImage(const Resource &, Diagnostics &);
} // namespace v1
} // namespace voxelui
