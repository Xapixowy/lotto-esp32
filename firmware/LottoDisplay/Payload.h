#pragma once

#include <ArduinoJson.h>
#include "Controller.h"

namespace lotto {

constexpr int RESULTS_SCHEMA_VERSION = 2;

inline Status responseStatus(const std::string& status) {
    if (status == "fetching") return Status::Fetching;
    if (status == "access_denied") return Status::AccessDenied;
    if (status == "backend_unavailable") return Status::BackendUnavailable;
    if (status == "lotto_refresh_failed") return Status::LottoFailed;
    if (status == "stale") return Status::Stale;
    return Status::InvalidResponse;
}

inline bool parseSnapshot(JsonVariantConst json, Snapshot& snapshot) {
    if (!json["schema_version"].is<int>() || json["schema_version"].as<int>() != RESULTS_SCHEMA_VERSION ||
        !json["status"].is<const char*>() || std::string(json["status"].as<const char*>()) != "ready" ||
        !json["lotto_fetched_at"].is<std::int64_t>() || !json["server_time"].is<std::int64_t>() ||
        !json["lotto_time"].is<const char*>() || !json["sync_time"].is<const char*>() ||
        !json["results"].is<JsonArrayConst>()) return false;

    snapshot.lottoFetchedAt = json["lotto_fetched_at"].as<std::int64_t>();
    snapshot.serverTime = json["server_time"].as<std::int64_t>();
    snapshot.lottoTime = json["lotto_time"].as<std::string>();
    snapshot.syncTime = json["sync_time"].as<std::string>();
    auto slides = json["results"].as<JsonArrayConst>();
    if (slides.size() == 0 || slides.size() > 16) return false;
    for (JsonVariantConst item : slides) {
        if (!item["id"].is<const char*>() || !item["label"].is<const char*>() || !item["groups"].is<JsonArrayConst>()) return false;
        Slide slide{item["id"].as<std::string>(), item["label"].as<std::string>(), {}};
        if (slide.id.empty() || slide.id.size() > 40 || slide.label.empty() || slide.label.size() > 64) return false;
        for (const auto& existing : snapshot.slides) if (existing.id == slide.id) return false;
        auto groups = item["groups"].as<JsonArrayConst>();
        if (groups.size() == 0 || groups.size() > 16) return false;
        for (JsonVariantConst itemGroup : groups) {
            if (!itemGroup["label"].is<const char*>() || !itemGroup["numbers"].is<JsonArrayConst>()) return false;
            Group group{itemGroup["label"].as<std::string>(), "simple", {}};
            if (group.label.empty() || group.label.size() > 128) return false;
            auto numbers = itemGroup["numbers"].as<JsonArrayConst>();
            if (numbers.size() == 0 || numbers.size() > MAX_GROUP_VALUES) return false;
            for (JsonVariantConst number : numbers) {
                if (!number["value"].is<int>() || number["value"].as<int>() < 0 || number["value"].as<int>() > 999 ||
                    !number["type"].is<const char*>()) return false;
                const int value = number["value"].as<int>();
                if (!group.values.empty() && value < group.values.back()) return false;
                const std::string type = number["type"].as<std::string>();
                if (type != "simple" && type != "special") return false;
                group.values.push_back(value);
                group.special.push_back(type == "special");
            }
            slide.groups.push_back(std::move(group));
        }
        snapshot.slides.push_back(std::move(slide));
    }
    return true;
}

inline Status parseResultsResponse(int code, JsonVariantConst json, Snapshot& snapshot) {
    if (!json["schema_version"].is<int>() || json["schema_version"].as<int>() != RESULTS_SCHEMA_VERSION) return Status::InvalidResponse;
    const auto payloadStatus = json["status"].as<std::string>();
    if (code == 200 && payloadStatus == "ready" && parseSnapshot(json, snapshot)) return Status::Ready;
    if (code == 503 && payloadStatus != "ready") return responseStatus(payloadStatus);
    return Status::InvalidResponse;
}

} // namespace lotto
