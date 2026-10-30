#include <functional>
#include <libxml/parser.h>
#include <voxelui/document.hpp>
namespace voxelui {
inline namespace v1 {
std::string Node::attr(std::string_view key, std::string fallback) const {
    auto i = attributes.find(std::string(key));
    return i == attributes.end() ? std::move(fallback) : i->second;
}
bool builtinTag(std::string_view t) {
    static const std::set<std::string> tags = {
        "Screen", "Panel",    "Row",       "Column", "Grid",     "Stack",  "ScrollView", "Text",
        "Image",  "Vector",   "Button",    "Toggle", "Checkbox", "Slider", "TextInput",  "ProgressBar",
        "Repeat", "Template", "Component", "Modal",  "Tooltip",  "Select"};
    return tags.contains(std::string(t));
}
namespace detail {
std::optional<Node> parseXml(std::string_view xml, std::string source, Diagnostics &d, Limits limits) {
    if (xml.empty() || xml.size() > limits.bytes) {
        d.push_back({Severity::error, source, 1, 1, "XML is empty or exceeds byte limit"});
        return {};
    }
    // DTDs are prohibited, including internal subsets. No entity substitution or XInclude.
    if (xml.find("<!DOCTYPE") != xml.npos || xml.find("<!ENTITY") != xml.npos) {
        d.push_back({Severity::error, source, 1, 1, "DTDs and entity declarations are forbidden"});
        return {};
    }
    xmlParserCtxtPtr ctx = xmlNewParserCtxt();
    if (!ctx)
        return {};
    xmlDocPtr doc = xmlCtxtReadMemory(ctx, xml.data(), int(xml.size()), source.c_str(), "UTF-8",
                                      XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) {
        auto *e = xmlCtxtGetLastError(ctx);
        d.push_back({Severity::error, source, e ? e->line : 1, e ? e->int2 : 1,
                     e && e->message ? trim(e->message) : "Malformed XML"});
        xmlFreeParserCtxt(ctx);
        return {};
    }
    if (doc->intSubset || doc->extSubset) {
        d.push_back({Severity::error, source, 1, 1, "DTDs are forbidden"});
        xmlFreeDoc(doc);
        xmlFreeParserCtxt(ctx);
        return {};
    }
    std::size_t count = 0;
    bool failed = false;
    std::function<Node(xmlNode *, std::size_t)> read = [&](xmlNode *x, std::size_t depth) {
        Node n;
        n.tag = reinterpret_cast<const char *>(x->name);
        n.source = source;
        n.line = int(xmlGetLineNo(x));
        if (depth > limits.depth || ++count > limits.elements) {
            if (!failed)
                d.push_back({Severity::error, source, n.line, 1, "XML nesting or element limit exceeded"});
            failed = true;
            return n;
        }
        std::size_t attrs = 0;
        for (auto *a = x->properties; a; a = a->next) {
            if (++attrs > limits.attributes) {
                failed = true;
                d.push_back({Severity::error, source, n.line, 1, "Attribute limit exceeded"});
                break;
            }
            auto *val = xmlNodeListGetString(doc, a->children, 1);
            n.attributes[reinterpret_cast<const char *>(a->name)] =
                val ? reinterpret_cast<const char *>(val) : "";
            xmlFree(val);
        }
        n.id = n.attr("id");
        for (auto *c = x->children; c && !failed; c = c->next) {
            if (c->type == XML_ELEMENT_NODE)
                n.children.push_back(read(c, depth + 1));
            else if (c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE) {
                if (c->content)
                    n.text += reinterpret_cast<const char *>(c->content);
            } else if (c->type == XML_ENTITY_REF_NODE) {
                failed = true;
                d.push_back({Severity::error, source, n.line, 1, "Entity references are forbidden"});
            }
        }
        n.text = trim(n.text);
        return n;
    };
    auto *root = xmlDocGetRootElement(doc);
    std::optional<Node> result;
    if (root)
        result = read(root, 1);
    else
        failed = true;
    xmlFreeDoc(doc);
    xmlFreeParserCtxt(ctx);
    if (failed)
        return {};
    return result;
}
} // namespace detail
ParseResult parseDocument(std::string_view xml, std::string source, const std::set<std::string> &customTags,
                          Limits limits) {
    ParseResult out;
    auto root = detail::parseXml(xml, source, out.diagnostics, limits);
    if (!root)
        return out;
    Document doc;
    doc.root = std::move(*root);
    auto error = [&](const Node &n, std::string msg) {
        out.diagnostics.push_back({Severity::error, source, n.line, 1, std::move(msg)});
    };
    if (doc.root.tag != "Screen" || doc.root.attr("version") != "1")
        error(doc.root, "Root must be <Screen version=\"1\">");
    std::set<std::string> ids;
    static const std::set<std::string> attrs = {
        "version",  "id",      "class",    "style", "stylesheet",  "source",  "value",     "action",
        "items",    "key",     "template", "name",  "text",        "checked", "disabled",  "selected",
        "writable", "min",     "max",      "step",  "placeholder", "columns", "draggable", "tooltip",
        "visible",  "options", "anchor",   "slot",  "permission"};
    std::function<void(const Node &, bool)> validate = [&](const Node &n, bool inTemplate) {
        if (!builtinTag(n.tag) && !customTags.contains(n.tag))
            error(n, "Unknown component: " + n.tag);
        if (n.id.size() > 256)
            error(n, "Element IDs are limited to 256 UTF-8 bytes");
        if (!n.id.empty() && !inTemplate && !ids.insert(n.id).second)
            error(n, "Duplicate element ID: " + n.id);
        if (n.tag == "Screen" && &n != &doc.root)
            error(n, "Screen cannot be nested");
        if ((n.tag == "Image" || n.tag == "Vector") && n.attr("source").empty())
            error(n, "Image/Vector requires source resource");
        if ((n.tag == "Text" || n.tag == "Image" || n.tag == "Vector" || n.tag == "TextInput" ||
             n.tag == "Slider" || n.tag == "ProgressBar") &&
            !n.children.empty())
            error(n, "Leaf component cannot contain children");
        if (n.tag == "Repeat" && (n.attr("items").empty() || n.attr("key").empty() || n.children.size() != 1))
            error(n, "Repeat needs items, key and exactly one child template");
        if (n.tag == "Template") {
            std::set<std::string> localIds;
            std::function<void(const Node &)> local = [&](const Node &c) {
                if (!c.id.empty() && !localIds.insert(c.id).second)
                    error(c, "Duplicate template-local ID: " + c.id);
                for (auto &child : c.children)
                    local(child);
            };
            for (auto &c : n.children)
                local(c);
            auto name = n.attr("name");
            if (name.empty() || n.children.size() != 1 || doc.templates.contains(name))
                error(n, "Template needs a unique name and exactly one root child");
            else
                doc.templates[name] = n.children.front();
        }
        for (auto &[k, v] : n.attributes) {
            if (!attrs.contains(k) && !k.starts_with("data-") && n.tag != "Component" && n.tag != "Button" &&
                !customTags.contains(n.tag))
                error(n, "Unknown attribute: " + k);
            if ((k == "stylesheet" || k == "source" || k == "action") && !v.empty() && v.front() != '{' &&
                !validResourceId(v))
                error(n, "Invalid namespaced resource/action ID: " + v);
        }
        for (auto &c : n.children)
            validate(c, inTemplate || n.tag == "Template");
    };
    validate(doc.root, false);
    std::function<void(const Node &, std::set<std::string>, std::size_t)> references =
        [&](const Node &n, std::set<std::string> chain, std::size_t depth) {
            if (depth > limits.depth) {
                error(n, "Template expansion exceeds nesting limit");
                return;
            }
            if (n.tag == "Component") {
                auto name = n.attr("template");
                auto it = doc.templates.find(name);
                if (it == doc.templates.end())
                    error(n, "Unknown template: " + name);
                else if (!chain.insert(name).second)
                    error(n, "Circular template reference: " + name);
                else
                    references(it->second, chain, depth + 1);
            }
            for (auto &c : n.children)
                references(c, chain, depth + 1);
        };
    references(doc.root, {}, 0);
    if (!hasErrors(out.diagnostics))
        out.document = std::move(doc);
    return out;
}
} // namespace v1
} // namespace voxelui
