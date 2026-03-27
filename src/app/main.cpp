#include "ashpaw/app/server_app.hpp"
#include "ashpaw/config/server_config.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto options = ashpaw::config::parse_cli_options(argc, argv);
        auto config = ashpaw::config::load_server_config(options.config_path);
        config = ashpaw::config::apply_cli_overrides(config, options);

        ashpaw::app::ServerApp app(config);
        app.initialize();
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "AshPaw server failed: " << ex.what() << '\n';
        return 1;
    }
}
