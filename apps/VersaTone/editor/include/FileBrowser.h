/*
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•—    â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— 
 * â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘    â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â•â•â• 
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘ â–ˆâ•— â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ–ˆâ•—
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ–ˆâ•”â–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•
 * â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â• â•šâ•â•â•â•šâ•â•â•  â•šâ•â•â•â•â•â• 
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: FileBrowser.h
 * Purpose: Header file for audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#pragma once
#include <QWidget>
#include <QStringList>
#include <QString>
#include <QMap>

class SimpleWavPlayer;

class QTreeView;
class QFileSystemModel;
class QPushButton;
class QVBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QAction;
class QComboBox;
class QSplitter;
class QTextEdit;
class QBuffer;
class QListWidget;

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    enum SampleCategory {
        All = 0,
        Drums,
        Bass,
        Synth,
        Vocal,
        FX,
        Instrument,
        Loop,
        OneShot,
        Ambient,
        Percussion
    };
    
    enum SampleGenre {
        AllGenres = 0,
        Electronic,
        HipHop,
        Rock,
        Pop,
        Jazz,
        Classical,
        Experimental,
        World,
        Cinematic
    };

    explicit FileBrowser(QWidget* parent = nullptr);
    ~FileBrowser();

signals:
    void sampleSelected(const QString& filePath);
    void libraryAdded(const QString& folderPath);
    void sampleDragStarted(const QString& filePath);
    void batchSamplesSelected(const QStringList& filePaths);
    void smartFilterApplied(const QString& filterType, const QStringList& results);
    void sampleDroppedOnTrack(const QString& filePath, int trackIndex);
    void sampleDroppedOnSequencer(const QString& filePath, int step, int track);
    void sampleDroppedOnMixer(const QString& filePath, int channelIndex);

private slots:
    void onFileDoubleClicked(const QModelIndex& index);
    void onAddLibrary();
    void onRefreshLibraries();
    void onContextMenuRequested(const QPoint& pos);
    void showFileContextMenu(const QString& filePath, const QPoint& globalPos);
    void onPreviewSample();
    void onStopPreview();
    void onCategoryChanged(int index);
    void onGenreChanged(int index);
    void onClearFilters();
    void onBatchLoad();
    void onSmartFilter();
    void onSelectAllAudio();
    void onLoadSelectedSamples();

protected:
    // void mousePressEvent(QMouseEvent* event) override;  // Now handled via eventFilter
    // void mouseMoveEvent(QMouseEvent* event) override;   // Now handled via eventFilter
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void startDrag(const QString& filePath);

private:
    void setupUI();
    void setupFileFilters();
    void setupCategoryFilter();
    bool isAudioFile(const QString& filePath) const;
    void addLibraryFolder(const QString& folderPath);
    bool matchesCurrentFilter(const QString& filePath) const;
    QString getCategoryKeywords(SampleCategory category) const;
    QString getGenreKeywords(SampleGenre genre) const;
    
    QTreeView* treeView;
    QFileSystemModel* fileSystemModel;
    QPushButton* addLibraryBtn;
    QPushButton* refreshBtn;
    QLineEdit* searchBox;
    QLabel* statusLabel;
    
    QStringList audioExtensions;
    QStringList libraryPaths;
    QMenu* contextMenu;
    QAction* loadSampleAction;
    QAction* addToLibraryAction;
    QAction* previewAction;
    QAction* stopPreviewAction;
    
    // Drag & Drop
    QPoint dragStartPos;
    QString currentPreviewFile;
    
    // Sample Preview
    SimpleWavPlayer* wavPlayer;
    
    // Library Management
    QComboBox* librarySelector;
    QPushButton* clearFiltersBtn;
    QLabel* activeFiltersLabel;
    
    SampleCategory currentCategory;
    SampleGenre currentGenre;
    QMap<SampleCategory, QStringList> categoryKeywords;
    QMap<SampleGenre, QStringList> genreKeywords;
    
    void updateLibrarySelector();
    void navigateToLibrary(const QString& libraryPath);
    QStringList getSelectedAudioFiles() const;
    QStringList scanForAudioFiles(const QString& directory, const QString& filterType = "") const;
    void applySmartFilter(const QString& filterType);
    void updateSelectionDisplay();
    QString detectSampleCategory(const QString& filePath) const;
    QString getCategoryIcon(const QString& category) const;
    void analyzeAndAddLibrary(const QString& folderPath);
    
    // Batch Operations
    QPushButton* batchLoadBtn;
    QPushButton* smartFilterBtn;
    QPushButton* selectAllBtn;
    QLabel* selectionCountLabel;
    QTextEdit* batchResultsDisplay;
    
    QStringList selectedFiles;
    QString currentSmartFilter;
    
    // Loaded Samples Management
    QStringList loadedSamples;
    QListWidget* loadedSamplesView;
};

