#include "ashpaw/session/session_manager.hpp"

namespace ashpaw::session {

SessionManager::SessionManager(std::uint16_t max_players)
    : max_players_(max_players) {}

bool SessionManager::is_full() const noexcept {
    return sessions_.size() >= max_players_;
}

Session& SessionManager::create(_ENetPeer* peer) {
    auto [it, inserted] = sessions_.emplace(peer, Session {
                                                      .session_id = next_session_id_++,
                                                      .peer = peer,
                                                      .state = ConnectionState::connected_unverified,
                                                      .player_id = {},
                                                      .display_name = {},
                                                      .entity_id = 0,
                                                      .last_replicated_positions = {},
                                                      .last_replicated_object_states = {}
                                                  });
    if (!inserted) {
        it->second.state = ConnectionState::connected_unverified;
    }
    return it->second;
}

bool SessionManager::remove(_ENetPeer* peer) {
    return sessions_.erase(peer) > 0;
}

Session* SessionManager::find(_ENetPeer* peer) {
    const auto found = sessions_.find(peer);
    return found == sessions_.end() ? nullptr : &found->second;
}

const Session* SessionManager::find(_ENetPeer* peer) const {
    const auto found = sessions_.find(peer);
    return found == sessions_.end() ? nullptr : &found->second;
}

std::vector<Session*> SessionManager::active_sessions() {
    std::vector<Session*> result;
    result.reserve(sessions_.size());
    for (auto& [peer, session] : sessions_) {
        (void)peer;
        if (session.state == ConnectionState::active) {
            result.push_back(&session);
        }
    }
    return result;
}

std::size_t SessionManager::size() const noexcept {
    return sessions_.size();
}

}  // namespace ashpaw::session
