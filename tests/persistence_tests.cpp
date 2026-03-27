#include "ashpaw/persistence/player_repository.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace {

class TempDirectory {
  public:
    explicit TempDirectory(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::filesystem::remove_all(path_);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

}  // namespace

TEST_CASE("file player repository saves and loads player state", "[persistence]") {
    TempDirectory directory("ashpaw_persistence_repo");
    ashpaw::persistence::FilePlayerRepository repository(directory.path());

    const ashpaw::persistence::PlayerSave save {
        .schema_version = 1,
        .player_id = "birch",
        .display_name = "Birch",
        .last_map = "dev_map",
        .last_position = {.x = 144.0F, .y = 80.0F}
    };

    REQUIRE(repository.save(save));

    const auto loaded = repository.load("birch");
    REQUIRE(loaded.has_value());
    CHECK(loaded->schema_version == 1);
    CHECK(loaded->player_id == "birch");
    CHECK(loaded->display_name == "Birch");
    CHECK(loaded->last_map == "dev_map");
    CHECK(loaded->last_position.x == 144.0F);
    CHECK(loaded->last_position.y == 80.0F);
}

TEST_CASE("file player repository ignores corrupt player save data", "[persistence]") {
    TempDirectory directory("ashpaw_persistence_corrupt");
    ashpaw::persistence::FilePlayerRepository repository(directory.path());

    std::ofstream output(directory.path() / "birch.json");
    output << "{ not valid json";
    output.close();

    const auto loaded = repository.load("birch");
    CHECK_FALSE(loaded.has_value());
}
