/** @fileoverview Niri JSON IPC: focused output / outputs / workspaces. */
#include "niri-ipc.hpp"
#include "capture.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

bool transformSwapsAxes(const QString &transform) {
  const QString t = transform.trimmed().toLower();
  return t == QStringLiteral("90") || t == QStringLiteral("270") ||
         t == QStringLiteral("flipped-90") || t == QStringLiteral("flipped-270") ||
         t == QStringLiteral("90-odd") || t == QStringLiteral("270-odd");
}

bool parseSingleOutputObject(const QJsonObject &object, NiriOutput &output,
                             QString &error) {
  output.name = object.value(QStringLiteral("name")).toString();
  if (output.name.isEmpty()) {
    error = QStringLiteral("Niri output has no name");
    return false;
  }
  const QJsonObject logical =
      object.value(QStringLiteral("logical")).toObject();
  if (logical.isEmpty()) {
    error = QStringLiteral("Niri output %1 is disabled or has no logical "
                           "geometry")
                .arg(output.name);
    return false;
  }
  output.x = logical.value(QStringLiteral("x")).toInt();
  output.y = logical.value(QStringLiteral("y")).toInt();
  output.logicalWidth = logical.value(QStringLiteral("width")).toInt();
  output.logicalHeight = logical.value(QStringLiteral("height")).toInt();
  output.scale = logical.value(QStringLiteral("scale")).toDouble(1.0);
  if (!(output.scale > 0.01))
    output.scale = 1.0;
  output.transform = logical.value(QStringLiteral("transform")).toString();
  if (output.transform.isEmpty())
    output.transform = QStringLiteral("Normal");
  // Physical size comes from the current mode index.
  const QJsonArray modes = object.value(QStringLiteral("modes")).toArray();
  const int current = object.value(QStringLiteral("current_mode")).toInt(-1);
  if (current >= 0 && current < modes.size()) {
    const QJsonObject mode = modes.at(current).toObject();
    output.physicalWidth = mode.value(QStringLiteral("width")).toInt();
    output.physicalHeight = mode.value(QStringLiteral("height")).toInt();
  }
  if (output.physicalWidth <= 0 || output.physicalHeight <= 0) {
    // Fall back to logical*scale; never leave zeroes that later become a
    // bogus geometry.
    output.physicalWidth =
        qRound(output.logicalWidth * output.scale);
    output.physicalHeight =
        qRound(output.logicalHeight * output.scale);
  }
  if (output.logicalWidth <= 0 || output.logicalHeight <= 0) {
    error = QStringLiteral("Niri output %1 reported an empty logical size")
                .arg(output.name);
    return false;
  }
  if (transformSwapsAxes(output.transform))
    std::swap(output.logicalWidth, output.logicalHeight);
  output.hasLogical = true;
  return true;
}

} // namespace

bool runNiriMsg(const QStringList &args, QByteArray &stdoutBytes,
                QString &error, int timeoutMs) {
  QProcess process;
  process.setProcessChannelMode(QProcess::SeparateChannels);
  QStringList full{QStringLiteral("msg"), QStringLiteral("-j")};
  full.append(args);
  process.start(QStringLiteral("niri"), full);
  if (!process.waitForStarted(2000)) {
    error = QStringLiteral("Could not start niri msg: %1")
                .arg(process.errorString());
    return false;
  }
  process.closeWriteChannel();
  const bool finished = process.waitForFinished(timeoutMs);
  if (!finished) {
    process.kill();
    error = QStringLiteral("niri msg timed out (%1)")
                .arg(args.join(QLatin1Char(' ')));
    return false;
  }
  if (process.exitCode() != 0) {
    const QString detail =
        QString::fromUtf8(process.readAllStandardError()).trimmed();
    error = detail.isEmpty()
                ? QStringLiteral("niri msg failed: %1").arg(args.join(QLatin1Char(' ')))
                : detail;
    return false;
  }
  stdoutBytes = process.readAllStandardOutput();
  return true;
}

bool parseNiriFocusedOutput(const QByteArray &json, NiriOutput &output,
                            QString &error) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    error = QStringLiteral("Could not parse Niri focused output: %1")
                .arg(parseError.errorString());
    return false;
  }
  NiriOutput parsed;
  if (!parseSingleOutputObject(document.object(), parsed, error))
    return false;
  parsed.isFocused = true;
  output = parsed;
  return true;
}

bool parseNiriOutputs(const QByteArray &json, QVector<NiriOutput> &outputs,
                      QString &error) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    error = QStringLiteral("Could not parse Niri outputs: %1")
                .arg(parseError.errorString());
    return false;
  }
  const QJsonObject root = document.object();
  QVector<NiriOutput> parsed;
  for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
    if (!it.value().isObject())
      continue;
    NiriOutput output;
    QString outputError;
    // Disabled outputs have no logical geometry; skip them instead of
    // turning missing values into zeroes.
    if (!parseSingleOutputObject(it.value().toObject(), output, outputError))
      continue;
    parsed.push_back(output);
  }
  if (parsed.isEmpty()) {
    error = QStringLiteral("Niri reported no enabled outputs");
    return false;
  }
  outputs = parsed;
  return true;
}

bool parseNiriWorkspaces(const QByteArray &json,
                         QVector<NiriWorkspace> &workspaces, QString &error) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    error = QStringLiteral("Could not parse Niri workspaces: %1")
                .arg(parseError.errorString());
    return false;
  }
  QVector<NiriWorkspace> parsed;
  for (const QJsonValue value : document.array()) {
    const QJsonObject object = value.toObject();
    NiriWorkspace workspace;
    // IDs are 64-bit; never use idx as identity.
    workspace.id =
        static_cast<qint64>(object.value(QStringLiteral("id")).toDouble(-1));
    workspace.idx = object.value(QStringLiteral("idx")).toInt(-1);
    workspace.output = object.value(QStringLiteral("output")).toString();
    workspace.isActive = object.value(QStringLiteral("is_active")).toBool();
    workspace.isFocused = object.value(QStringLiteral("is_focused")).toBool();
    const QJsonValue active = object.value(QStringLiteral("active_window_id"));
    workspace.hasActiveWindow = !active.isNull();
    if (workspace.hasActiveWindow)
      workspace.activeWindowId = static_cast<qint64>(active.toDouble(-1));
    if (workspace.id < 0)
      continue; // Preserve optional fields; drop unusable entries.
    parsed.push_back(workspace);
  }
  workspaces = parsed;
  return true;
}

bool queryNiriFocusedOutput(NiriOutput &output, QString &error) {
  QByteArray stdoutBytes;
  if (!runNiriMsg({QStringLiteral("focused-output")}, stdoutBytes, error))
    return false;
  return parseNiriFocusedOutput(stdoutBytes, output, error);
}

bool queryNiriOutputs(QVector<NiriOutput> &outputs, QString &error) {
  QByteArray stdoutBytes;
  if (!runNiriMsg({QStringLiteral("outputs")}, stdoutBytes, error))
    return false;
  return parseNiriOutputs(stdoutBytes, outputs, error);
}

bool niriOutputToMonitor(const NiriOutput &output, MonitorInfo &monitor,
                         QString &error) {
  if (!output.hasLogical || output.logicalWidth <= 0 ||
      output.logicalHeight <= 0) {
    error = QStringLiteral("Niri output %1 has no usable geometry")
                .arg(output.name);
    return false;
  }
  monitor.name = output.name;
  monitor.geometry = {output.x, output.y, output.logicalWidth,
                      output.logicalHeight};
  monitor.pixelSize = {output.physicalWidth, output.physicalHeight};
  monitor.scale = output.scale;
  // Niri workspace IDs are 64-bit; MonitorInfo::workspaceId is an int used
  // only for display. Keep 0 (unknown) rather than truncating an ID into it.
  monitor.workspaceId = 0;
  return true;
}

bool pickFocusedMonitor(const QVector<NiriOutput> &outputs,
                        const QString &focusedName, MonitorInfo &monitor,
                        QString &error) {
  for (const NiriOutput &output : outputs) {
    if (output.name == focusedName)
      return niriOutputToMonitor(output, monitor, error);
  }
  error = QStringLiteral("Niri did not report focused output %1")
              .arg(focusedName);
  return false;
}

QVector<WindowTarget> parseNiriWindowsPlaceholder(const QByteArray &,
                                                  const MonitorInfo &) {
  // Deliberate: no tiled-window screen coordinates are available from Niri
  // IPC (see PLAN.md §2). Return empty so the editor shows region/full-monitor
  // only, instead of a fabricated window list.
  return {};
}
