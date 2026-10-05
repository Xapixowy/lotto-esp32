#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace lotto {

enum class Status { Ready, Fetching, WifiDisconnected, BackendUnreachable, AccessDenied,
                    BackendUnavailable, LottoFailed, Stale, InvalidResponse };
enum class Touch { Previous, Next, Lock };
constexpr std::size_t MAX_GROUP_VALUES = 20;

struct Group {
    std::string label;
    std::string kind;
    std::vector<int> values;
};

struct Slide {
    std::string id;
    std::string label;
    std::vector<Group> groups;
};

struct Snapshot {
    std::int64_t lottoFetchedAt = 0;
    std::int64_t serverTime = 0;
    std::string lottoTime;
    std::string syncTime;
    std::vector<Slide> slides;
};

struct View {
    Status status = Status::Fetching;
    std::string title;
    std::string groupLabel;
    std::string kind;
    std::vector<int> values;
    std::string lottoTime;
    std::string syncTime;
    std::size_t page = 0;
    std::size_t pages = 0;
    bool locked = false;
    bool refreshing = false;
    std::uint16_t pageRemainingPermille = 1000;
    bool touchFeedback = false;
    Touch touchedControl = Touch::Previous;
};

class Controller {
public:
    void receive(const Snapshot& snapshot, std::uint64_t now) {
        if (snapshot.slides.empty() || snapshot.slides.size() > 16 || snapshot.lottoFetchedAt <= 0 || snapshot.serverTime < snapshot.lottoFetchedAt) {
            fail(Status::InvalidResponse, now);
            return;
        }
        for (const auto& slide : snapshot.slides) {
            if (slide.id.empty() || slide.label.empty() || slide.groups.empty()) {
                fail(Status::InvalidResponse, now);
                return;
            }
            for (const auto& group : slide.groups) {
                if (group.label.empty() || group.values.empty() || group.values.size() > MAX_GROUP_VALUES || (group.kind != "simple" && group.kind != "additional")) {
                    fail(Status::InvalidResponse, now);
                    return;
                }
            }
        }
        if (snapshot.serverTime - snapshot.lottoFetchedAt > 480) {
            fail(Status::Stale, now);
            return;
        }
        std::string selected = snapshot_.slides.empty() ? "" : snapshot_.slides[slide_].id;
        snapshot_ = snapshot;
        slide_ = 0;
        for (std::size_t i = 0; i < snapshot_.slides.size(); ++i) {
            if (snapshot_.slides[i].id == selected) slide_ = i;
        }
        page_ = snapshot_.slides.empty() ? 0 : std::min(page_, pageCount(snapshot_.slides[slide_]) - 1);
        if (status_ != Status::Ready) pageStartedAt_ = now;
        status_ = Status::Ready;
        receivedAt_ = now;
        refreshing_ = false;
    }

    void fail(Status status, std::uint64_t) {
        status_ = status;
        refreshing_ = false;
        hasTouched_ = false;
    }

    bool pollDue(std::uint64_t now) const {
        return !hasPolled_ || now - lastPolledAt_ >= 15000;
    }

    void connectionRestored() {
        hasPolled_ = false;
    }

    void beginPoll(std::uint64_t now) {
        hasPolled_ = true;
        lastPolledAt_ = now;
        refreshing_ = true;
    }

    void tick(std::uint64_t now) {
        if (locked_ && now - lockedAt_ >= 300000) {
            locked_ = false;
            pageStartedAt_ = now;
        }
        if (status_ != Status::Ready || snapshot_.slides.empty()) return;
        if (static_cast<std::uint64_t>(snapshot_.serverTime - snapshot_.lottoFetchedAt) * 1000 + now - receivedAt_ > 480000) {
            fail(Status::Stale, now);
            return;
        }
        if (!locked_ && now - pageStartedAt_ >= PAGE_DURATION_MS) {
            pageStartedAt_ = now;
            movePage(true);
        }
    }

    void touch(Touch action, std::uint64_t now) {
        tick(now);
        if (status_ != Status::Ready || snapshot_.slides.empty()) return;
        hasTouched_ = true;
        touchedAt_ = now;
        touchedControl_ = action;
        if (action == Touch::Lock) {
            locked_ = !locked_;
            if (locked_) lockedAt_ = now;
            else pageStartedAt_ = now;
        } else {
            movePage(action == Touch::Next);
            pageStartedAt_ = now;
        }
    }

    View view(std::uint64_t now) const {
        View result;
        result.status = status_;
        if (status_ != Status::Ready || snapshot_.slides.empty()) return result;
        const auto& slide = snapshot_.slides[slide_];
        result.title = slide.label;
        const auto& group = slide.groups[page_];
        result.groupLabel = group.label;
        result.kind = group.kind;
        result.values = group.values;
        result.page = page_;
        result.pages = pageCount(slide);
        result.locked = locked_;
        if (!locked_) {
            const auto elapsed = std::min<std::uint64_t>(now - pageStartedAt_, PAGE_DURATION_MS);
            result.pageRemainingPermille = static_cast<std::uint16_t>((PAGE_DURATION_MS - elapsed) * 1000 / PAGE_DURATION_MS);
        }
        result.touchFeedback = hasTouched_ && now - touchedAt_ < 150;
        result.touchedControl = touchedControl_;
        result.refreshing = refreshing_;
        result.lottoTime = snapshot_.lottoTime;
        result.syncTime = snapshot_.syncTime;
        return result;
    }

private:
    static constexpr std::uint64_t PAGE_DURATION_MS = 10000;

    void movePage(bool forward) {
        if (forward) {
            if (++page_ >= pageCount(snapshot_.slides[slide_])) {
                slide_ = (slide_ + 1) % snapshot_.slides.size();
                page_ = 0;
            }
        } else if (page_ > 0) {
            --page_;
        } else {
            slide_ = (slide_ + snapshot_.slides.size() - 1) % snapshot_.slides.size();
            page_ = pageCount(snapshot_.slides[slide_]) - 1;
        }
    }

    static std::size_t pageCount(const Slide& slide) {
        return slide.groups.size();
    }

    Snapshot snapshot_;
    Status status_ = Status::Fetching;
    std::uint64_t receivedAt_ = 0;
    std::uint64_t pageStartedAt_ = 0;
    std::size_t slide_ = 0;
    std::size_t page_ = 0;
    bool locked_ = false;
    std::uint64_t lockedAt_ = 0;
    bool refreshing_ = false;
    bool hasPolled_ = false;
    std::uint64_t lastPolledAt_ = 0;
    bool hasTouched_ = false;
    std::uint64_t touchedAt_ = 0;
    Touch touchedControl_ = Touch::Previous;
};

} // namespace lotto
