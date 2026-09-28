#include "AnalysisController.h"

#include <QFileInfo>
#include <QFuture>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>
#include <filesystem>

namespace {

QVariantMap row (const QString& label, const QString& detail, double ms, double hz)
{
    QVariantMap map;
    map.insert ("label", label);
    map.insert ("detail", detail);
    map.insert ("ms", ms);
    map.insert ("hz", hz);
    return map;
}

QString formatMs (double ms)
{
    return QString::number (ms, 'f', ms < 100.0 ? 2 : 0) + " ms";
}

} // namespace

AnalysisController::AnalysisController (QObject* parent)
    : QObject (parent)
{
    connect (&watcher_, &QFutureWatcher<tmix::AnalysisResult>::finished,
             this, [this] {
                 const auto result = watcher_.result();
                 setBusy (false);

                 if (! result.ok)
                 {
                     if (! cancelRequested_.load())
                     {
                         error_ = QString::fromStdString (result.error);
                         emit resultChanged();
                         emit failed (error_);
                     }
                     return;
                 }

                 applyResult (result);
             });
}

AnalysisController::~AnalysisController()
{
    cancelRequested_.store (true);
    watcher_.waitForFinished();
}

void AnalysisController::setBusy (bool busy)
{
    if (busy_ == busy)
        return;

    busy_ = busy;
    emit busyChanged();
}

void AnalysisController::analyse (const QUrl& fileUrl)
{
    analysePath (fileUrl.toLocalFile());
}

void AnalysisController::analysePath (const QString& path)
{
    if (busy_)
    {
        cancelRequested_.store (true);
        watcher_.waitForFinished();
        cancelRequested_.store (false);
    }

    if (path.isEmpty())
        return;

    filePath_ = path;
    fileName_ = QFileInfo (path).fileName();
    error_.clear();
    cancelRequested_.store (false);

    progress_ = 0.0;
    stage_ = QStringLiteral ("decoding");
    emit progressChanged();

    setBusy (true);

    const std::filesystem::path fileSystemPath (path.toStdWString());

    watcher_.setFuture (QtConcurrent::run ([this, fileSystemPath] {
        return tmix::analyseFile (fileSystemPath, {}, [this] (float fraction, const char* stage) {
            if (cancelRequested_.load())
                return false;

            // Hop back to the GUI thread; the core callback runs on the worker.
            QMetaObject::invokeMethod (this, [this, fraction, stage] {
                progress_ = fraction;
                stage_ = QString::fromUtf8 (stage);
                emit progressChanged();
            }, Qt::QueuedConnection);

            return true;
        });
    }));
}

void AnalysisController::applyResult (const tmix::AnalysisResult& result)
{
    hasResult_ = true;
    detectedTempo_ = result.tempo;

    if (result.tempo.valid)
    {
        bpm_             = result.tempo.bpm;
        searchedBpm_     = result.tempo.rawBpm;
        tempoFolded_     = result.tempo.folded;
        tempoUncertain_  = result.tempo.uncertain;
        tempoConfidence_ = result.tempo.confidence;

        tempoCandidates_.clear();
        for (const auto& candidate : result.tempo.candidates)
        {
            QVariantMap map;
            map.insert ("bpm", candidate.bpm);
            map.insert ("score", candidate.score);
            map.insert ("half", candidate.halfOfPrimary);
            map.insert ("double", candidate.doubleOfPrimary);
            tempoCandidates_.append (map);
        }
    }
    else
    {
        bpm_ = 0.0;
        searchedBpm_ = 0.0;
        tempoFolded_ = false;
        tempoUncertain_ = false;
        tempoConfidence_ = 0.0;
        tempoCandidates_.clear();
    }

    tuningValid_  = result.key.tuningValid;
    referenceHz_  = result.key.referenceHz;
    tuningCents_  = result.key.tuningCents;

    keyName_        = QString::fromStdString (result.key.name);
    keyUncertain_   = result.key.uncertain;
    keyConfidence_  = result.key.confidence;

    keyCandidates_.clear();
    for (const auto& candidate : result.key.candidates)
    {
        QVariantMap map;
        map.insert ("name", QString::fromStdString (candidate.name));
        map.insert ("score", candidate.score);
        keyCandidates_.append (map);
    }

    if (bpm_ > 0.0)
        rebuildTables (bpm_);
    else
    {
        noteValues_.clear();
        delays_.clear();
        attacks_.clear();
        releases_.clear();
        syncedReleases_.clear();
        reverbs_.clear();
        preDelays_.clear();
    }

    emit resultChanged();
}

void AnalysisController::setBpm (double bpm)
{
    if (! (bpm > 0.0) || ! std::isfinite (bpm))
        return;

    bpm_ = bpm;

    // The tables are what the user actually reads, so they follow the manual
    // value immediately rather than waiting for another analysis.
    rebuildTables (bpm_);
    emit resultChanged();
}

void AnalysisController::restoreDetectedTempo()
{
    if (! detectedTempo_.valid)
        return;

    bpm_ = detectedTempo_.bpm;
    rebuildTables (bpm_);
    emit resultChanged();
}

void AnalysisController::rebuildTables (double bpm)
{
    const tmix::NoteTiming note (bpm);
    noteValues_.clear();
    for (const auto& entry : note.table())
        noteValues_.append (row (QString::fromStdString (entry.label),
                                 QString(), entry.ms, entry.hz));

    const tmix::MixTiming timing (bpm);

    delays_.clear();
    for (const auto& preset : timing.delayPresets())
    {
        QVariantMap map;
        map.insert ("name", QString::fromStdString (preset.name));
        map.insert ("leftMs", preset.leftMs);
        map.insert ("rightMs", preset.rightMs);
        delays_.append (map);
    }

    const auto toList = [] (const std::vector<tmix::TimeValue>& values) {
        QVariantList list;
        for (const auto& value : values)
            list.append (row (QString::fromStdString (value.label),
                              QString::fromStdString (value.use),
                              value.ms, value.hz));
        return list;
    };

    attacks_        = toList (timing.attackSuggestions());
    releases_       = toList (timing.releaseSuggestions());
    syncedReleases_ = toList (timing.releaseSyncedOptions());

    reverbs_.clear();
    for (const auto& entry : timing.reverbDecays())
        reverbs_.append (row (QString::fromStdString (entry.label),
                              QString::fromStdString (entry.use),
                              entry.ms, 0.0));

    preDelays_ = toList (timing.preDelays());
}
