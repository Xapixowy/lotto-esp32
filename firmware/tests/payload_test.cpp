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
    assert(snapshot.slides[6].groups[1].kind == "additional");
    assert(snapshot.slides[6].groups[1].values == std::vector<int>({3,9}));
    json["schema_version"] = 2;
    lotto::Snapshot unsupported;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), unsupported));
    json["schema_version"] = 1;
    json["results"][0]["groups"][0]["value"][0] = "1";
    lotto::Snapshot wrongType;
    assert(!lotto::parseSnapshot(json.as<JsonVariantConst>(), wrongType));
    assert(lotto::responseStatus("access_denied") == lotto::Status::AccessDenied);
    assert(lotto::responseStatus("fetching") == lotto::Status::Fetching);
    assert(lotto::responseStatus("lotto_refresh_failed") == lotto::Status::LottoFailed);
    std::cout << "Shared backend/firmware payload contract passed\n";
}
