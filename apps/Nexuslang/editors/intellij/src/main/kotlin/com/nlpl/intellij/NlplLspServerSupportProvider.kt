package com.nlpl.intellij

import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.platform.lsp.api.LspServerSupportProvider
import com.intellij.platform.lsp.api.LspServerManager
import com.intellij.platform.lsp.api.ProjectWideLspServerDescriptor
import java.io.File

/**
 * Activates the NexusLang LSP server for all .nlpl files in the project.
 *
 * The LSP server is started with:
 *   python3 -m nexuslang.lsp
 * or the `nlpl lsp` CLI command if available on PATH.
 *
 * Server path resolution order:
 *   1. NLPL_LSP_COMMAND environment variable
 *   2. `nlpl lsp` on PATH
 *   3. `python3 -m nexuslang.lsp`
 */
class NlplLspServerSupportProvider : LspServerSupportProvider {

    override fun fileOpened(
        project: Project,
        file: VirtualFile,
        serverStarter: LspServerSupportProvider.LspServerStarter
    ) {
        if (file.extension.equals("nlpl", ignoreCase = true)) {
            serverStarter.ensureServerStarted(NlplLspServerDescriptor(project))
        }
    }
}

/**
 * Descriptor for the NexusLang language server process.
 */
class NlplLspServerDescriptor(project: Project) : ProjectWideLspServerDescriptor(project, "NexusLang") {

    override fun isSupportedFile(file: VirtualFile): Boolean {
        return file.extension.equals("nlpl", ignoreCase = true)
    }

    override fun createCommandLine(): GeneralCommandLine {
        val envCommand = System.getenv("NLPL_LSP_COMMAND")
        if (envCommand != null) {
            val parts = envCommand.split(" ")
            return GeneralCommandLine(parts)
                .withWorkDirectory(project.basePath)
        }

        // Try `nlpl lsp` on PATH
        val nlplBin = findExecutableOnPath("nlpl")
        if (nlplBin != null) {
            return GeneralCommandLine(nlplBin.absolutePath, "lsp")
                .withWorkDirectory(project.basePath)
        }

        // Fall back to python3 -m nexuslang.lsp
        val python = findExecutableOnPath("python3") ?: findExecutableOnPath("python")
        val pythonPath = python?.absolutePath ?: "python3"
        return GeneralCommandLine(pythonPath, "-m", "nexuslang.lsp")
            .withWorkDirectory(project.basePath)
            .withEnvironment("PYTHONUNBUFFERED", "1")
    }

    private fun findExecutableOnPath(name: String): File? {
        val path = System.getenv("PATH") ?: return null
        return path.split(File.pathSeparator)
            .map { File(it, name) }
            .firstOrNull { it.canExecute() }
    }
}
