/** @fileoverview The auto-scroll injection worker: drives the application
 *  under the capture region with real wheel input, one tick per acknowledged
 *  capture cycle. Backend cascade (Niri): the wlr virtual-pointer protocol
 *  bound to the target output by default; a uinput kernel mouse only when
 *  the compositor's natural-scroll policy is known (currently never on Niri,
 *  so auto mode stays virtual-pointer until proven). */
#pragma once

#include "auto-capture.hpp"
#include "stitch.hpp"

#include <QString>

#include <atomic>
#include <memory>

/// Spawns the injection worker thread. `parkX/parkY` are physical pixels of
/// `outputName` where the pointer is parked so wheel events hit the page
/// inside the region. The worker publishes ready cycles on `handshake`
/// (cycle 1 = the unscrolled first frame), scrolls the acknowledged notch
/// count per cycle, and sets `stop` itself on any exit so its death is always
/// observable. Returns false with `error` set when no backend is available.
[[nodiscard]] bool spawnScrollInjector(
    std::shared_ptr<std::atomic<bool>> stop,
    std::shared_ptr<stitch::CaptureHandshake> handshake, int parkX, int parkY,
    stitch::Axis axis, const QString &outputName, QString &error);
