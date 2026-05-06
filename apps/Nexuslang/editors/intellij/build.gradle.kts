import org.jetbrains.intellij.platform.gradle.TestFrameworkType

plugins {
    id("java")
    id("org.jetbrains.kotlin.jvm") version "1.9.25"
    id("org.jetbrains.intellij.platform") version "2.1.0"
}

group = "com.nlpl.intellij"
version = "0.1.0"

repositories {
    mavenCentral()
    intellijPlatform {
        defaultRepositories()
    }
}

dependencies {
    intellijPlatform {
        intellijIdeaCommunity("2024.1")
        bundledPlugin("com.intellij.platform.lsp")
        testFramework(TestFrameworkType.Platform)
    }
    testImplementation("junit:junit:4.13.2")
}

intellijPlatform {
    pluginConfiguration {
        id = "com.nlpl.intellij"
        name = "NLPL Language Support"
        version = project.version.toString()

        description = """
            Native support for the NexusLang (NexusLang) in IntelliJ-based IDEs.

            Features:
            - Syntax highlighting for .nlpl files
            - LSP integration with the NexusLang language server (nlpl lsp)
            - File type recognition and icons
            - Code completion via LSP
            - Go to definition, find references
            - Hover documentation
            - Inline diagnostics
            - Build, test, run via run configurations
        """.trimIndent()

        ideaVersion {
            sinceBuild = "241"
        }

        vendor {
            name = "The No-Hands Company"
            url = "https://github.com/The-No-hands-Company/NLPL"
        }
    }

    signing {
        certificateChain = providers.environmentVariable("CERTIFICATE_CHAIN")
        privateKey = providers.environmentVariable("PRIVATE_KEY")
        password = providers.environmentVariable("PRIVATE_KEY_PASSWORD")
    }

    publishing {
        token = providers.environmentVariable("PUBLISH_TOKEN")
    }
}

kotlin {
    jvmToolchain(17)
}
