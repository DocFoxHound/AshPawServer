#pragma once

#include "ashpaw/session/session.hpp"

#include <optional>
#include <unordered_map>
#include <vector>

namespace ashpaw::session {

class SessionManager {
  public:
    explicit SessionManager(std::uint16_t max_players);

    [[nodiscard]] bool is_full() const noexcept;
    Session& create(_ENetPeer* peer);
    bool remove(_ENetPeer* peer);

    [[nodiscard]] Session* find(_ENetPeer* peer);
    [[nodiscard]] const Session* find(_ENetPeer* peer) const;
    [[nodiscard]] std::vector<Session*> active_sessions();
    [[nodiscard]] std::size_t size() const noexcept;

  private:
    std::uint32_t next_session_id_ {1};
    std::uint16_t max_players_ {16};
    std::unordered_map<_ENetPeer*, Session> sessions_;
};

}  // namespace ashpaw::session
