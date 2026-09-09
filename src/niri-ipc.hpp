/** @fileoverview Niri compositor metadata via `niri msg -j`.
 *
 *  Nirisnap queries Niri with small, bounded subprocess calls
 *  (`niri msg -j focused-output`, `outputs`, `workspaces`, and `windows` when
 *  window context is needed). Replies are validated JSON, never trusted as an
 *  atomic desktop snapshot: callers re-validate output identity around pixel
 *  capture and cancel/retry on change.
 */
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct MonitorInfo;
struct WindowTarget;

/// One Niri output in logical space, plus physical mode info.
struct NiriOutput {
  QString name;
  int x = 0;
  int y = 0;
  int logicalWidth = 0;
  int logicalHeight = 0;
  qreal scale = 1.0;
  /// Niri transform name ("Normal", "90", "180", "270", "Flipped", ...).
  QString transform;
  int physicalWidth = 0;
  int physicalHeight = 0;
  bool hasLogical = false;
  bool isFocused = false;
};

/// Minimal workspace identity (IDs are 64-bit-safe; indices are not IDs).
struct NiriWorkspace {
  qint64 id = 0;
  int idx = 0;
  QString output;
  bool isActive = false;
  bool isFocused = false;
  qint64 activeWindowId = 0;
  bool hasActiveWindow = false;
};

/// Run `niri msg -j <args>` with a bounded timeout. Returns stdout on success.
[[nodiscard]] bool runNiriMsg(const QStringList &args, QByteArray &stdoutBytes,
                              QString &error, int timeoutMs = 5000);

/// Parse `niri msg -j focused-output` (a single output object).
[[nodiscard]] bool parseNiriFocusedOutput(const QByteArray &json,
                                          NiriOutput &output, QString &error);
/// Parse `niri msg -j outputs` (a name-keyed object of outputs).
[[nodiscard]] bool parseNiriOutputs(const QByteArray &json,
                                    QVector<NiriOutput> &outputs,
                                    QString &error);
/// Parse `niri msg -j workspaces` (an array).
[[nodiscard]] bool parseNiriWorkspaces(const QByteArray &json,
                                       QVector<NiriWorkspace> &workspaces,
                                       QString &error);

/// Focused output + workspaces in one helper (two subprocess calls).
[[nodiscard]] bool queryNiriFocusedOutput(NiriOutput &output, QString &error);
[[nodiscard]] bool queryNiriOutputs(QVector<NiriOutput> &outputs,
                                    QString &error);

/// Convert a validated Niri output into the capture MonitorInfo.
/// Disabled outputs (no `logical`) fail rather than becoming zeroes.
/// `focusedName` marks which output is focused when parsing `outputs`.
[[nodiscard]] bool niriOutputToMonitor(const NiriOutput &output,
                                       MonitorInfo &monitor, QString &error);
[[nodiscard]] bool pickFocusedMonitor(const QVector<NiriOutput> &outputs,
                                      const QString &focusedName,
                                      MonitorInfo &monitor, QString &error);

/// Window discovery for Niri: tiled windows have NO reliable screen
/// coordinates (`tile_pos_in_workspace_view` is null in practice), so this
/// intentionally returns an empty list. The editor must not synthesize window
/// rectangles from column/row indices, widths, gaps, or scroll offsets.
/// The function exists so call sites keep their shape and the app-id
/// heuristic for filenames keeps working.
[[nodiscard]] QVector<WindowTarget>
parseNiriWindowsPlaceholder(const QByteArray &json, const MonitorInfo &monitor);
