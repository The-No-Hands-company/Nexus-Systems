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
 * File: PluginBrowser.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "PluginBrowser.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

PluginBrowser::PluginBrowser(QWidget* parent)
    : QWidget(parent)
{
    // Apply professional dark theme styling
    setStyleSheet("PluginBrowser { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a2a2a, stop:1 #1a1a1a); "
                  "border: 1px solid #404040; "
                  "} "
                  "QTreeView, QListView, QListWidget { "
                  "background-color: #1a1a1a; "
                  "color: #ffffff; "
                  "border: 1px solid #404040; "
                  "selection-background-color: #0078d4; "
                  "alternate-background-color: #2a2a2a; "
                  "} "
                  "QTreeView::item:hover, QListView::item:hover, QListWidget::item:hover { "
                  "background-color: #404040; "
                  "} "
                  "QTreeView::item:selected, QListView::item:selected, QListWidget::item:selected { "
                  "background-color: #0078d4; "
                  "color: #ffffff; "
                  "} "
                  "QGroupBox { "
                  "color: #ffffff; "
                  "border: 2px solid #404040; "
                  "border-radius: 8px; "
                  "margin-top: 1ex; "
                  "font-weight: bold; "
                  "} "
                  "QGroupBox::title { "
                  "subcontrol-origin: margin; "
                  "left: 10px; "
                  "padding: 0 5px 0 5px; "
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
                  "padding: 4px; "
                  "} "
                  "QLineEdit:focus { "
                  "border: 1px solid #0078d4; "
                  "}");
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);
    
    // Header
    QLabel* headerLabel = new QLabel("Plugin Browser", this);
    headerLabel->setStyleSheet("QLabel { color: #ffffff; font-weight: bold; font-size: 14px; padding: 8px; }");
    mainLayout->addWidget(headerLabel);
    
    // Search box
    searchBox = new QLineEdit(this);
    searchBox->setPlaceholderText("Search plugins...");
    mainLayout->addWidget(searchBox);
    
    // Plugin categories
    QGroupBox* categoriesGroup = new QGroupBox("Categories", this);
    QVBoxLayout* catLayout = new QVBoxLayout(categoriesGroup);
    
    categoryList = new QListWidget(this);
    categoryList->addItem("Instruments");
    categoryList->addItem("Effects");
    categoryList->addItem("Dynamics");
    categoryList->addItem("EQ");
    categoryList->addItem("Reverb");
    categoryList->addItem("Delay");
    categoryList->addItem("Modulation");
    categoryList->addItem("Distortion");
    categoryList->setMaximumHeight(120);
    catLayout->addWidget(categoryList);
    
    mainLayout->addWidget(categoriesGroup);
    
    // Plugin list
    QGroupBox* pluginsGroup = new QGroupBox("Available Plugins", this);
    QVBoxLayout* pluginLayout = new QVBoxLayout(pluginsGroup);
    
    pluginList = new QListWidget(this);
    // Add some sample plugins
    pluginList->addItem("DAWG Synthesizer");
    pluginList->addItem("DAWG Reverb");
    pluginList->addItem("DAWG Compressor");
    pluginList->addItem("DAWG EQ");
    pluginList->addItem("DAWG Delay");
    pluginList->addItem("DAWG Distortion");
    pluginLayout->addWidget(pluginList);
    
    // Plugin controls
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    QPushButton* loadBtn = new QPushButton("Load Plugin", this);
    QPushButton* scanBtn = new QPushButton("Scan for Plugins", this);
    controlsLayout->addWidget(loadBtn);
    controlsLayout->addWidget(scanBtn);
    pluginLayout->addLayout(controlsLayout);
    
    mainLayout->addWidget(pluginsGroup);
    
    setLayout(mainLayout);
}

PluginBrowser::~PluginBrowser() {}

