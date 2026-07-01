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
 * File: FileBrowser.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "FileBrowser.h"
#include "SimpleWavPlayer.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QUrl>
#include <QComboBox>
#include <QPainter>
#include <QTextEdit>
#include <QBuffer>
#include <QCheckBox>
#include <QProgressBar>
#include <QTimer>
#include <QDirIterator>
#include <QThread>
#include <QListWidget>
#include <QDirIterator>

FileBrowser::FileBrowser(QWidget* parent)
    : QWidget(parent)
    , wavPlayer(nullptr)
    , librarySelector(nullptr)
    , batchLoadBtn(nullptr)
    , smartFilterBtn(nullptr)
    , selectAllBtn(nullptr)
    , selectionCountLabel(nullptr)
    , batchResultsDisplay(nullptr)
    , currentCategory(All)
    , currentGenre(AllGenres)
{
    setAcceptDrops(true); // Enable drag and drop for the FileBrowser
    setupUI();
    setupFileFilters();
    setupCategoryFilter();
    
    // Initialize enhanced audio preview system
    wavPlayer = new SimpleWavPlayer(this);
    wavPlayer->setVolume(0.7f);
    
    // Connect signals
    connect(wavPlayer, &SimpleWavPlayer::playbackFinished, this, &FileBrowser::onStopPreview);
    connect(wavPlayer, &SimpleWavPlayer::playbackStarted, this, [this]() {
        statusLabel->setText(QString("ðŸŽµ Playing: %1").arg(QFileInfo(wavPlayer->currentFile()).fileName()));
    });
    
    // Load saved library paths
    QSettings settings;
    libraryPaths = settings.value("libraryPaths", QStringList()).toStringList();
    
    // Update library selector with saved libraries
    updateLibrarySelector();
}

FileBrowser::~FileBrowser() {
    // Save library paths
    QSettings settings;
    settings.setValue("libraryPaths", libraryPaths);
}

void FileBrowser::setupUI() {
    // Apply professional dark theme styling
    setStyleSheet("FileBrowser { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a2a2a, stop:1 #1a1a1a); "
                  "border: 1px solid #404040; "
                  "} "
                  "QTreeView, QListView { "
                  "background-color: #1a1a1a; "
                  "color: #ffffff; "
                  "border: 1px solid #404040; "
                  "selection-background-color: #0078d4; "
                  "alternate-background-color: #2a2a2a; "
                  "} "
                  "QTreeView::item:hover, QListView::item:hover { "
                  "background-color: #404040; "
                  "} "
                  "QTreeView::item:selected, QListView::item:selected { "
                  "background-color: #0078d4; "
                  "color: #ffffff; "
                  "} "
                  "QHeaderView::section { "
                  "background-color: #404040; "
                  "color: #ffffff; "
                  "border: 1px solid #555555; "
                  "padding: 4px; "
                  "} "
                  "QPushButton { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #404040, stop:1 #2a2a2a); "
                  "border: 1px solid #555555; "
                  "border-radius: 4px; "
                  "color: #ffffff; "
                  "font-weight: bold; "
                  "padding: 6px 12px; "
                  "} "
                  "QPushButton:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #505050, stop:1 #404040); "
                  "border: 1px solid #0078d4; "
                  "} "
                  "QPushButton:pressed { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0078d4, stop:1 #005a9e); "
                  "} "
                  "QLineEdit { "
                  "background-color: #1a1a1a; "
                  "border: 1px solid #404040; "
                  "border-radius: 4px; "
                  "color: #ffffff; "
                  "padding: 6px; "
                  "} "
                  "QLineEdit:focus { "
                  "border: 1px solid #0078d4; "
                  "} "
                  "QScrollBar:vertical { "
                  "border: none; "
                  "background: #2a2a2a; "
                  "width: 12px; "
                  "} "
                  "QScrollBar::handle:vertical { "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #404040, stop:1 #555555); "
                  "min-height: 20px; "
                  "border-radius: 6px; "
                  "} "
                  "QScrollBar::handle:vertical:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078d4, stop:1 #40a6ff); "
                  "}");
    
    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    
    // Add header label
    QLabel* headerLabel = new QLabel("Sample Browser", this);
    headerLabel->setStyleSheet("QLabel { color: #ffffff; font-weight: bold; font-size: 14px; padding: 8px; }");
    layout->addWidget(headerLabel);
    
    // **LIBRARY NAVIGATION DROPDOWN**
    QHBoxLayout* libraryLayout = new QHBoxLayout();
    QLabel* libraryLabel = new QLabel("ðŸ“š Quick Navigation:", this);
    libraryLabel->setStyleSheet("QLabel { color: #cccccc; font-size: 11px; }");
    
    librarySelector = new QComboBox(this);  // Store reference!
    librarySelector->setStyleSheet(
        "QComboBox { "
        "    background-color: #2a2a2a; "
        "    border: 1px solid #404040; "
        "    border-radius: 4px; "
        "    padding: 6px 12px; "
        "    color: #ffffff; "
        "    font-size: 11px; "
        "    min-width: 200px; "
        "} "
        "QComboBox:hover { "
        "    border-color: #0078d4; "
        "    background-color: #333333; "
        "}"
    );
    
    librarySelector->addItem("ðŸ  Home Directory", QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    librarySelector->addItem("ðŸŽµ Music Directory", QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    librarySelector->addItem("ðŸ’¾ Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    
    // Add saved libraries
    for (const QString& libraryPath : libraryPaths) {
        QString libraryName = QString("ðŸ“ %1").arg(QFileInfo(libraryPath).fileName());
        librarySelector->addItem(libraryName, libraryPath);
    }
    
    libraryLayout->addWidget(libraryLabel);
    libraryLayout->addWidget(librarySelector);
    libraryLayout->addStretch();
    layout->addLayout(libraryLayout);
    
    // Connect library selector for instant navigation
    connect(librarySelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            [this](int index) {
                QString selectedPath = librarySelector->itemData(index).toString();
                if (!selectedPath.isEmpty()) {
                    navigateToLibrary(selectedPath);
                }
            });
    
    // Search box
    searchBox = new QLineEdit(this);
    searchBox->setPlaceholderText("Search samples and libraries...");
    layout->addWidget(searchBox);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    addLibraryBtn = new QPushButton("ðŸ“ Add Library", this);
    refreshBtn = new QPushButton("ðŸ”„ Refresh", this);
    
    // **BATCH OPERATIONS BUTTONS**
    batchLoadBtn = new QPushButton("ðŸ“¦ Batch Load", this);
    smartFilterBtn = new QPushButton("ðŸ§  Smart Filter", this);
    selectAllBtn = new QPushButton("â˜‘ï¸ Select All Audio", this);
    
    // Style the new buttons
    QString batchButtonStyle = 
        "QPushButton { "
        "    background-color: #0078d4; "
        "    border: 1px solid #106ebe; "
        "    border-radius: 4px; "
        "    color: #ffffff; "
        "    padding: 8px 12px; "
        "    font-size: 11px; "
        "    font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "    background-color: #106ebe; "
        "    border-color: #005a9e; "
        "} "
        "QPushButton:pressed { "
        "    background-color: #005a9e; "
        "}";
    
    batchLoadBtn->setStyleSheet(batchButtonStyle);
    smartFilterBtn->setStyleSheet(batchButtonStyle);
    selectAllBtn->setStyleSheet(batchButtonStyle);
    
    buttonLayout->addWidget(addLibraryBtn);
    buttonLayout->addWidget(refreshBtn);
    buttonLayout->addWidget(selectAllBtn);
    buttonLayout->addWidget(smartFilterBtn);
    buttonLayout->addWidget(batchLoadBtn);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    // Create file system model and tree view
    fileSystemModel = new QFileSystemModel(this);
    fileSystemModel->setRootPath("");
    
    treeView = new QTreeView(this);
    treeView->setModel(fileSystemModel);
    treeView->setRootIndex(fileSystemModel->index(QStandardPaths::writableLocation(QStandardPaths::MusicLocation)));
    
    // Configure tree view
    treeView->hideColumn(1); // Size
    treeView->hideColumn(2); // Type  
    treeView->hideColumn(3); // Date Modified
    treeView->header()->setStretchLastSection(false);
    treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Enable drag and drop
    treeView->setDragEnabled(true);
    treeView->setDragDropMode(QAbstractItemView::DragOnly);
    treeView->setDefaultDropAction(Qt::CopyAction);
    
    // Install event filter to handle custom drag operations
    treeView->installEventFilter(this);
    
    layout->addWidget(treeView);
    
    // **BATCH OPERATIONS DISPLAY**
    selectionCountLabel = new QLabel("ðŸ“Š No files selected", this);
    selectionCountLabel->setStyleSheet("QLabel { color: #0078d4; font-size: 11px; font-weight: bold; padding: 4px; }");
    layout->addWidget(selectionCountLabel);
    
    // Batch results display (initially hidden)
    batchResultsDisplay = new QTextEdit(this);
    batchResultsDisplay->setMaximumHeight(80);
    batchResultsDisplay->setStyleSheet(
        "QTextEdit { "
        "    background-color: #2a2a2a; "
        "    border: 1px solid #404040; "
        "    border-radius: 4px; "
        "    color: #ffffff; "
        "    font-size: 10px; "
        "    font-family: 'Consolas', monospace; "
        "    padding: 6px; "
        "}"
    );
    batchResultsDisplay->setPlaceholderText("Batch operation results will appear here...");
    batchResultsDisplay->hide(); // Initially hidden
    layout->addWidget(batchResultsDisplay);
    
    // **LOADED SAMPLES VIEW**
    QLabel* loadedLabel = new QLabel("ðŸŽµ Loaded Samples", this);
    loadedLabel->setStyleSheet("QLabel { color: #0078d4; font-weight: bold; font-size: 12px; padding: 6px 4px; }");
    layout->addWidget(loadedLabel);
    
    loadedSamplesView = new QListWidget(this);
    loadedSamplesView->setMaximumHeight(120);
    loadedSamplesView->setStyleSheet(
        "QListWidget { "
        "    background-color: #1e1e1e; "
        "    border: 1px solid #404040; "
        "    border-radius: 4px; "
        "    color: #ffffff; "
        "    font-size: 11px; "
        "    selection-background-color: #0078d4; "
        "    alternate-background-color: #2a2a2a; "
        "} "
        "QListWidget::item { "
        "    padding: 4px; "
        "    border-bottom: 1px solid #333333; "
        "} "
        "QListWidget::item:hover { "
        "    background-color: #333333; "
        "}"
    );
    loadedSamplesView->setAlternatingRowColors(true);
    loadedSamplesView->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(loadedSamplesView);
    
    // Connect loaded samples actions
    connect(loadedSamplesView, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item) {
            QString filePath = item->data(Qt::UserRole).toString();
            currentPreviewFile = filePath;
            onPreviewSample();
        }
    });
    
    // Status label
    statusLabel = new QLabel("ðŸŽµ Ready - Double-click samples to load & preview, drag to move them around", this);
    statusLabel->setStyleSheet("QLabel { color: #888888; font-size: 11px; padding: 4px; }");
    layout->addWidget(statusLabel);
    
    // Setup context menu
    contextMenu = new QMenu(this);
    loadSampleAction = contextMenu->addAction("Load Sample");
    addToLibraryAction = contextMenu->addAction("Add to Library");
    contextMenu->addSeparator();
    previewAction = contextMenu->addAction("ðŸŽµ Preview Sample");
    stopPreviewAction = contextMenu->addAction("â¹ Stop Preview");
    stopPreviewAction->setEnabled(false);
    
    // Connect signals
    connect(treeView, &QTreeView::doubleClicked, this, &FileBrowser::onFileDoubleClicked);
    connect(treeView, &QTreeView::customContextMenuRequested, this, &FileBrowser::onContextMenuRequested);
    connect(addLibraryBtn, &QPushButton::clicked, this, &FileBrowser::onAddLibrary);
    connect(refreshBtn, &QPushButton::clicked, this, &FileBrowser::onRefreshLibraries);
    connect(selectAllBtn, &QPushButton::clicked, this, &FileBrowser::onSelectAllAudio);
    connect(smartFilterBtn, &QPushButton::clicked, this, &FileBrowser::onSmartFilter);
    connect(batchLoadBtn, &QPushButton::clicked, this, &FileBrowser::onLoadSelectedSamples);
    connect(previewAction, &QAction::triggered, this, &FileBrowser::onPreviewSample);
    connect(stopPreviewAction, &QAction::triggered, this, &FileBrowser::onStopPreview);
    connect(loadSampleAction, &QAction::triggered, [this]() {
        QModelIndex index = treeView->currentIndex();
        if (index.isValid()) {
            onFileDoubleClicked(index);
        }
    });
    
    setLayout(layout);
}

void FileBrowser::setupFileFilters() {
    // Define supported audio file extensions
    audioExtensions << "*.wav" << "*.mp3" << "*.flac" << "*.aiff" << "*.aif" 
                   << "*.ogg" << "*.m4a" << "*.wma" << "*.au" << "*.snd";
    
    // Set file filters to show audio files and directories
    QStringList filters;
    filters << audioExtensions;
    fileSystemModel->setNameFilters(filters);
    fileSystemModel->setNameFilterDisables(false);
}

void FileBrowser::setupCategoryFilter() {
    // Setup category keywords for smart filtering
    categoryKeywords[Drums] = {"kick", "snare", "hihat", "drum", "perc", "cymbal", "tom"};
    categoryKeywords[Bass] = {"bass", "sub", "808", "low", "bottom"};
    categoryKeywords[Synth] = {"synth", "lead", "pad", "pluck", "arp", "seq"};
    categoryKeywords[Vocal] = {"vocal", "voice", "sing", "chop", "phrase", "lyric"};
    categoryKeywords[FX] = {"fx", "effect", "sweep", "whoosh", "impact", "riser", "drop"};
    categoryKeywords[Instrument] = {"piano", "guitar", "string", "horn", "brass", "wind"};
    categoryKeywords[Loop] = {"loop", "full", "bar", "measure", "phrase"};
    categoryKeywords[OneShot] = {"shot", "hit", "stab", "one"};
    categoryKeywords[Ambient] = {"ambient", "pad", "texture", "atmo", "drone", "space"};
    categoryKeywords[Percussion] = {"perc", "conga", "bongo", "shake", "rattle", "ethnic"};
    
    // Setup genre keywords
    genreKeywords[Electronic] = {"house", "techno", "trance", "edm", "electro", "dubstep"};
    genreKeywords[HipHop] = {"hiphop", "rap", "trap", "boom", "bap", "urban"};
    genreKeywords[Rock] = {"rock", "metal", "punk", "grunge", "alternative"};
    genreKeywords[Pop] = {"pop", "commercial", "radio", "mainstream"};
    genreKeywords[Jazz] = {"jazz", "swing", "bebop", "fusion", "smooth"};
    genreKeywords[Classical] = {"classical", "orchestra", "symphony", "chamber", "baroque"};
    genreKeywords[Experimental] = {"experimental", "noise", "avant", "abstract", "weird"};
    genreKeywords[World] = {"world", "ethnic", "tribal", "folk", "traditional"};
    genreKeywords[Cinematic] = {"cinematic", "film", "movie", "soundtrack", "score", "epic"};
}

void FileBrowser::onFileDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    QString filePath = fileSystemModel->filePath(index);
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isFile() && isAudioFile(filePath)) {
        statusLabel->setText(QString("ðŸŽµ Loading: %1").arg(fileInfo.fileName()));
        
        // Add to loaded samples list
        if (!loadedSamples.contains(filePath)) {
            loadedSamples.append(filePath);
            
            // Add to UI list with nice formatting
            QString category = detectSampleCategory(filePath);
            QString categoryIcon = getCategoryIcon(category);
            QString displayText = QString("%1 %2").arg(categoryIcon).arg(fileInfo.fileName());
            
            QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, filePath); // Store full path
            item->setToolTip(QString("Path: %1\nCategory: %2\nSize: %3 KB")
                           .arg(filePath)
                           .arg(category)
                           .arg(fileInfo.size() / 1024));
            
            loadedSamplesView->addItem(item);
            loadedSamplesView->scrollToBottom();
        }
        
        // Start preview automatically on double-click
        currentPreviewFile = filePath;
        onPreviewSample();
        
        // Also emit sample selected for loading into DAW
        emit sampleSelected(filePath);
        
        statusLabel->setText(QString("âœ… Loaded & Previewing: %1").arg(fileInfo.fileName()));
    } else if (fileInfo.isDir()) {
        // Expand/collapse directory
        if (treeView->isExpanded(index)) {
            treeView->collapse(index);
        } else {
            treeView->expand(index);
        }
    }
}

void FileBrowser::onAddLibrary() {
    QString folderPath = QFileDialog::getExistingDirectory(
        this, 
        "ðŸŽµ Add Sample Library", 
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!folderPath.isEmpty()) {
        // Analyze library before adding
        analyzeAndAddLibrary(folderPath);
    }
}

void FileBrowser::analyzeAndAddLibrary(const QString& folderPath) {
    QFileInfo folderInfo(folderPath);
    QString libraryName = folderInfo.baseName();
    
    // Show analysis progress
    statusLabel->setText(QString("ðŸ” Analyzing library: %1...").arg(libraryName));
    QApplication::processEvents();
    
    // Scan for audio files and categorize
    QStringList allAudioFiles = scanForAudioFiles(folderPath);
    QMap<QString, int> categoryStats;
    
    for (const QString& filePath : allAudioFiles) {
        QString category = detectSampleCategory(filePath);
        categoryStats[category]++;
    }
    
    // Add library to saved paths
    addLibraryFolder(folderPath);
    
    // Generate analysis report
    QString analysisReport = QString("ðŸ“Š Library Analysis: %1\n").arg(libraryName);
    analysisReport += QString("ðŸ“ Total samples: %1\n\n").arg(allAudioFiles.count());
    
    // Category breakdown
    analysisReport += "ðŸŽ¯ Category Breakdown:\n";
    for (auto it = categoryStats.begin(); it != categoryStats.end(); ++it) {
        QString icon = getCategoryIcon(it.key());
        analysisReport += QString("%1 %2: %3 samples\n").arg(icon).arg(it.key()).arg(it.value());
    }
    
    // Display analysis in batch results
    if (batchResultsDisplay) {
        batchResultsDisplay->clear();
        batchResultsDisplay->append(analysisReport);
        batchResultsDisplay->append("\nâœ… Library successfully added to collection!");
    }
    
    // Update UI
    updateLibrarySelector();
    navigateToLibrary(folderPath);
    
    statusLabel->setText(QString("âœ… Added library: %1 (%2 samples)").arg(libraryName).arg(allAudioFiles.count()));
    emit libraryAdded(folderPath);
}

void FileBrowser::onRefreshLibraries() {
    statusLabel->setText("ðŸ”„ Refreshing libraries...");
    QApplication::processEvents();
    
    // Refresh file system model
    fileSystemModel->setRootPath("");
    
    // Re-analyze all libraries
    int totalSamples = 0;
    for (const QString& libraryPath : libraryPaths) {
        QStringList samples = scanForAudioFiles(libraryPath);
        totalSamples += samples.count();
    }
    
    statusLabel->setText(QString("âœ… Libraries refreshed (%1 total samples across %2 libraries)")
                         .arg(totalSamples).arg(libraryPaths.count()));
}

void FileBrowser::onContextMenuRequested(const QPoint& pos) {
    QModelIndex index = treeView->indexAt(pos);
    if (index.isValid()) {
        QString filePath = fileSystemModel->filePath(index);
        showFileContextMenu(filePath, treeView->mapToGlobal(pos));
    }
}

void FileBrowser::showFileContextMenu(const QString& filePath, const QPoint& globalPos) {
    QFileInfo fileInfo(filePath);
    
    loadSampleAction->setEnabled(fileInfo.isFile() && isAudioFile(filePath));
    addToLibraryAction->setEnabled(fileInfo.isDir());
    
    QAction* selectedAction = contextMenu->exec(globalPos);
    
    if (selectedAction == addToLibraryAction && fileInfo.isDir()) {
        addLibraryFolder(filePath);
    }
}

bool FileBrowser::isAudioFile(const QString& filePath) const {
    QFileInfo fileInfo(filePath);
    QString extension = "*." + fileInfo.suffix().toLower();
    return audioExtensions.contains(extension);
}

void FileBrowser::addLibraryFolder(const QString& folderPath) {
    if (!libraryPaths.contains(folderPath)) {
        libraryPaths.append(folderPath);
        
        // Save library paths immediately
        QSettings settings;
        settings.setValue("libraryPaths", libraryPaths);
        
        QString libraryName = QFileInfo(folderPath).fileName();
        statusLabel->setText(QString("âœ… Library added: %1").arg(libraryName));
        emit libraryAdded(folderPath);
        
        // Count audio files in the library
        QDir libraryDir(folderPath);
        QStringList audioFiles;
        for (const QString& ext : audioExtensions) {
            audioFiles += libraryDir.entryList({ext}, QDir::Files | QDir::NoSymLinks, QDir::Name);
        }
        
        // Show success with file count
        QMessageBox::information(this, "ðŸŽµ Library Added Successfully", 
                               QString("Sample library added:\nðŸ“ %1\n\nðŸŽµ Found %2 audio files\n\nThe browser will now navigate to this library.")
                               .arg(folderPath)
                               .arg(audioFiles.count()));
    } else {
        statusLabel->setText("âš ï¸ Library already exists in your collection");
        QMessageBox::information(this, "Library Already Added", 
                               QString("This library is already in your collection:\n%1").arg(folderPath));
    }
}

// ðŸŽµ DRAG & DROP IMPLEMENTATION
// Note: Mouse events now handled via eventFilter on treeView
/*
void FileBrowser::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void FileBrowser::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    if ((event->pos() - dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    // Get the item under cursor
    QModelIndex index = treeView->indexAt(treeView->mapFromParent(event->pos()));
    if (!index.isValid()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    QString filePath = fileSystemModel->filePath(index);
    if (isAudioFile(filePath)) {
        startDrag(filePath);
    }
    
    QWidget::mouseMoveEvent(event);
}
*/

void FileBrowser::startDrag(const QString& filePath) {
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    
    // Set comprehensive file data
    mimeData->setUrls({QUrl::fromLocalFile(filePath)});
    mimeData->setText(filePath);
    mimeData->setData("application/x-dawg-sample", filePath.toUtf8());
    
    // Add sample metadata
    QFileInfo fileInfo(filePath);
    QString metadata = QString("name:%1;size:%2;type:%3")
        .arg(fileInfo.baseName())
        .arg(fileInfo.size())
        .arg(fileInfo.suffix().toLower());
    mimeData->setData("application/x-dawg-sample-metadata", metadata.toUtf8());
    
    // Detect sample category for smart routing
    QString category = detectSampleCategory(filePath);
    mimeData->setData("application/x-dawg-sample-category", category.toUtf8());
    
    drag->setMimeData(mimeData);
    
    // Create enhanced drag pixmap with category icon
    QString fileName = fileInfo.baseName();
    QString categoryIcon = getCategoryIcon(category);
    
    QPixmap dragPixmap(140, 35);
    dragPixmap.fill(QColor(0, 120, 212, 180)); // Semi-transparent blue
    
    QPainter painter(&dragPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    
    // Draw category icon
    painter.setFont(QFont("Segoe UI Emoji", 10));
    painter.drawText(5, 20, categoryIcon);
    
    // Draw file name
    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    painter.drawText(20, 20, fileName.left(18) + (fileName.length() > 18 ? "..." : ""));
    
    // Draw file type
    painter.setFont(QFont("Segoe UI", 7));
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(20, 30, fileInfo.suffix().toUpper());
    
    painter.end();
    
    drag->setPixmap(dragPixmap);
    drag->setHotSpot(QPoint(70, 17));
    
    statusLabel->setText(QString("ðŸš€ Dragging: %1 (%2)").arg(fileInfo.fileName()).arg(category));
    emit sampleDragStarted(filePath);
    
    Qt::DropAction result = drag->exec(Qt::CopyAction | Qt::MoveAction);
    
    // Update status based on drop result
    if (result == Qt::IgnoreAction) {
        statusLabel->setText("Drag cancelled");
    } else {
        statusLabel->setText(QString("âœ… %1 ready to use!").arg(fileInfo.fileName()));
    }
}

// ðŸŽµ SAMPLE PREVIEW IMPLEMENTATION
void FileBrowser::onPreviewSample() {
    QModelIndex index = treeView->currentIndex();
    if (!index.isValid()) return;
    
    QString filePath = fileSystemModel->filePath(index);
    if (!isAudioFile(filePath)) return;
    
    // Stop current preview if playing
    if (wavPlayer && wavPlayer->isPlaying()) {
        wavPlayer->stop();
    }
    
    // Try to load and play new sample
    currentPreviewFile = filePath;
    QFileInfo fileInfo(filePath);
    
    if (wavPlayer && wavPlayer->loadFile(filePath)) {
        // WAV file loaded successfully
        wavPlayer->play();
        statusLabel->setText(QString("ðŸŽµ Playing: %1").arg(fileInfo.fileName()));
        previewAction->setEnabled(false);
        stopPreviewAction->setEnabled(true);
    } else {
        // Fallback for unsupported formats or parsing failures
        statusLabel->setText(QString("ðŸŽµ Preview: %1 (format not fully supported, showing info only)").arg(fileInfo.fileName()));
        
        // Show file information instead
        QString infoMessage = QString("ðŸ“ File: %1\n").arg(fileInfo.fileName());
        infoMessage += QString("ðŸ“Š Size: %1 KB\n").arg(fileInfo.size() / 1024);
        infoMessage += QString("ðŸŽ¯ Category: %1\n").arg(detectSampleCategory(filePath));
        infoMessage += QString("ðŸ“‚ Path: %1").arg(filePath);
        
        if (batchResultsDisplay) {
            batchResultsDisplay->show();
            batchResultsDisplay->clear();
            batchResultsDisplay->append("ðŸŽµ Sample Info Preview:");
            batchResultsDisplay->append(infoMessage);
        }
        
        previewAction->setEnabled(false);
        stopPreviewAction->setEnabled(true);
        
        // Auto-stop after 3 seconds for info display
        QTimer::singleShot(3000, this, &FileBrowser::onStopPreview);
    }
}

void FileBrowser::onStopPreview() {
    if (wavPlayer && wavPlayer->isPlaying()) {
        wavPlayer->stop();
    }
    
    // Hide batch results if it was showing preview info
    if (batchResultsDisplay && batchResultsDisplay->isVisible()) {
        QString text = batchResultsDisplay->toPlainText();
        if (text.contains("Sample Info Preview")) {
            batchResultsDisplay->hide();
        }
    }
    
    // Update UI
    statusLabel->setText("Preview stopped");
    previewAction->setEnabled(true);
    stopPreviewAction->setEnabled(false);
    currentPreviewFile.clear();
}

// ðŸŽ›ï¸ CATEGORY FILTERING IMPLEMENTATION
QString FileBrowser::getCategoryKeywords(SampleCategory category) const {
    return categoryKeywords.value(category).join("|");
}

QString FileBrowser::getGenreKeywords(SampleGenre genre) const {
    return genreKeywords.value(genre).join("|");
}

bool FileBrowser::matchesCurrentFilter(const QString& filePath) const {
    if (currentCategory == All && currentGenre == AllGenres) {
        return true; // No filters active
    }
    
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.baseName().toLower();
    QString folderPath = fileInfo.path().toLower();
    
    // Check category match
    bool categoryMatch = (currentCategory == All);
    if (!categoryMatch && categoryKeywords.contains(currentCategory)) {
        for (const QString& keyword : categoryKeywords[currentCategory]) {
            if (fileName.contains(keyword.toLower()) || folderPath.contains(keyword.toLower())) {
                categoryMatch = true;
                break;
            }
        }
    }
    
    // Check genre match
    bool genreMatch = (currentGenre == AllGenres);
    if (!genreMatch && genreKeywords.contains(currentGenre)) {
        for (const QString& keyword : genreKeywords[currentGenre]) {
            if (fileName.contains(keyword.toLower()) || folderPath.contains(keyword.toLower())) {
                genreMatch = true;
                break;
            }
        }
    }
    
    return categoryMatch && genreMatch;
}

void FileBrowser::onCategoryChanged(int index) {
    // Placeholder for future category filtering
    statusLabel->setText("Category filtering coming soon!");
}

void FileBrowser::onGenreChanged(int index) {
    // Placeholder for future genre filtering
    statusLabel->setText("Genre filtering coming soon!");
}

void FileBrowser::onClearFilters() {
    currentCategory = All;
    currentGenre = AllGenres;
    statusLabel->setText("All filters cleared - showing all samples");
}

// ðŸ“š LIBRARY NAVIGATION HELPERS
void FileBrowser::updateLibrarySelector() {
    if (!librarySelector) return;
    
    // Clear existing items except default ones
    librarySelector->clear();
    
    // Add default locations
    librarySelector->addItem("ðŸ  Home Directory", QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    librarySelector->addItem("ðŸŽµ Music Directory", QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    librarySelector->addItem("ðŸ’¾ Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    
    // Add separator
    librarySelector->insertSeparator(librarySelector->count());
    
    // Add saved libraries
    for (const QString& libraryPath : libraryPaths) {
        QString libraryName = QString("ðŸ“ %1").arg(QFileInfo(libraryPath).fileName());
        librarySelector->addItem(libraryName, libraryPath);
    }
    
    statusLabel->setText(QString("ðŸ“š Library selector updated - %1 libraries available").arg(libraryPaths.count()));
}

void FileBrowser::navigateToLibrary(const QString& libraryPath) {
    QModelIndex libraryIndex = fileSystemModel->index(libraryPath);
    if (libraryIndex.isValid()) {
        treeView->setRootIndex(libraryIndex);
        
        // Find and select the library in the selector
        if (librarySelector) {
            for (int i = 0; i < librarySelector->count(); ++i) {
                if (librarySelector->itemData(i).toString() == libraryPath) {
                    librarySelector->setCurrentIndex(i);
                    break;
                }
            }
        }
        
        QString libraryName = QFileInfo(libraryPath).fileName();
        statusLabel->setText(QString("ðŸ“ Browsing: %1").arg(libraryName));
        
        // Expand first level to show content
        for (int i = 0; i < fileSystemModel->rowCount(libraryIndex); ++i) {
            QModelIndex childIndex = fileSystemModel->index(i, 0, libraryIndex);
            if (fileSystemModel->isDir(childIndex)) {
                treeView->expand(childIndex);
            }
        }
    } else {
        statusLabel->setText(QString("âš ï¸ Could not access library: %1").arg(libraryPath));
    }
}

// ðŸ“¦ BATCH OPERATIONS IMPLEMENTATION
void FileBrowser::onSelectAllAudio() {
    QModelIndex rootIndex = treeView->rootIndex();
    selectedFiles = scanForAudioFiles(fileSystemModel->filePath(rootIndex));
    
    updateSelectionDisplay();
    
    if (!selectedFiles.isEmpty()) {
        batchResultsDisplay->show();
        batchResultsDisplay->setPlainText(QString("ðŸ“¦ Selected %1 audio files:\n%2")
                                         .arg(selectedFiles.count())
                                         .arg(selectedFiles.join("\n")));
        statusLabel->setText(QString("âœ… Selected %1 audio files for batch loading").arg(selectedFiles.count()));
    } else {
        statusLabel->setText("âš ï¸ No audio files found in current directory");
    }
}

void FileBrowser::onSmartFilter() {
    // Create smart filter menu
    QMenu smartMenu(this);
    smartMenu.setStyleSheet(
        "QMenu { "
        "    background-color: #2a2a2a; "
        "    border: 1px solid #404040; "
        "    color: #ffffff; "
        "    padding: 4px; "
        "} "
        "QMenu::item { "
        "    padding: 8px 16px; "
        "    border-radius: 4px; "
        "} "
        "QMenu::item:selected { "
        "    background-color: #0078d4; "
        "}"
    );
    
    smartMenu.addAction("ðŸ¥ Drums & Percussion", [this]() { applySmartFilter("drums"); });
    smartMenu.addAction("ðŸŽ¸ Bass Sounds", [this]() { applySmartFilter("bass"); });
    smartMenu.addAction("ðŸŽ¹ Synth & Leads", [this]() { applySmartFilter("synth"); });
    smartMenu.addAction("ðŸŽ¤ Vocals", [this]() { applySmartFilter("vocal"); });
    smartMenu.addAction("ðŸ”„ Loops", [this]() { applySmartFilter("loop"); });
    smartMenu.addAction("ðŸ’¥ One-Shots", [this]() { applySmartFilter("shot"); });
    smartMenu.addAction("ðŸŒŠ Ambient & Pads", [this]() { applySmartFilter("ambient"); });
    smartMenu.addAction("âœ¨ FX & Sweeps", [this]() { applySmartFilter("fx"); });
    
    smartMenu.exec(smartFilterBtn->mapToGlobal(QPoint(0, smartFilterBtn->height())));
}

void FileBrowser::onLoadSelectedSamples() {
    if (selectedFiles.isEmpty()) {
        statusLabel->setText("âš ï¸ No samples selected for batch loading");
        return;
    }
    
    batchResultsDisplay->show();
    batchResultsDisplay->clear();
    batchResultsDisplay->append("ðŸš€ Starting batch load operation...\n");
    
    int loadedCount = 0;
    for (const QString& filePath : selectedFiles) {
        QFileInfo fileInfo(filePath);
        batchResultsDisplay->append(QString("âœ… Loading: %1").arg(fileInfo.fileName()));
        
        // Emit signal for each sample
        emit sampleSelected(filePath);
        loadedCount++;
        
        // Process events to update UI
        QApplication::processEvents();
    }
    
    batchResultsDisplay->append(QString("\nðŸŽ‰ Batch load complete! Loaded %1 samples.").arg(loadedCount));
    statusLabel->setText(QString("ðŸŽ‰ Batch loaded %1 samples successfully!").arg(loadedCount));
    
    // Emit batch signal
    emit batchSamplesSelected(selectedFiles);
    
    // Clear selection after loading
    selectedFiles.clear();
    updateSelectionDisplay();
}

QStringList FileBrowser::getSelectedAudioFiles() const {
    return selectedFiles;
}

QStringList FileBrowser::scanForAudioFiles(const QString& directory, const QString& filterType) const {
    QStringList audioFiles;
    QDir dir(directory);
    
    // Recursively scan for audio files
    QDirIterator iterator(directory, audioExtensions, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    
    while (iterator.hasNext()) {
        QString filePath = iterator.next();
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.baseName().toLower();
        
        // Apply filter if specified
        if (!filterType.isEmpty()) {
            if (categoryKeywords.contains(currentCategory)) {
                bool matchesFilter = false;
                for (const QString& keyword : categoryKeywords[currentCategory]) {
                    if (fileName.contains(keyword.toLower())) {
                        matchesFilter = true;
                        break;
                    }
                }
                if (!matchesFilter && filterType != "all") continue;
            }
        }
        
        audioFiles.append(filePath);
    }
    
    return audioFiles;
}

void FileBrowser::applySmartFilter(const QString& filterType) {
    QModelIndex rootIndex = treeView->rootIndex();
    QString currentPath = fileSystemModel->filePath(rootIndex);
    
    batchResultsDisplay->show();
    batchResultsDisplay->clear();
    batchResultsDisplay->append(QString("ðŸ§  Applying smart filter: %1\n").arg(filterType));
    
    QStringList filteredFiles;
    QStringList allAudioFiles = scanForAudioFiles(currentPath);
    
    for (const QString& filePath : allAudioFiles) {
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.baseName().toLower();
        QString folderName = fileInfo.path().toLower();
        
        bool matches = false;
        
        // Smart filtering based on keywords
        if (filterType == "drums") {
            matches = fileName.contains("kick") || fileName.contains("snare") || fileName.contains("hihat") || 
                     fileName.contains("drum") || fileName.contains("perc") || folderName.contains("drum");
        } else if (filterType == "bass") {
            matches = fileName.contains("bass") || fileName.contains("sub") || fileName.contains("808") || 
                     fileName.contains("low") || folderName.contains("bass");
        } else if (filterType == "synth") {
            matches = fileName.contains("synth") || fileName.contains("lead") || fileName.contains("pad") || 
                     fileName.contains("pluck") || folderName.contains("synth");
        } else if (filterType == "vocal") {
            matches = fileName.contains("vocal") || fileName.contains("voice") || fileName.contains("sing") || 
                     fileName.contains("chop") || folderName.contains("vocal");
        } else if (filterType == "loop") {
            matches = fileName.contains("loop") || fileName.contains("full") || folderName.contains("loop");
        } else if (filterType == "shot") {
            matches = fileName.contains("shot") || fileName.contains("hit") || fileName.contains("stab") || 
                     fileName.contains("one") || folderName.contains("shot");
        } else if (filterType == "ambient") {
            matches = fileName.contains("ambient") || fileName.contains("pad") || fileName.contains("texture") || 
                     fileName.contains("atmo") || folderName.contains("ambient");
        } else if (filterType == "fx") {
            matches = fileName.contains("fx") || fileName.contains("effect") || fileName.contains("sweep") || 
                     fileName.contains("whoosh") || folderName.contains("fx");
        }
        
        if (matches) {
            filteredFiles.append(filePath);
            batchResultsDisplay->append(QString("âœ… %1").arg(fileInfo.fileName()));
        }
    }
    
    selectedFiles = filteredFiles;
    updateSelectionDisplay();
    
    batchResultsDisplay->append(QString("\nðŸŽ¯ Smart filter found %1 matching files").arg(filteredFiles.count()));
    statusLabel->setText(QString("ðŸ§  Smart filter applied: %1 (%2 files found)").arg(filterType).arg(filteredFiles.count()));
    
    emit smartFilterApplied(filterType, filteredFiles);
}

void FileBrowser::updateSelectionDisplay() {
    if (selectedFiles.isEmpty()) {
        selectionCountLabel->setText("ðŸ“Š No files selected");
        batchLoadBtn->setEnabled(false);
    } else {
        selectionCountLabel->setText(QString("ðŸ“Š %1 files selected").arg(selectedFiles.count()));
        batchLoadBtn->setEnabled(true);
    }
}

// ðŸŽ¯ MISSING METHOD IMPLEMENTATIONS
void FileBrowser::onBatchLoad() {
    // Redirect to onLoadSelectedSamples for now
    onLoadSelectedSamples();
}

bool FileBrowser::eventFilter(QObject* obj, QEvent* event) {
    if (obj == treeView) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragStartPos = mouseEvent->pos();
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) && 
                (mouseEvent->pos() - dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                
                // Get the item under cursor
                QModelIndex index = treeView->indexAt(mouseEvent->pos());
                if (index.isValid()) {
                    QString filePath = fileSystemModel->filePath(index);
                    if (isAudioFile(filePath)) {
                        startDrag(filePath);
                        return true; // Consume the event
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void FileBrowser::dragEnterEvent(QDragEnterEvent* event) {
    // Check if the drag contains audio file data
    if (event->mimeData()->hasUrls() || 
        event->mimeData()->hasFormat("application/x-dawg-sample")) {
        event->acceptProposedAction();
        statusLabel->setText("ðŸ’« Drop here to load sample!");
    } else {
        event->ignore();
    }
}

void FileBrowser::dragMoveEvent(QDragMoveEvent* event) {
    // Accept drag move events for audio files
    if (event->mimeData()->hasUrls() || 
        event->mimeData()->hasFormat("application/x-dawg-sample")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileBrowser::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    QString filePath;
    
    // Extract file path from different MIME data formats
    if (mimeData->hasFormat("application/x-dawg-sample")) {
        filePath = QString::fromUtf8(mimeData->data("application/x-dawg-sample"));
    } else if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            filePath = urls.first().toLocalFile();
        }
    } else if (mimeData->hasText()) {
        filePath = mimeData->text();
    }
    
    if (!filePath.isEmpty() && isAudioFile(filePath)) {
        // Load the sample into our loaded samples list
        if (!loadedSamples.contains(filePath)) {
            loadedSamples.append(filePath);
            
            // Add to loaded samples view with category icon
            QFileInfo fileInfo(filePath);
            QString category = detectSampleCategory(filePath);
            QString categoryIcon = getCategoryIcon(category);
            QString displayText = QString("%1 %2 (%3)")
                .arg(categoryIcon)
                .arg(fileInfo.baseName())
                .arg(category);
                
            QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setToolTip(QString("File: %1\nCategory: %2\nSize: %3 bytes")
                .arg(filePath)
                .arg(category)
                .arg(fileInfo.size()));
            loadedSamplesView->addItem(item);
        }
        
        statusLabel->setText(QString("âœ… Loaded: %1").arg(QFileInfo(filePath).fileName()));
        emit sampleDroppedOnTrack(filePath, 0); // Default to track 0
        event->acceptProposedAction();
    } else {
        statusLabel->setText("âŒ Only audio files can be dropped here");
        event->ignore();
    }
}

QString FileBrowser::detectSampleCategory(const QString& filePath) const {
    QString fileName = QFileInfo(filePath).baseName().toLower();
    
    // Drum detection
    if (fileName.contains("kick") || fileName.contains("drum") || fileName.contains("snare") || 
        fileName.contains("hihat") || fileName.contains("cymbal") || fileName.contains("tom") ||
        fileName.contains("perc") || fileName.contains("beat")) {
        return "Drums";
    }
    
    // Bass detection
    if (fileName.contains("bass") || fileName.contains("sub") || fileName.contains("808") || 
        fileName.contains("low") || fileName.contains("bottom")) {
        return "Bass";
    }
    
    // Vocal detection
    if (fileName.contains("vocal") || fileName.contains("voice") || fileName.contains("choir") || 
        fileName.contains("singer") || fileName.contains("vox") || fileName.contains("ah") ||
        fileName.contains("oh") || fileName.contains("talk")) {
        return "Vocal";
    }
    
    // Synth detection
    if (fileName.contains("synth") || fileName.contains("lead") || fileName.contains("pad") || 
        fileName.contains("pluck") || fileName.contains("arp") || fileName.contains("stab")) {
        return "Synth";
    }
    
    // Loop detection
    if (fileName.contains("loop") || fileName.contains("bar") || fileName.contains("phrase") || 
        fileName.contains("sequence") || fileName.contains("pattern")) {
        return "Loop";
    }
    
    // One-shot detection
    if (fileName.contains("shot") || fileName.contains("hit") || fileName.contains("stab") || 
        fileName.contains("impact") || fileName.contains("crash")) {
        return "OneShot";
    }
    
    // FX detection
    if (fileName.contains("fx") || fileName.contains("effect") || fileName.contains("sweep") || 
        fileName.contains("rise") || fileName.contains("drop") || fileName.contains("whoosh") ||
        fileName.contains("noise") || fileName.contains("scratch")) {
        return "FX";
    }
    
    // Ambient detection
    if (fileName.contains("ambient") || fileName.contains("atmo") || fileName.contains("texture") || 
        fileName.contains("drone") || fileName.contains("space") || fileName.contains("wind")) {
        return "Ambient";
    }
    
    return "Instrument"; // Default category
}

QString FileBrowser::getCategoryIcon(const QString& category) const {
    if (category == "Drums") return "ðŸ¥";
    if (category == "Bass") return "ðŸŽ¸";
    if (category == "Vocal") return "ðŸŽ¤";
    if (category == "Synth") return "ðŸŽ¹";
    if (category == "Loop") return "ðŸ”„";
    if (category == "OneShot") return "ðŸ’¥";
    if (category == "FX") return "âœ¨";
    if (category == "Ambient") return "ðŸŒŠ";
    if (category == "Instrument") return "ðŸŽµ";
    return "ðŸ“"; // Default icon
}

