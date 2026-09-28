#pragma once

#include "Analysis.h"
#include "MixTiming.h"
#include "NoteTiming.h"

#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <atomic>
#include <memory>

// Bridges the analysis core to QML.
//
// The core knows nothing about Qt; this class owns the worker thread, turns
// progress callbacks into signals, and rebuilds the derived tables whenever
// the tempo changes. All the numeric work stays in the core so it can be
// verified from the command line and the test binary alone.
class AnalysisController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY (bool    busy       READ busy       NOTIFY busyChanged)
    Q_PROPERTY (double  progress   READ progress   NOTIFY progressChanged)
    Q_PROPERTY (QString stage      READ stage      NOTIFY progressChanged)

    Q_PROPERTY (bool    hasResult  READ hasResult  NOTIFY resultChanged)
    Q_PROPERTY (QString fileName   READ fileName   NOTIFY resultChanged)
    Q_PROPERTY (QString filePath   READ filePath   NOTIFY resultChanged)
    Q_PROPERTY (QString error      READ error      NOTIFY resultChanged)

    Q_PROPERTY (double  bpm            READ bpm            NOTIFY resultChanged)
    Q_PROPERTY (double  searchedBpm    READ searchedBpm    NOTIFY resultChanged)
    Q_PROPERTY (bool    tempoFolded    READ tempoFolded    NOTIFY resultChanged)
    Q_PROPERTY (bool    tempoUncertain READ tempoUncertain NOTIFY resultChanged)
    Q_PROPERTY (double  tempoConfidence READ tempoConfidence NOTIFY resultChanged)
    Q_PROPERTY (QVariantList tempoCandidates READ tempoCandidates NOTIFY resultChanged)

    Q_PROPERTY (bool    tuningValid    READ tuningValid    NOTIFY resultChanged)
    Q_PROPERTY (double  referenceHz    READ referenceHz    NOTIFY resultChanged)
    Q_PROPERTY (double  tuningCents    READ tuningCents    NOTIFY resultChanged)

    Q_PROPERTY (QString keyName        READ keyName        NOTIFY resultChanged)
    Q_PROPERTY (bool    keyUncertain   READ keyUncertain   NOTIFY resultChanged)
    Q_PROPERTY (double  keyConfidence  READ keyConfidence  NOTIFY resultChanged)
    Q_PROPERTY (QVariantList keyCandidates READ keyCandidates NOTIFY resultChanged)

    Q_PROPERTY (QVariantList noteValues     READ noteValues     NOTIFY resultChanged)
    Q_PROPERTY (QVariantList delays         READ delays         NOTIFY resultChanged)
    Q_PROPERTY (QVariantList attacks        READ attacks        NOTIFY resultChanged)
    Q_PROPERTY (QVariantList releases       READ releases       NOTIFY resultChanged)
    Q_PROPERTY (QVariantList syncedReleases READ syncedReleases NOTIFY resultChanged)
    Q_PROPERTY (QVariantList reverbs        READ reverbs        NOTIFY resultChanged)
    Q_PROPERTY (QVariantList preDelays      READ preDelays      NOTIFY resultChanged)

public:
    explicit AnalysisController (QObject* parent = nullptr);
    ~AnalysisController() override;

    bool    busy() const { return busy_; }
    double  progress() const { return progress_; }
    QString stage() const { return stage_; }

    bool    hasResult() const { return hasResult_; }
    QString fileName() const { return fileName_; }
    QString filePath() const { return filePath_; }
    QString error() const { return error_; }

    double  bpm() const { return bpm_; }
    double  searchedBpm() const { return searchedBpm_; }
    bool    tempoFolded() const { return tempoFolded_; }
    bool    tempoUncertain() const { return tempoUncertain_; }
    double  tempoConfidence() const { return tempoConfidence_; }
    QVariantList tempoCandidates() const { return tempoCandidates_; }

    bool   tuningValid() const { return tuningValid_; }
    double referenceHz() const { return referenceHz_; }
    double tuningCents() const { return tuningCents_; }

    QString keyName() const { return keyName_; }
    bool    keyUncertain() const { return keyUncertain_; }
    double  keyConfidence() const { return keyConfidence_; }
    QVariantList keyCandidates() const { return keyCandidates_; }

    QVariantList noteValues() const { return noteValues_; }
    QVariantList delays() const { return delays_; }
    QVariantList attacks() const { return attacks_; }
    QVariantList releases() const { return releases_; }
    QVariantList syncedReleases() const { return syncedReleases_; }
    QVariantList reverbs() const { return reverbs_; }
    QVariantList preDelays() const { return preDelays_; }

    // Starts an analysis on a worker thread. A second call while one is
    // running cancels the first.
    Q_INVOKABLE void analyse (const QUrl& fileUrl);

    // Same, from a plain filesystem path. Used when a file is handed to the
    // application on the command line.
    Q_INVOKABLE void analysePath (const QString& path);

    // Manual tempo correction. Every derived table is rebuilt from it, so the
    // user can set a tempo by hand and still get the whole reference sheet.
    Q_INVOKABLE void setBpm (double bpm);

    // Restores the tempo the search produced, undoing a manual correction.
    Q_INVOKABLE void restoreDetectedTempo();

private:
    void applyResult (const tmix::AnalysisResult& result);
    void rebuildTables (double bpm);
    void setBusy (bool busy);

    QFutureWatcher<tmix::AnalysisResult> watcher_;
    std::atomic<bool> cancelRequested_ { false };

    bool    busy_     = false;
    double  progress_ = 0.0;
    QString stage_;

    bool    hasResult_ = false;
    QString fileName_;
    QString filePath_;
    QString error_;

    double bpm_             = 0.0;
    double searchedBpm_     = 0.0;
    bool   tempoFolded_     = false;
    bool   tempoUncertain_  = false;
    double tempoConfidence_ = 0.0;

    bool    tuningValid_   = false;
    double  referenceHz_   = 440.0;
    double  tuningCents_   = 0.0;

    QString keyName_;
    bool    keyUncertain_  = false;
    double  keyConfidence_ = 0.0;

    tmix::TempoResult     detectedTempo_;
    QVariantList          tempoCandidates_;
    QVariantList          keyCandidates_;

    QVariantList noteValues_;
    QVariantList delays_;
    QVariantList attacks_;
    QVariantList releases_;
    QVariantList syncedReleases_;
    QVariantList reverbs_;
    QVariantList preDelays_;

signals:
    void busyChanged();
    void progressChanged();
    void resultChanged();
    void failed (const QString& message);
};
