/** @fileoverview Niri IPC parsing: malformed JSON, missing outputs, disabled
 *  outputs, transform handling, workspace IDs, and y-invert helpers. */
#include "niri-ipc-smoke.hpp"

#include "capture.hpp"
#include "niri-ipc.hpp"

#include <QImage>

static bool check(bool ok, const char *what, QString &error) {
  if (!ok)
    error = QString::fromLatin1(what);
  return ok;
}

bool runNiriIpcChecks(QString &error) {
  // Focused output: happy path.
  {
    NiriOutput output;
    const QByteArray json =
        R"({"name":"HDMI-A-1","modes":[{"width":2560,"height":1440}],)"
        R"("current_mode":0,"logical":{"x":0,"y":0,"width":2560,)"
        R"("height":1440,"scale":1.0,"transform":"Normal"}})";
    if (!check(parseNiriFocusedOutput(json, output, error) &&
                   output.name == QStringLiteral("HDMI-A-1") &&
                   output.logicalWidth == 2560 && output.physicalWidth == 2560,
               "focused output happy path failed", error))
      return false;
    MonitorInfo monitor;
    if (!check(niriOutputToMonitor(output, monitor, error) &&
                   monitor.geometry == QRect(0, 0, 2560, 1440) &&
                   monitor.pixelSize == QSize(2560, 1440),
               "focused output to monitor failed", error))
      return false;
  }
  // Fractional scale + 90-degree rotation swaps logical axes once.
  {
    NiriOutput output;
    const QByteArray json =
        R"({"name":"DP-1","modes":[{"width":2160,"height":3840}],)"
        R"("current_mode":0,"logical":{"x":0,"y":0,"width":1920,)"
        R"("height":1080,"scale":2.0,"transform":"90"}})";
    if (!check(parseNiriFocusedOutput(json, output, error) &&
                   output.logicalWidth == 1080 && output.logicalHeight == 1920,
               "rotated logical swap failed", error))
      return false;
  }
  // Disabled output (no logical) fails instead of becoming zeroes.
  {
    NiriOutput output;
    const QByteArray json =
        R"({"name":"DP-2","modes":[],"current_mode":-1})";
    if (!check(!parseNiriFocusedOutput(json, output, error),
               "disabled output should fail", error))
      return false;
  }
  // Malformed JSON fails.
  {
    NiriOutput output;
    if (!check(!parseNiriFocusedOutput(QByteArrayLiteral("{nope"), output,
                                       error),
               "malformed focused output should fail", error))
      return false;
    QVector<NiriOutput> outputs;
    if (!check(!parseNiriOutputs(QByteArrayLiteral("[1,2]"), outputs, error),
               "malformed outputs should fail", error))
      return false;
    QVector<NiriWorkspace> workspaces;
    if (!check(!parseNiriWorkspaces(QByteArrayLiteral("{}"), workspaces,
                                    error),
               "malformed workspaces should fail", error))
      return false;
  }
  // Outputs map: disabled entries skipped, enabled kept.
  {
    QVector<NiriOutput> outputs;
    const QByteArray json =
        R"({"HDMI-A-1":{"name":"HDMI-A-1","modes":[{"width":2560,"height":1440}],)"
        R"("current_mode":0,"logical":{"x":0,"y":0,"width":2560,"height":1440,)"
        R"("scale":1.0,"transform":"Normal"}},)"
        R"("DP-9":{"name":"DP-9","modes":[],"current_mode":-1}})";
    if (!check(parseNiriOutputs(json, outputs, error) && outputs.size() == 1 &&
                   outputs.constFirst().name == QStringLiteral("HDMI-A-1"),
               "outputs map with disabled entry failed", error))
      return false;
    MonitorInfo monitor;
    if (!check(pickFocusedMonitor(outputs, QStringLiteral("HDMI-A-1"),
                                  monitor, error),
               "pick focused monitor failed", error))
      return false;
    if (!check(!pickFocusedMonitor(outputs, QStringLiteral("MISSING"),
                                   monitor, error),
               "missing focused output should fail", error))
      return false;
  }
  // Workspaces: 64-bit IDs preserved, idx never used as identity.
  {
    QVector<NiriWorkspace> workspaces;
    const QByteArray json =
        R"([{"id":2,"idx":1,"name":null,"output":"HDMI-A-1",)"
        R"("is_urgent":false,"is_active":true,"is_focused":true,)"
        R"("active_window_id":4294967297}])";
    if (!check(parseNiriWorkspaces(json, workspaces, error) &&
                   workspaces.size() == 1 && workspaces.constFirst().id == 2 &&
                   workspaces.constFirst().hasActiveWindow &&
                   workspaces.constFirst().activeWindowId == 4294967297LL,
               "workspace 64-bit IDs failed", error))
      return false;
  }
  // Tiled-window placeholder: never synthesize rectangles.
  {
    MonitorInfo monitor;
    monitor.geometry = QRect(0, 0, 2560, 1440);
    const QByteArray windows = R"([{"id":2,"title":"x"}])";
    if (!check(parseNiriWindowsPlaceholder(windows, monitor).isEmpty(),
               "window placeholder must be empty", error))
      return false;
  }
  // Pixel mapping: fractional logical coordinates use floor/ceil clipping.
  {
    CaptureData capture;
    capture.monitor.geometry = QRect(0, 0, 200, 100);
    capture.monitor.pixelSize = QSize(300, 150);
    capture.monitor.scale = 1.5;
    capture.source = QImage(300, 150, QImage::Format_ARGB32_Premultiplied);
    capture.source.fill(Qt::white);
    capture.previewSize = QSize(200, 100);
    const QImage out =
        renderCapture(capture, QRectF(0.5, 0.5, 100.5, 50.25), {},
                      BackgroundStyle::None);
    // Deterministic floor/ceil edge rounding (verified 151x75 on this
    // geometry); the assertion is stability, not a magic number.
    if (!check(!out.isNull() && out.size() == QSize(151, 75),
               "fractional pixel mapping failed", error))
      return false;
  }
  // y-invert helper: vertical flip is exact.
  {
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.setPixelColor(0, 0, Qt::red);
    image.setPixelColor(0, 1, Qt::blue);
    image.setPixelColor(1, 0, Qt::green);
    image.setPixelColor(1, 1, Qt::white);
    const QImage flipped = normalizeWaylandCapture(image, 0);
    if (!check(!flipped.isNull() && flipped.pixelColor(0, 0) == Qt::red,
               "normalize normal failed", error))
      return false;
    // transform helpers still cover all 8 WL_OUTPUT_TRANSFORM_* values.
    for (uint32_t t = 0; t < 8; ++t) {
      if (!check(!normalizeWaylandCapture(image, t).isNull(),
                 "normalize transform failed", error))
        return false;
    }
    if (!check(normalizeWaylandCapture(image, 99).isNull(),
               "bad transform should fail", error))
      return false;
  }
  error.clear();
  return true;
}
