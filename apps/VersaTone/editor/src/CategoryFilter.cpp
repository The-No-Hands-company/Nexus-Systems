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
 * File: CategoryFilter.cpp
 * Purpose: Audio filter effect processing
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "CategoryFilter.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>

CategoryFilter::CategoryFilter(QWidget* parent)
    : QWidget(parent)
    , categoryCombo(nullptr)
    , genreCombo(nullptr)
    , clearBtn(nullptr)
    , activeFiltersLabel(nullptr)
    , currentCategory(All)
    , currentGenre(AllGenres)
{
    setupUI();
    setupCategories();
}

void CategoryFilter::setupUI() {
    // Professional dark theme styling
    setStyleSheet(
        "CategoryFilter { "
        "    background-color: #1a1a1a; "
        "    border: 1px solid #404040; "
        "    border-radius: 6px; "
        "    padding: 8px; "
        "} "
        "QComboBox { "
        "    background-color: #2a2a2a; "
        "    border: 1px solid #404040; "
        "    border-radius: 4px; "
        "    padding: 6px 12px; "
        "    color: #ffffff; "
        "    font-size: 11px; "
        "    min-width: 120px; "
        "} "
        "QComboBox:hover { "
        "    border-color: #0078d4; "
        "    background-color: #333333; "
        "} "
        "QComboBox::drop-down { "
        "    border: none; "
        "    width: 20px; "
        "} "
        "QComboBox::down-arrow { "
        "    image: none; "
        "    border: 2px solid #888888; "
        "    border-width: 0 2px 2px 0; "
        "    width: 6px; "
        "    height: 6px; "
        "    transform: rotate(45deg); "
        "} "
        "QPushButton { "
        "    background-color: #404040; "
        "    border: 1px solid #606060; "
        "    border-radius: 4px; "
        "    color: #ffffff; "
        "    padding: 6px 12px; "
        "    font-size: 11px; "
        "} "
        "QPushButton:hover { "
        "    background-color: #505050; "
        "    border-color: #0078d4; "
        "} "
        "QLabel { "
        "    color: #cccccc; "
        "    font-size: 11px; "
        "    font-weight: bold; "
        "}"
    );
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // Header
    QLabel* headerLabel = new QLabel("ðŸŽ›ï¸ Sample Filters", this);
    headerLabel->setStyleSheet("QLabel { color: #0078d4; font-size: 12px; font-weight: bold; }");
    mainLayout->addWidget(headerLabel);
    
    // Filter controls
    QHBoxLayout* filtersLayout = new QHBoxLayout();
    filtersLayout->setSpacing(12);
    
    // Category filter
    QLabel* categoryLabel = new QLabel("Category:", this);
    categoryCombo = new QComboBox(this);
    filtersLayout->addWidget(categoryLabel);
    filtersLayout->addWidget(categoryCombo);
    
    // Genre filter
    QLabel* genreLabel = new QLabel("Genre:", this);
    genreCombo = new QComboBox(this);
    filtersLayout->addWidget(genreLabel);
    filtersLayout->addWidget(genreCombo);
    
    // Clear button
    clearBtn = new QPushButton("Clear All", this);
    filtersLayout->addWidget(clearBtn);
    
    filtersLayout->addStretch();
    mainLayout->addLayout(filtersLayout);
    
    // Active filters display
    activeFiltersLabel = new QLabel("No filters active", this);
    activeFiltersLabel->setStyleSheet("QLabel { color: #888888; font-size: 10px; }");
    mainLayout->addWidget(activeFiltersLabel);
    
    // Connect signals
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CategoryFilter::onCategoryChanged);
    connect(genreCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CategoryFilter::onGenreChanged);
    connect(clearBtn, &QPushButton::clicked, this, &CategoryFilter::onClearFilters);
}

void CategoryFilter::setupCategories() {
    // Populate category combo
    categoryCombo->addItem("All Categories", All);
    categoryCombo->addItem("ðŸ¥ Drums", Drums);
    categoryCombo->addItem("ðŸŽ¸ Bass", Bass);
    categoryCombo->addItem("ðŸŽ¹ Synth", Synth);
    categoryCombo->addItem("ðŸŽ¤ Vocal", Vocal);
    categoryCombo->addItem("âœ¨ FX", FX);
    categoryCombo->addItem("ðŸŽº Instrument", Instrument);
    categoryCombo->addItem("ðŸ”„ Loop", Loop);
    categoryCombo->addItem("ðŸ’¥ One-Shot", OneShot);
    categoryCombo->addItem("ðŸŒŠ Ambient", Ambient);
    categoryCombo->addItem("ðŸ¥ Percussion", Percussion);
    
    // Populate genre combo
    genreCombo->addItem("All Genres", AllGenres);
    genreCombo->addItem("ðŸŽ›ï¸ Electronic", Electronic);
    genreCombo->addItem("ðŸŽ¤ Hip-Hop", HipHop);
    genreCombo->addItem("ðŸŽ¸ Rock", Rock);
    genreCombo->addItem("ðŸŽµ Pop", Pop);
    genreCombo->addItem("ðŸŽ· Jazz", Jazz);
    genreCombo->addItem("ðŸŽ¼ Classical", Classical);
    genreCombo->addItem("ðŸ§ª Experimental", Experimental);
    genreCombo->addItem("ðŸŒ World", World);
    genreCombo->addItem("ðŸŽ¬ Cinematic", Cinematic);
    
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

QString CategoryFilter::getCategoryKeywords(SampleCategory category) const {
    return categoryKeywords.value(category).join("|");
}

QString CategoryFilter::getGenreKeywords(SampleGenre genre) const {
    return genreKeywords.value(genre).join("|");
}

QString CategoryFilter::getCustomFilter() const {
    QStringList filters;
    
    if (currentCategory != All) {
        filters << getCategoryKeywords(currentCategory);
    }
    
    if (currentGenre != AllGenres) {
        filters << getGenreKeywords(currentGenre);
    }
    
    return filters.join("|");
}

bool CategoryFilter::matchesFilter(const QString& filePath) const {
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

void CategoryFilter::onCategoryChanged(int index) {
    currentCategory = static_cast<SampleCategory>(categoryCombo->itemData(index).toInt());
    
    // Update active filters display
    QStringList activeFilters;
    if (currentCategory != All) {
        activeFilters << categoryCombo->currentText();
    }
    if (currentGenre != AllGenres) {
        activeFilters << genreCombo->currentText();
    }
    
    activeFiltersLabel->setText(activeFilters.isEmpty() ? "No filters active" : 
                               QString("Active: %1").arg(activeFilters.join(", ")));
    
    emit filterChanged(currentCategory, currentGenre, getCustomFilter());
}

void CategoryFilter::onGenreChanged(int index) {
    currentGenre = static_cast<SampleGenre>(genreCombo->itemData(index).toInt());
    
    // Update active filters display
    QStringList activeFilters;
    if (currentCategory != All) {
        activeFilters << categoryCombo->currentText();
    }
    if (currentGenre != AllGenres) {
        activeFilters << genreCombo->currentText();
    }
    
    activeFiltersLabel->setText(activeFilters.isEmpty() ? "No filters active" : 
                               QString("Active: %1").arg(activeFilters.join(", ")));
    
    emit filterChanged(currentCategory, currentGenre, getCustomFilter());
}

void CategoryFilter::onClearFilters() {
    categoryCombo->setCurrentIndex(0); // All Categories
    genreCombo->setCurrentIndex(0);    // All Genres
    currentCategory = All;
    currentGenre = AllGenres;
    
    activeFiltersLabel->setText("No filters active");
    emit clearAllFilters();
}

