#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace lotto {

enum class Status { Ready, Fetching, WifiDisconnected, BackendUnreachable, AccessDenied,
                    BackendUnavailable, LottoFailed, Stale, InvalidResponse };
enum class Touch { Previous, Next, Lock };

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
                if (group.label.empty() || group.values.empty() || (group.kind != "simple" && group.kind != "additional")) {
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
    }

    bool pollDue(std::uint64_t now) const {
        return !hasPolled_ || now - lastPolledAt_ >= 15000;
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
        if (now - pageStartedAt_ >= 10000) {
            pageStartedAt_ = now;
            if (++page_ >= pageCount(snapshot_.slides[slide_])) {
                page_ = 0;
                if (!locked_) slide_ = (slide_ + 1) % snapshot_.slides.size();
            }
        }
    }

    void touch(Touch action, std::uint64_t now) {
        tick(now);
        if (status_ != Status::Ready || snapshot_.slides.empty()) return;
        if (action == Touch::Lock) {
            locked_ = !locked_;
            if (locked_) lockedAt_ = now;
            else pageStartedAt_ = now;
        } else {
            slide_ = (slide_ + (action == Touch::Next ? 1 : snapshot_.slides.size() - 1)) % snapshot_.slides.size();
            page_ = 0;
            pageStartedAt_ = now;
        }
    }

    View view(std::uint64_t) const {
        View result;
        result.status = status_;
        if (status_ != Status::Ready || snapshot_.slides.empty()) return result;
        const auto& slide = snapshot_.slides[slide_];
        result.title = slide.label;
        std::size_t remaining = page_;
        for (const auto& group : slide.groups) {
            auto pages = (group.values.size() + 11) / 12;
            if (remaining >= pages) { remaining -= pages; continue; }
            result.groupLabel = group.label;
            result.kind = group.kind;
            auto begin = group.values.begin() + remaining * 12;
            result.values.assign(begin, begin + std::min<std::size_t>(12, group.values.size() - remaining * 12));
            break;
        }
        result.page = page_;
        result.pages = pageCount(slide);
        result.locked = locked_;
        result.refreshing = refreshing_;
        result.lottoTime = snapshot_.lottoTime;
        result.syncTime = snapshot_.syncTime;
        return result;
    }

private:
    static std::size_t pageCount(const Slide& slide) {
        std::size_t count = 0;
        for (const auto& group : slide.groups) count += (group.values.size() + 11) / 12;
        return std::max<std::size_t>(1, count);
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
};

} // namespace lotto
