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
    json["results"][0]["groups"][0]["numbers"].as<JsonArray>().remove(0);
    lotto::Snapshot missingNumber;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), missingNumber));
    json["results"][0]["groups"][0].remove("numbers");
    json["schema_version"] = 2;
    lotto::Snapshot unsupported;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), unsupported));
    json["schema_version"] = 1;
    json["results"][0]["groups"][0]["value"][0] = "1";
    lotto::Snapshot wrongType;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), wrongType));
    auto values = json["results"][0]["groups"][0]["value"].to<JsonArray>();
    for (int number = 1; number <= 20; ++number) values.add(number);
    lotto::Snapshot fullGroup;
    assert(lotto::parseSnapshot(json.as<JsonVariantConst>(), fullGroup));
    assert(fullGroup.slides[0].groups[0].values.size() == 20);
    values.add(21);
    lotto::Snapshot oversized;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), oversized));
    assert(lotto::responseStatus("access_denied") == lotto::Status::AccessDenied);
    assert(lotto::responseStatus("fetching") == lotto::Status::Fetching);
    assert(lotto::responseStatus("lotto_refresh_failed") == lotto::Status::LottoFailed);
    std::cout << "Shared backend/firmware payload contract passed\n";
}
