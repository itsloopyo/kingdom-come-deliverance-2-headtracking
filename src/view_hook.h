#pragma once

#include <cstdint>

#include "runtime_state.h"

// The CView::Update hook - the entire game-facing side of the camera
// modification. It lets the engine compose m_viewParams into the camera matrix,
// then rewrites ONLY that matrix and rebuilds the frustum from it. m_viewParams
// is what the game reads for aim, raycasts and weapon direction, so never
// touching it is the whole aim-decoupling story.

namespace kcd2_ht::view_hook
{
    // Called once per active view update, before the injection. The hook owns
    // liveness; deciding what to report about it belongs to the caller, which is
    // the only place holding the config and the receiver.
    using UpdateObserver = void (*)();

    // @p moduleBase is WHGame.dll's, and the active build profile must already
    // be selected - every address installed here comes out of it.
    // HookManager::Initialize() must already have run.
    bool Install(std::uintptr_t moduleBase, Session& session, UpdateObserver onUpdate);

    // Active view updates seen since load. The number that separates "the hook
    // never fired" from "the tracker is not sending".
    std::uint64_t UpdateCount();

    // The live camera's VERTICAL field of view in radians, and its
    // width-to-height ratio. Refreshed every active view update and zero until
    // the first one arrives. Both are what the frame is actually rendered with,
    // so the in-game FOV slider, the aim-zoom blend and a resolution change all
    // land here with nothing for the mod to track.
    float FovRadians();
    float ProjectionRatio();
}
