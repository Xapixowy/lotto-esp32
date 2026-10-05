#include "../LottoDisplay/Payload.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream file(argv[1]);
    assert(file.good());
    std::stringstream buffer;
    buffer << file.rdbuf();
    JsonDocument json;
    assert(!deserializeJson(json, buffer.str()));
    lotto::Snapshot snapshot;
    assert(lotto::parseSnapshot(json.as<JsonVariantConst>(), snapshot));
    assert(snapshot.slides.size() == 8);
    assert(snapshot.slides[6].label == "Eurojackpot");
    assert(snapshot.slides[6].groups.size() == 1);
    assert(snapshot.slides[6].groups[0].values == std::vector<int>({3,5,9,12,23,34,45}));
    assert(snapshot.slides[6].groups[0].special == std::vector<bool>({true,false,true,false,false,false,false}));
    lotto::Controller controller;
    controller.receive(snapshot, 0);
    for (int page = 0; page < 6; ++page) controller.touch(lotto::Touch::Next, page);
    assert(controller.view(6).special == snapshot.slides[6].groups[0].special);
    auto recolored = snapshot;
    recolored.slides[6].groups[0].special[0] = false;
    controller.receive(recolored, 7);
    assert(controller.view(7).values == snapshot.slides[6].groups[0].values);
    assert(!controller.view(7).special[0]);
    recolored.slides[6].groups[0].special.pop_back();
    controller.receive(recolored, 8);
    assert(controller.view(8).status == lotto::Status::InvalidResponse);
    json["results"][6]["groups"][0]["numbers"][0]["type"] = "unknown";
    lotto::Snapshot unknownType;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), unknownType));
    json["results"][6]["groups"][0]["numbers"][0]["type"] = "special";
    json["results"][6]["groups"][0]["numbers"][0]["value"] = 999;
    lotto::Snapshot mismatched;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), mismatched));
    json["results"][6]["groups"][0]["numbers"][0]["value"] = 3;
    json["results"][0]["groups"][0]["numbers"].to<JsonArray>();
    lotto::Snapshot emptyNumbers;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), emptyNumbers));
    json["results"][0]["groups"][0].remove("numbers");
    lotto::Snapshot missingNumbers;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), missingNumbers));
    json["schema_version"] = 1;
    lotto::Snapshot unsupported;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), unsupported));
    json["schema_version"] = 2;
    auto numbers = json["results"][0]["groups"][0]["numbers"].to<JsonArray>();
    for (int value = 1; value <= 20; ++value) {
        auto number = numbers.add<JsonObject>();
        number["value"] = value;
        number["type"] = value == 10 ? "special" : "simple";
    }
    lotto::Snapshot fullGroup;
    assert(lotto::parseSnapshot(json.as<JsonVariantConst>(), fullGroup));
    assert(fullGroup.slides[0].groups[0].values.size() == 20);
    assert(fullGroup.slides[0].groups[0].special[9]);
    numbers[0]["value"] = "1";
    lotto::Snapshot wrongType;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), wrongType));
    numbers[0]["value"] = 1;
    auto extra = numbers.add<JsonObject>();
    extra["value"] = 21;
    extra["type"] = "simple";
    lotto::Snapshot oversized;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), oversized));
    assert(lotto::responseStatus("access_denied") == lotto::Status::AccessDenied);
    assert(lotto::responseStatus("fetching") == lotto::Status::Fetching);
    assert(lotto::responseStatus("lotto_refresh_failed") == lotto::Status::LottoFailed);
    std::cout << "Shared backend/firmware payload contract passed\n";
}
