#pragma once
#include "actions.hpp"
#include "layout.hpp"
namespace voxelui {
inline namespace v1 {
struct ElementState {
    bool selected{}, checked{};
    double value{};
    std::string text, composition;
    std::size_t caret{}, anchor{};
    Vec2 scroll;
};
struct FrameInfo {
    float deltaSeconds{1.f / 60}, uiScale{1}, dpiScale{1};
    Vec2 viewport{960, 640};
};
struct RuntimeStats {
    std::size_t fullRebuilds{}, bindingEvaluations{}, patchedNodes{}, styledNodes{}, measuredNodes{},
        placedNodes{};
};
class Runtime {
    Resources resources_;
    Actions actions_;
    DataModel data_;
    Components components_;
    TextSystem text_{resources_};
    VectorCache vectors_;
    std::optional<Document> document_;
    std::shared_ptr<LayoutNode> tree_;
    Stylesheet stylesheet_;
    std::string screenId_, overrideCss_;
    std::map<std::string, std::map<std::string, Value>> boundValues_;
    std::map<std::string, std::string> stateTypes_;
    std::map<std::string, ElementState> states_;
    std::map<std::string, std::shared_ptr<const Image>> images_;
    std::map<std::string, WidgetDefinition> slots_;
    std::map<std::string, std::shared_ptr<const VectorDocument>> vectorDocuments_;
    std::map<std::string, std::uint64_t> imageRevisions_;
    std::set<std::string> dependencies_;
    struct Animation {
        Style from, to;
        float elapsed{}, duration{};
    };
    std::map<std::string, Animation> animations_;
    std::map<std::string, Style> previousStyles_;
    void animate(float delta);
    std::uint64_t dataRevision_{}, resourceEpoch_{}, componentRevision_{};
    bool dirty_{true};
    FrameInfo frame_;
    LayoutStats stats_;
    std::string focus_, hover_, capture_, pressed_, drag_, slotCapture_;
    Vec2 down_{};
    Diagnostics diagnostics_;
    std::function<void(RoutedEvent &)> eventObserver_;
    std::function<std::string()> clipboardRead_;
    std::function<void(std::string)> clipboardWrite_;
    struct BindingSource {
        std::string path;
        std::vector<std::variant<std::string, std::size_t>> steps;
    };
    struct BindingTarget {
        BindingSource source;
        std::string id, attribute; // Empty attribute denotes node text.
    };
    struct RepeatBinding {
        BindingSource source;
        std::string key;
        std::vector<std::string> keys;
    };
    struct NodeIndex {
        LayoutNode *node{}, *parent{};
    };
    std::vector<BindingTarget> bindings_;
    std::vector<RepeatBinding> repeats_;
    std::map<std::string, std::vector<std::size_t>, std::less<>> bindingIndex_;
    std::map<std::string, NodeIndex, std::less<>> nodeIndex_;
    std::set<std::string> styleDirty_;
    bool layoutDirty_{};
    bool bindingRejected_{};
    RuntimeStats runtimeStats_;
    std::size_t expandedBytes_{};
    const Value *bindingValue(const BindingSource &) const;
    void indexTree();
    void invalidateMeasure(LayoutNode &);
    void invalidateStyle(std::string_view);
    bool patchBindings();
    bool refreshStyles();
    void layout();
    void validateInteraction();
    void rebuild();
    void paint(const LayoutNode &, DrawList &);
    void activate(const LayoutNode &);
    void write(const LayoutNode &, std::string_view, Value);

  public:
    Resources &resources() {
        return resources_;
    }
    Actions &actions() {
        return actions_;
    }
    DataModel &data() {
        return data_;
    }
    Components &components() {
        return components_;
    }
    bool load(std::string_view resourceId);
    bool loadXml(std::string_view xml, std::string source = "<memory>");
    bool reload();
    void close();
    bool setStyleOverride(std::string css);
    bool unregisterSlot(std::string_view name) {
        bool erased = slots_.erase(std::string(name)) != 0;
        if (!slotCapture_.empty())
            capture_.clear();
        slotCapture_.clear();
        return erased;
    }
    void registerSlot(std::string name, WidgetDefinition slot) {
        slots_[std::move(name)] = std::move(slot);
        if (!slotCapture_.empty())
            capture_.clear();
        slotCapture_.clear();
    }
    // Host-managed widget state that changes intrinsic size must invalidate by ID.
    bool invalidateMeasurement(std::string_view elementId);
    void update(FrameInfo frame = {});
    bool processEvent(const Event &);
    DrawList buildDrawList(bool debug = false);
    const LayoutNode *tree() const {
        return tree_ ? &*tree_ : nullptr;
    }
    const LayoutNode *find(std::string_view id) const;
    const ElementState *state(std::string_view id) const {
        auto i = states_.find(std::string(id));
        return i == states_.end() ? nullptr : &i->second;
    }
    const std::string &focusedId() const {
        return focus_;
    }
    const Diagnostics &diagnostics() const {
        return diagnostics_;
    }
    void clearDiagnostics() {
        diagnostics_.clear();
    }
    const RuntimeStats &runtimeStats() const {
        return runtimeStats_;
    }
    const LayoutStats &layoutStats() const {
        return stats_;
    }
    void observeEvents(std::function<void(RoutedEvent &)> callback) {
        eventObserver_ = std::move(callback);
    }
    void clipboard(std::function<std::string()> read, std::function<void(std::string)> write) {
        clipboardRead_ = std::move(read);
        clipboardWrite_ = std::move(write);
    }
};
} // namespace v1
} // namespace voxelui
