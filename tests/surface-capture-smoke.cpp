/** @fileoverview Exercises wlr-screencopy capture cleanup with fake proxies. */
#include "surface-capture-smoke.hpp"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <wayland-client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace {
enum class CleanupCall {
  Frame,
  Output,
  Manager,
  Buffer,
  Pool,
  Shm,
  Registry,
  Display,
};

std::array<CleanupCall, 9> cleanupCalls;
std::size_t cleanupCallCount = 0;
bool recordingCleanup = false;

/** Records one cleanup call while the lifecycle test is active. */
template <CleanupCall Call, typename Proxy> void recordCleanup(Proxy *) {
  if (cleanupCallCount < cleanupCalls.size())
    cleanupCalls[cleanupCallCount++] = Call;
}
} // namespace

/** Records or forwards frame destruction. */
void recordFrameDestroy(zwlr_screencopy_frame_v1 *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Frame>(proxy);
  else
    ::zwlr_screencopy_frame_v1_destroy(proxy);
}
/** Records or forwards output release. */
void recordOutputRelease(wl_output *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Output>(proxy);
  else
    ::wl_output_release(proxy);
}
/** Records or forwards manager destruction. */
void recordManagerDestroy(zwlr_screencopy_manager_v1 *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Manager>(proxy);
  else
    ::zwlr_screencopy_manager_v1_destroy(proxy);
}
/** Records or forwards buffer destruction. */
void recordBufferDestroy(wl_buffer *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Buffer>(proxy);
  else
    ::wl_buffer_destroy(proxy);
}
/** Records or forwards pool destruction. */
void recordPoolDestroy(wl_shm_pool *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Pool>(proxy);
  else
    ::wl_shm_pool_destroy(proxy);
}
/** Records or forwards shared-memory destruction. */
void recordShmDestroy(wl_shm *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Shm>(proxy);
  else
    ::wl_shm_destroy(proxy);
}
/** Records or forwards registry destruction. */
void recordRegistryDestroy(wl_registry *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Registry>(proxy);
  else
    ::wl_registry_destroy(proxy);
}
/** Records or forwards display disconnection. */
void recordDisplayDisconnect(wl_display *proxy) {
  if (recordingCleanup)
    recordCleanup<CleanupCall::Display>(proxy);
  else
    ::wl_display_disconnect(proxy);
}

#define zwlr_screencopy_frame_v1_destroy recordFrameDestroy
#define wl_output_release recordOutputRelease
#define zwlr_screencopy_manager_v1_destroy recordManagerDestroy
#define wl_buffer_destroy recordBufferDestroy
#define wl_shm_pool_destroy recordPoolDestroy
#define wl_shm_destroy recordShmDestroy
#define wl_registry_destroy recordRegistryDestroy
#define wl_display_disconnect recordDisplayDisconnect
#include "surface-capture.cpp"
#undef zwlr_screencopy_frame_v1_destroy
#undef wl_output_release
#undef zwlr_screencopy_manager_v1_destroy
#undef wl_buffer_destroy
#undef wl_shm_pool_destroy
#undef wl_shm_destroy
#undef wl_registry_destroy
#undef wl_display_disconnect

/** Runs the native capture cleanup lifecycle check. */
bool runWaylandCleanupChecks() {
  cleanupCallCount = 0;
  recordingCleanup = true;
  {
    CaptureState state;
    state.display = reinterpret_cast<wl_display *>(1);
    state.registry = reinterpret_cast<wl_registry *>(2);
    state.shm = reinterpret_cast<wl_shm *>(3);
    state.manager = reinterpret_cast<zwlr_screencopy_manager_v1 *>(6);
    for (const auto handle : {4, 12}) {
      auto output = std::make_unique<OutputInfo>();
      output->output =
          reinterpret_cast<wl_output *>(static_cast<std::uintptr_t>(handle));
      state.outputs.push_back(std::move(output));
    }
    state.frame = reinterpret_cast<zwlr_screencopy_frame_v1 *>(9);
    state.buffer = reinterpret_cast<wl_buffer *>(10);
    state.pool = reinterpret_cast<wl_shm_pool *>(11);
  }
  recordingCleanup = false;
  constexpr std::array expected{
      CleanupCall::Frame,   CleanupCall::Output,   CleanupCall::Output,
      CleanupCall::Manager, CleanupCall::Buffer,   CleanupCall::Pool,
      CleanupCall::Shm,     CleanupCall::Registry, CleanupCall::Display,
  };
  return cleanupCallCount == expected.size() && cleanupCalls == expected;
}
