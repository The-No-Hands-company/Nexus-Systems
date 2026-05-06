# NexusLang IntelliJ Plugin

Provides native NexusLang language support in all IntelliJ-based IDEs including IntelliJ IDEA,
PyCharm, CLion, WebStorm, and Rider.

## Features

- Syntax highlighting for `.nlpl` files
- LSP integration (code completion, go-to-definition, find references, hover docs)
- Inline diagnostics (errors, warnings)
- Signature help for function calls
- Custom file type icon
- Colors & Fonts customization under Settings > Editor > Color Scheme > NexusLang

## Requirements

- IntelliJ-based IDE 2024.1 or newer
- NexusLang installed: `pip install nlpl` or built from source

The LSP server (`nlpl lsp`) starts automatically when you open a `.nlpl` file.

## Building from Source

```bash
cd editors/intellij

# Build the plugin
./gradlew buildPlugin

# Run an IDE sandbox instance with the plugin installed
./gradlew runIde

# Run tests
./gradlew test

# Verify before publishing
./gradlew verifyPlugin
```

The distributable `.zip` will be in `build/distributions/`.

## Installation

### From JetBrains Marketplace (when published)
1. Go to **Settings | Plugins | Marketplace**
2. Search for "NexusLang"
3. Click Install

### From local build
1. Build the plugin: `./gradlew buildPlugin`
2. Go to **Settings | Plugins**
3. Click the gear icon, select **Install Plugin from Disk**
4. Choose `build/distributions/nlpl-intellij-*.zip`

## Configuration

By default the plugin detects the NexusLang LSP server automatically:

1. Checks the `NLPL_LSP_COMMAND` environment variable
2. Looks for `nlpl lsp` on `PATH`
3. Falls back to `python3 -m nexuslang.lsp`

To override, set the environment variable before launching IntelliJ:

```bash
export NLPL_LSP_COMMAND="python3 /path/to/nlpl/src/main.py lsp"
```

## Project Structure

```
editors/intellij/
    build.gradle.kts                         Gradle build config
    settings.gradle.kts                      Gradle settings
    src/main/kotlin/com/nlpl/intellij/
        NlplLanguage.kt                      Language descriptor
        NlplFileType.kt                      File type (.nxl)
        NlplIcons.kt                         File icon
        NlplTokenTypes.kt                    Lexer token types
        NlplHighlightingLexer.kt             Regex-based highlighting lexer
        NlplSyntaxHighlighter.kt             Token -> color mapping
        NlplSyntaxHighlighterFactory.kt      Factory for syntax highlighter
        NlplColorSettingsPage.kt             Color settings page
        NlplLspServerSupportProvider.kt      LSP server activation + descriptor
    src/main/resources/
        META-INF/plugin.xml                  Plugin manifest
        icons/nlpl.svg                       File type icon
```
