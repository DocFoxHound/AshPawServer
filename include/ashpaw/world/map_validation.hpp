#pragma once

#include "ashpaw/world/map.hpp"

#include <string>
#include <vector>

namespace ashpaw::world {

struct MapValidationIssue {
    std::string message;
};

[[nodiscard]] std::vector<MapValidationIssue> validate_map(const MapData& map);

}  // namespace ashpaw::world
