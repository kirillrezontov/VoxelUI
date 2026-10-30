#pragma once
#include "resources.hpp"
#include <set>
namespace voxelui {
inline namespace v1 {
struct Node {
    std::string tag, id, text, source;
    int line{1};
    std::map<std::string, std::string> attributes;
    std::vector<Node> children;
    std::string attr(std::string_view key, std::string fallback = {}) const;
};
struct Document {
    Node root;
    std::map<std::string, Node> templates;
};
struct ParseResult {
    std::optional<Document> document;
    Diagnostics diagnostics;
    explicit operator bool() const {
        return document.has_value() && !hasErrors(diagnostics);
    }
};
ParseResult parseDocument(std::string_view xml, std::string source = "<memory>",
                          const std::set<std::string> &customTags = {}, Limits limits = {});
bool builtinTag(std::string_view tag);
// Private parser boundary shared by XML and SVG; DTD/entity declarations are rejected before parsing.
namespace detail {
std::optional<Node> parseXml(std::string_view, std::string, Diagnostics &, Limits);
}
} // namespace v1
} // namespace voxelui
