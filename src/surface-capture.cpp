/** @fileoverview Captures native monitor outputs via wlr-screencopy.
 *
 *  Niri implements zwlr_screencopy_manager_v1 (v3 on the verified session) and
 *  does not expose ext-image-copy-capture. Frames are cursor-free
 *  (overlay_cursor=0) so the overlay never photographs itself, and SHM-only:
 *  linux_dmabuf announcements are ignored.
 *
 *  Unlike ext-image-copy-capture there is no transform event; frames arrive
 *  in output pixels and only y_invert may require a vertical flip. Do not
 *  reuse ext transform handling here.
 */
#include "capture.hpp"
#include "startup-timing.hpp"

#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <QImage>
#include <QTransform>

#include <wayland-client.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <poll.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

// Named (not anonymous) so OutputCapture::State can derive from it without
// giving an exported type internal-linkage members.
namespace capture_detail {
struct OutputInfo {
  wl_output *output = nullptr;
  std::string name;
};

struct CaptureState {
  wl_display *display = nullptr;
  wl_registry *registry = nullptr;
  wl_shm *shm = nullptr;
  zwlr_screencopy_manager_v1 *manager = nullptr;
  zwlr_screencopy_frame_v1 *frame = nullptr;
  wl_shm_pool *pool = nullptr;
  wl_buffer *buffer = nullptr;
  std::vector<std::unique_ptr<OutputInfo>> outputs;
  // Advertised SHM frame description (one buffer event per frame).
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t format = 0;
  uint32_t stride = 0;
  bool hasBuffer = false;
  bool bufferDone = false;
  uint32_t managerVersion = 1;
  /// Geometry/format the current shm buffer was actually created with.
  uint32_t bufferWidth = 0;
  uint32_t bufferHeight = 0;
  uint32_t bufferFormat = 0;
  uint32_t bufferStride = 0;
  uint32_t flags = 0;
  bool frameDone = false;
  bool frameReady = false;
  bool frameFailed = false;
  int fd = -1;
  void *memory = MAP_FAILED;
  std::size_t memorySize = 0;

  /** Releases capture protocol objects before disconnecting the display. */
  ~CaptureState() {
    if (frame)
      zwlr_screencopy_frame_v1_destroy(frame);
    for (const auto &output : outputs) {
      if (output->output)
        wl_output_release(output->output);
    }
    if (manager)
      zwlr_screencopy_manager_v1_destroy(manager);
    if (buffer)
      wl_buffer_destroy(buffer);
    if (pool)
      wl_shm_pool_destroy(pool);
    if (memory != MAP_FAILED)
      munmap(memory, memorySize);
    if (fd >= 0)
      close(fd);
    if (shm)
      wl_shm_destroy(shm);
    if (registry)
      wl_registry_destroy(registry);
    if (display)
      wl_display_disconnect(display);
  }
};

void outputGeometry(void *, wl_output *, int32_t, int32_t, int32_t, int32_t,
                    int32_t, const char *, const char *, int32_t) {}
void outputMode(void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {}
void outputDone(void *, wl_output *) {}
void outputScale(void *, wl_output *, int32_t) {}
void outputName(void *data, wl_output *, const char *name) {
  static_cast<OutputInfo *>(data)->name = name ? name : "";
}
void outputDescription(void *, wl_output *, const char *) {}
constexpr wl_output_listener kOutputListener{outputGeometry, outputMode,
                                             outputDone,     outputScale,
                                             outputName,     outputDescription};

void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                    const char *interface, uint32_t version) {
  auto &state = *static_cast<CaptureState *>(data);
  if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    state.shm = static_cast<wl_shm *>(
        wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface,
                          zwlr_screencopy_manager_v1_interface.name) == 0) {
    state.managerVersion = std::min(version, 3U);
    state.manager = static_cast<zwlr_screencopy_manager_v1 *>(wl_registry_bind(
        registry, name, &zwlr_screencopy_manager_v1_interface,
        state.managerVersion));
  } else if (std::strcmp(interface, wl_output_interface.name) == 0 &&
             version >= 4) {
    auto output = std::make_unique<OutputInfo>();
    output->output = static_cast<wl_output *>(
        wl_registry_bind(registry, name, &wl_output_interface, 4));
    wl_output_add_listener(output->output, &kOutputListener, output.get());
    state.outputs.push_back(std::move(output));
  }
}
void registryRemoved(void *, wl_registry *, uint32_t) {}
constexpr wl_registry_listener kRegistryListener{registryGlobal,
                                                 registryRemoved};

void frameBuffer(void *data, zwlr_screencopy_frame_v1 *, uint32_t format,
                 uint32_t width, uint32_t height, uint32_t stride) {
  auto &state = *static_cast<CaptureState *>(data);
  state.format = format;
  state.width = width;
  state.height = height;
  state.stride = stride;
  state.hasBuffer = true;
}
void frameFlags(void *data, zwlr_screencopy_frame_v1 *, uint32_t flags) {
  static_cast<CaptureState *>(data)->flags = flags;
}
void frameReady(void *data, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t,
                uint32_t) {
  auto &state = *static_cast<CaptureState *>(data);
  state.frameReady = true;
  state.frameDone = true;
}
void frameFailed(void *data, zwlr_screencopy_frame_v1 *) {
  auto &state = *static_cast<CaptureState *>(data);
  state.frameFailed = true;
  state.frameDone = true;
}
void frameDamage(void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t,
                 uint32_t, uint32_t) {}
void frameLinuxDmabuf(void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t,
                       uint32_t) {}
void frameBufferDone(void *data, zwlr_screencopy_frame_v1 *) {
  static_cast<CaptureState *>(data)->bufferDone = true;
}
constexpr zwlr_screencopy_frame_v1_listener kFrameListener{
    frameBuffer, frameFlags,   frameReady,      frameFailed,
    frameDamage, frameLinuxDmabuf, frameBufferDone};

bool dispatchUntil(CaptureState &state, const bool *done, int timeoutMs,
                   QString &error) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (!*done) {
    if (wl_display_dispatch_pending(state.display) < 0) {
      error = QStringLiteral("Wayland screencopy dispatch failed");
      return false;
    }
    if (*done)
      return true;

    while (wl_display_prepare_read(state.display) != 0) {
      if (wl_display_dispatch_pending(state.display) < 0) {
        error = QStringLiteral("Wayland screencopy dispatch failed");
        return false;
      }
      if (*done)
        return true;
    }
    if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
      wl_display_cancel_read(state.display);
      error = QStringLiteral("Could not flush Wayland screencopy request");
      return false;
    }

    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      wl_display_cancel_read(state.display);
      error = QStringLiteral("Wayland screencopy timed out");
      return false;
    }
    pollfd descriptor{wl_display_get_fd(state.display), POLLIN, 0};
    const int result =
        poll(&descriptor, 1, static_cast<int>(remaining.count()));
    if (result <= 0 || !(descriptor.revents & POLLIN)) {
      wl_display_cancel_read(state.display);
      error = result == 0
                  ? QStringLiteral("Wayland screencopy timed out")
                  : QStringLiteral("Wayland screencopy connection failed");
      return false;
    }
    if (wl_display_read_events(state.display) < 0) {
      error = QStringLiteral("Could not read Wayland screencopy response");
      return false;
    }
  }
  return true;
}

bool createShmBuffer(CaptureState &state, QString &error) {
  StartupTimingScope timing("allocate screencopy shm buffer");
  if (!state.hasBuffer) {
    error = QStringLiteral("Compositor offered no SHM screencopy buffer");
    return false;
  }
  constexpr uint32_t supported[]{WL_SHM_FORMAT_ARGB8888,
                                 WL_SHM_FORMAT_XRGB8888};
  const bool formatOk =
      state.format == WL_SHM_FORMAT_ARGB8888 ||
      state.format == WL_SHM_FORMAT_XRGB8888;
  // Accept the compositor's exact stride; validate arithmetic instead of
  // assuming width*4.
  if (!formatOk) {
    // Best effort: still accept ABGR/XBGR by mapping them, but prefer the
    // two canonical formats. Reject anything else.
    if (state.format != WL_SHM_FORMAT_ABGR8888 &&
        state.format != WL_SHM_FORMAT_XBGR8888) {
      error = QStringLiteral(
          "Compositor offered an unsupported screencopy format");
      return false;
    }
  }
  if (state.width == 0 || state.height == 0 || state.width > 32768 ||
      state.height > 32768) {
    error = QStringLiteral("Compositor returned an invalid screencopy size");
    return false;
  }
  if (state.stride < state.width * 4 || state.stride > 32768 * 4) {
    error = QStringLiteral("Compositor returned an invalid screencopy stride");
    return false;
  }
  const std::size_t size =
      static_cast<std::size_t>(state.stride) * state.height;
  if (size > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
    error = QStringLiteral("Screencopy buffer is too large");
    return false;
  }
  (void)supported;

  state.bufferWidth = state.width;
  state.bufferHeight = state.height;
  state.bufferFormat = state.format;
  state.bufferStride = state.stride;
  state.memorySize = size;

  char name[96];
  std::snprintf(name, sizeof(name), "/nirisnap-capture-%d-%p", getpid(),
                static_cast<void *>(&state));
  state.fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (state.fd < 0) {
    error = QStringLiteral("Could not allocate screencopy memory");
    return false;
  }
  shm_unlink(name);
  if (ftruncate(state.fd, static_cast<off_t>(state.memorySize)) < 0) {
    error = QStringLiteral("Could not size screencopy memory");
    return false;
  }
  state.memory = mmap(nullptr, state.memorySize, PROT_READ | PROT_WRITE,
                      MAP_SHARED, state.fd, 0);
  if (state.memory == MAP_FAILED) {
    error = QStringLiteral("Could not map screencopy memory");
    return false;
  }

  state.pool = wl_shm_create_pool(state.shm, state.fd,
                                  static_cast<int32_t>(state.memorySize));
  if (!state.pool) {
    error = QStringLiteral("Could not create Wayland screencopy pool");
    return false;
  }
  state.buffer = wl_shm_pool_create_buffer(
      state.pool, 0, static_cast<int32_t>(state.bufferWidth),
      static_cast<int32_t>(state.bufferHeight),
      static_cast<int32_t>(state.bufferStride), state.bufferFormat);
  if (!state.buffer) {
    error = QStringLiteral("Could not create Wayland screencopy buffer");
    return false;
  }
  return true;
}

/** Releases the shm buffer so createShmBuffer() can size a new one. */
void destroyShmBuffer(CaptureState &state) {
  if (state.buffer)
    wl_buffer_destroy(state.buffer);
  if (state.pool)
    wl_shm_pool_destroy(state.pool);
  if (state.memory != MAP_FAILED)
    munmap(state.memory, state.memorySize);
  if (state.fd >= 0)
    close(state.fd);
  state.buffer = nullptr;
  state.pool = nullptr;
  state.memory = MAP_FAILED;
  state.memorySize = 0;
  state.fd = -1;
}

QImage::Format capturedImageFormat(uint32_t format) {
  switch (format) {
  case WL_SHM_FORMAT_ARGB8888:
    return QImage::Format_ARGB32_Premultiplied;
  case WL_SHM_FORMAT_XRGB8888:
    return QImage::Format_RGB32;
  case WL_SHM_FORMAT_ABGR8888:
    return QImage::Format_RGBA8888_Premultiplied;
  case WL_SHM_FORMAT_XBGR8888:
    return QImage::Format_RGBX8888;
  default:
    return QImage::Format_Invalid;
  }
}

QImage copyCapturedImage(const CaptureState &state) {
  StartupTimingScope timing("copy captured pixels");
  const QImage::Format imageFormat = capturedImageFormat(state.bufferFormat);
  if (imageFormat == QImage::Format_Invalid)
    return {};
  return QImage(static_cast<uchar *>(state.memory),
                static_cast<int>(state.bufferWidth),
                static_cast<int>(state.bufferHeight),
                static_cast<int>(state.bufferStride), imageFormat)
      .copy();
}

struct CapturedMapping {
  void *memory = MAP_FAILED;
  std::size_t size = 0;
};

void unmapCapturedImage(void *context) {
  const std::unique_ptr<CapturedMapping> mapping(
      static_cast<CapturedMapping *>(context));
  if (mapping->memory != MAP_FAILED)
    munmap(mapping->memory, mapping->size);
}

/** Transfers a one-shot capture's mmap into QImage instead of copying. */
QImage takeCapturedImage(CaptureState &state) {
  StartupTimingScope timing("take captured pixel buffer");
  const QImage::Format imageFormat = capturedImageFormat(state.bufferFormat);
  if (imageFormat == QImage::Format_Invalid || state.memory == MAP_FAILED)
    return {};

  if (state.buffer) {
    wl_buffer_destroy(state.buffer);
    state.buffer = nullptr;
  }
  if (state.pool) {
    wl_shm_pool_destroy(state.pool);
    state.pool = nullptr;
  }
  if (state.fd >= 0) {
    close(state.fd);
    state.fd = -1;
  }

  auto *mapping = new CapturedMapping{state.memory, state.memorySize};
  QImage image(static_cast<uchar *>(state.memory),
               static_cast<int>(state.bufferWidth),
               static_cast<int>(state.bufferHeight),
               static_cast<int>(state.bufferStride), imageFormat,
               unmapCapturedImage, mapping);
  // Apply y-invert by flipping rows in place before handing ownership to
  // QImage: screencopy is the only transform metadata on this path.
  state.memory = MAP_FAILED;
  state.memorySize = 0;
  return image;
}

QImage applyYInvert(QImage image, bool yInvert) {
  if (!yInvert || image.isNull())
    return image;
  // Vertical flip: rows are contiguous with bytesPerLine() stride.
  QImage flipped(image.size(), image.format());
  const int lines = image.height();
  const int bytes = image.bytesPerLine();
  for (int y = 0; y < lines; ++y)
    std::memcpy(flipped.scanLine(y), image.constScanLine(lines - 1 - y),
                static_cast<std::size_t>(bytes));
  return flipped;
}

bool awaitBufferDescription(CaptureState &state, QString &error) {
  // v3 announces buffer_done after all buffer types; v1/v2 send a single
  // buffer event. Wait for whichever completes first.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
  const auto done = [&] {
    return state.hasBuffer &&
           (state.managerVersion < 3 || state.bufferDone);
  };
  while (!done()) {
    if (wl_display_dispatch_pending(state.display) < 0) {
      error = QStringLiteral("Wayland screencopy dispatch failed");
      return false;
    }
    if (done())
      return true;
    while (wl_display_prepare_read(state.display) != 0) {
      if (wl_display_dispatch_pending(state.display) < 0) {
        error = QStringLiteral("Wayland screencopy dispatch failed");
        return false;
      }
      if (done())
        return true;
    }
    if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
      wl_display_cancel_read(state.display);
      error = QStringLiteral("Could not flush Wayland screencopy request");
      return false;
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      wl_display_cancel_read(state.display);
      error = QStringLiteral("Wayland screencopy timed out");
      return false;
    }
    pollfd descriptor{wl_display_get_fd(state.display), POLLIN, 0};
    const int result =
        poll(&descriptor, 1, static_cast<int>(remaining.count()));
    if (result <= 0 || !(descriptor.revents & POLLIN)) {
      wl_display_cancel_read(state.display);
      error = result == 0
                  ? QStringLiteral("Wayland screencopy timed out")
                  : QStringLiteral("Wayland screencopy connection failed");
      return false;
    }
    if (wl_display_read_events(state.display) < 0) {
      error = QStringLiteral("Could not read Wayland screencopy response");
      return false;
    }
  }
  return true;
}

} // namespace capture_detail

using namespace capture_detail;

QImage normalizeWaylandCapture(const QImage &image, std::uint32_t transform) {
  // Kept for the editor/tests: maps WL_OUTPUT_TRANSFORM_* to an upright
  // image. The wlr-screencopy path itself never calls this with a transform;
  // it only applies y_invert (see applyYInvert).
  const auto rotated = [&image](qreal degrees) {
    return image.transformed(QTransform().rotate(degrees));
  };
  const auto flipped = [](const QImage &source) {
    return source.transformed(QTransform().scale(-1, 1));
  };
  switch (transform) {
  case WL_OUTPUT_TRANSFORM_NORMAL:
    return image;
  case WL_OUTPUT_TRANSFORM_90:
    return rotated(90);
  case WL_OUTPUT_TRANSFORM_180:
    return rotated(180);
  case WL_OUTPUT_TRANSFORM_270:
    return rotated(-90);
  case WL_OUTPUT_TRANSFORM_FLIPPED:
    return flipped(image);
  case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    return flipped(rotated(90));
  case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    return flipped(rotated(180));
  case WL_OUTPUT_TRANSFORM_FLIPPED_270:
    return flipped(rotated(-90));
  default:
    return {};
  }
}

/** Connects to the display and binds the screencopy globals. */
static bool connectScreencopyDisplay(CaptureState &state, QString &error) {
  StartupTimingScope timing("Wayland connect + registry roundtrips");
  state.display = wl_display_connect(nullptr);
  if (!state.display) {
    error = QStringLiteral("Could not connect to Wayland for output capture");
    return false;
  }
  state.registry = wl_display_get_registry(state.display);
  wl_registry_add_listener(state.registry, &kRegistryListener, &state);
  if (wl_display_roundtrip(state.display) < 0 ||
      wl_display_roundtrip(state.display) < 0) {
    error = QStringLiteral("Could not enumerate Wayland capture sources");
    return false;
  }
  if (!state.shm || !state.manager) {
    error = QStringLiteral(
        "Compositor does not expose wlr-screencopy output capture");
    return false;
  }
  return true;
}

static wl_output *findOutput(CaptureState &state, const QString &name) {
  const std::string wanted = name.toStdString();
  for (const auto &output : state.outputs) {
    if (output->name == wanted)
      return output->output;
  }
  return nullptr;
}

/** Captures one frame from `output` into `image` (cursor-free). */
static bool captureOneFrame(CaptureState &state, wl_output *output,
                            QImage &image, QString &error,
                            bool takeOwnership) {
  state.frame = zwlr_screencopy_manager_v1_capture_output(state.manager, 0,
                                                          output);
  if (!state.frame) {
    error = QStringLiteral("Could not start screencopy frame");
    return false;
  }
  zwlr_screencopy_frame_v1_add_listener(state.frame, &kFrameListener, &state);
  // The buffer description arrives as events on the new frame object; flush
  // the request then wait for it.
  wl_display_flush(state.display);
  if (!awaitBufferDescription(state, error)) {
    zwlr_screencopy_frame_v1_destroy(state.frame);
    state.frame = nullptr;
    return false;
  }
  if (!createShmBuffer(state, error)) {
    zwlr_screencopy_frame_v1_destroy(state.frame);
    state.frame = nullptr;
    return false;
  }
  state.frameDone = false;
  state.frameReady = false;
  state.frameFailed = false;
  state.flags = 0;
  zwlr_screencopy_frame_v1_copy(state.frame, state.buffer);
  wl_display_flush(state.display);
  if (!dispatchUntil(state, &state.frameDone, 2000, error)) {
    zwlr_screencopy_frame_v1_destroy(state.frame);
    state.frame = nullptr;
    return false;
  }
  zwlr_screencopy_frame_v1_destroy(state.frame);
  state.frame = nullptr;
  if (!state.frameReady) {
    error = QStringLiteral("Compositor could not capture the focused output");
    return false;
  }
  const bool yInvert =
      (state.flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
  if (takeOwnership) {
    QImage captured = takeCapturedImage(state);
    if (captured.isNull()) {
      error = QStringLiteral("Could not decode screencopy capture");
      return false;
    }
    image = applyYInvert(std::move(captured), yInvert);
  } else {
    QImage captured = copyCapturedImage(state);
    if (captured.isNull()) {
      error = QStringLiteral("Could not decode screencopy capture");
      return false;
    }
    image = applyYInvert(std::move(captured), yInvert);
  }
  if (image.isNull()) {
    error = QStringLiteral("Could not decode screencopy capture");
    return false;
  }
  return true;
}

bool captureOutputSurface(const MonitorInfo &monitor, QImage &image,
                          QString &error) {
  StartupTimingScope timing("native output capture total");
  if (monitor.name.isEmpty()) {
    error = QStringLiteral("Focused monitor has no output name");
    return false;
  }

  CaptureState state;
  if (!connectScreencopyDisplay(state, error))
    return false;

  wl_output *output = findOutput(state, monitor.name);
  if (!output) {
    error = QStringLiteral("Could not find Wayland output %1 for capture")
                .arg(monitor.name);
    return false;
  }
  return captureOneFrame(state, output, image, error, true);
}

struct OutputCapture::State : capture_detail::CaptureState {
  wl_output *target = nullptr;
};

OutputCapture::OutputCapture() = default;
OutputCapture::~OutputCapture() = default;

bool OutputCapture::open(const QString &outputName, QString &error) {
  close();
  auto state = std::make_unique<State>();
  if (!connectScreencopyDisplay(*state, error))
    return false;
  state->target = findOutput(*state, outputName);
  if (!state->target) {
    error = QStringLiteral("Output %1 is not available for native capture")
                .arg(outputName);
    return false;
  }
  state_ = std::move(state);
  return true;
}

bool OutputCapture::grab(QImage &image, QString &error, int timeoutMs) {
  if (!state_) {
    error = QStringLiteral("Output capture is not open");
    return false;
  }
  State &state = *state_;
  // Each grab uses a fresh protocol frame (screencopy frames are one-shot)
  // over the persistent connection; the SHM pool is rebuilt only when the
  // advertised geometry changes.
  state.frame = zwlr_screencopy_manager_v1_capture_output(state.manager, 0,
                                                          state.target);
  if (!state.frame) {
    error = QStringLiteral("Could not start screencopy frame");
    return false;
  }
  zwlr_screencopy_frame_v1_add_listener(state.frame, &kFrameListener, &state);
  state.hasBuffer = false;
  state.bufferDone = false;
  state.width = state.height = state.stride = state.format = 0;
  wl_display_flush(state.display);
  if (!awaitBufferDescription(state, error)) {
    zwlr_screencopy_frame_v1_destroy(state.frame);
    state.frame = nullptr;
    return false;
  }
  const bool geometryChanged =
      state.buffer == nullptr || state.width != state.bufferWidth ||
      state.height != state.bufferHeight || state.format != state.bufferFormat ||
      state.stride != state.bufferStride;
  if (geometryChanged) {
    destroyShmBuffer(state);
    if (!createShmBuffer(state, error)) {
      zwlr_screencopy_frame_v1_destroy(state.frame);
      state.frame = nullptr;
      return false;
    }
  }
  state.frameDone = false;
  state.frameReady = false;
  state.frameFailed = false;
  state.flags = 0;
  zwlr_screencopy_frame_v1_copy(state.frame, state.buffer);
  wl_display_flush(state.display);
  if (!dispatchUntil(state, &state.frameDone, timeoutMs, error)) {
    zwlr_screencopy_frame_v1_destroy(state.frame);
    state.frame = nullptr;
    return false;
  }
  zwlr_screencopy_frame_v1_destroy(state.frame);
  state.frame = nullptr;
  if (!state.frameReady) {
    error = QStringLiteral("Compositor could not capture the output");
    return false;
  }
  const bool yInvert =
      (state.flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
  QImage captured = copyCapturedImage(state);
  if (captured.isNull()) {
    error = QStringLiteral("Could not decode native output capture");
    return false;
  }
  image = applyYInvert(std::move(captured), yInvert);
  if (image.isNull()) {
    error = QStringLiteral("Could not decode native output capture");
    return false;
  }
  return true;
}

bool OutputCapture::isOpen() const { return state_ != nullptr; }

bool OutputCapture::sessionStopped() const { return false; }

QSize OutputCapture::bufferSize() const {
  if (!state_)
    return {};
  return {static_cast<int>(state_->bufferWidth),
          static_cast<int>(state_->bufferHeight)};
}

void OutputCapture::close() { state_.reset(); }
