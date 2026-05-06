import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';
import { activateDebugSupport } from './debugAdapter';

let client: LanguageClient;

// ---------------------------------------------------------------------------
// Diagnostic hover enrichment
// ---------------------------------------------------------------------------

/**
 * Structured data payload attached to NexusLang diagnostics by the LSP server.
 * Mirrors diagnostic.data shape defined in diagnostics.py _build_diagnostic().
 */
interface NLPLDiagnosticData {
    title?: string;
    category?: string;
    fixes?: string[];
    explainHint?: string;
    docLink?: string;
}

/**
 * Hover provider that enriches error-squiggle hover with NLPL-specific detail:
 * error title, category, top fixes, and the --explain CLI hint.
 * Registered for all .nlpl files; silently skips diagnostics without data.
 */
class NLPLDiagnosticHoverProvider implements vscode.HoverProvider {
    provideHover(
        document: vscode.TextDocument,
        position: vscode.Position,
        _token: vscode.CancellationToken
    ): vscode.Hover | undefined {
        const diagnostics = vscode.languages
            .getDiagnostics(document.uri)
            .filter(d => d.source === 'nlpl' && d.range.contains(position));

        if (diagnostics.length === 0) {
            return undefined;
        }

        const parts: vscode.MarkdownString[] = [];

        for (const diag of diagnostics) {
            const data = (diag as unknown as { data?: NLPLDiagnosticData }).data;
            if (!data) {
                continue;
            }

            const md = new vscode.MarkdownString('', true);
            md.isTrusted = true;

            const codeLabel = diag.code ? ` \`${diag.code}\`` : '';
            const titleText = data.title ?? diag.message;
            md.appendMarkdown(`**${titleText}**${codeLabel}`);

            if (data.category) {
                md.appendMarkdown(` — _${data.category}_`);
            }
            md.appendMarkdown('\n\n');

            if (data.fixes && data.fixes.length > 0) {
                md.appendMarkdown('**How to fix:**\n');
                for (const fix of data.fixes.slice(0, 3)) {
                    md.appendMarkdown(`- ${fix}\n`);
                }
                md.appendMarkdown('\n');
            }

            if (data.explainHint) {
                md.appendMarkdown(`_Run \`${data.explainHint}\` for full details_\n`);
            }

            if (data.docLink) {
                md.appendMarkdown(`[Documentation](${data.docLink})\n`);
            }

            parts.push(md);
        }

        if (parts.length === 0) {
            return undefined;
        }

        // Merge all hover sections into one Hover
        const combined = new vscode.MarkdownString('', true);
        combined.isTrusted = true;
        for (let i = 0; i < parts.length; i++) {
            if (i > 0) {
                combined.appendMarkdown('\n\n---\n\n');
            }
            combined.appendMarkdown(parts[i].value);
        }

        return new vscode.Hover(combined);
    }
}

// ---------------------------------------------------------------------------
// Explain Error Code command
// ---------------------------------------------------------------------------

/**
 * Show the explain output for the NexusLang error code on the diagnostic under the
 * cursor, or prompt the user to enter a code manually.
 */
async function explainErrorCode(context: vscode.ExtensionContext): Promise<void> {
    // Try to pick up code from the active editor cursor position first
    const editor = vscode.window.activeTextEditor;
    let code: string | undefined;

    if (editor) {
        const diags = vscode.languages
            .getDiagnostics(editor.document.uri)
            .filter(d =>
                d.source === 'nlpl' &&
                d.range.contains(editor.selection.active) &&
                d.code
            );

        if (diags.length > 0) {
            code = String(diags[0].code);
        }
    }

    if (!code) {
        code = await vscode.window.showInputBox({
            prompt: 'Enter NexusLang error code (e.g. E200)',
            placeHolder: 'E200',
            validateInput: v => /^E\d{3}$/.test(v.trim()) ? undefined : 'Format must be E followed by 3 digits'
        });
    }

    if (!code) {
        return;
    }

    code = code.trim().toUpperCase();

    // Run `python -m nxl --explain <CODE>` from workspace root
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    if (!workspaceFolder) {
        vscode.window.showErrorMessage('No workspace folder open.');
        return;
    }

    const config = vscode.workspace.getConfiguration('nexuslang');
    const pythonPath = config.get<string>('languageServer.pythonPath', 'python3');
    const srcPath = path.join(workspaceFolder.uri.fsPath, 'src');

    const terminal = vscode.window.createTerminal({
        name: `NLPL Explain ${code}`,
        cwd: workspaceFolder.uri.fsPath,
        env: { ...process.env, PYTHONPATH: srcPath }
    });
    terminal.sendText(`${pythonPath} -m nexuslang.main --explain ${code}`);
    terminal.show();
}

export async function activate(context: vscode.ExtensionContext) {
    console.log('[NexusLang] Extension activation started');
    vscode.window.showInformationMessage('NLPL extension is activating...');

    // Activate debug support
    activateDebugSupport(context);

    // Register diagnostic hover enrichment
    context.subscriptions.push(
        vscode.languages.registerHoverProvider(
            { scheme: 'file', language: 'nlpl' },
            new NLPLDiagnosticHoverProvider()
        )
    );

    // Register "NLPL: Explain Error Code" command
    context.subscriptions.push(
        vscode.commands.registerCommand('nexuslang.explainErrorCode', () => explainErrorCode(context))
    );

    // Get configuration
    const config = vscode.workspace.getConfiguration('nexuslang');
    const enabled = config.get<boolean>('languageServer.enabled', true);
    
    console.log('[NexusLang] Language server enabled:', enabled);

    if (!enabled) {
        console.log('[NexusLang] Language server is disabled');
        return;
    }

    // Determine server path
    let serverPath = config.get<string>('languageServer.path', '');
    let args: string[] = [];
    
    if (!serverPath) {
        // Prefer the workspace server script when available; otherwise use module entrypoint.
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
        const pythonPath = config.get<string>('languageServer.pythonPath', 'python3');
        serverPath = pythonPath;
        const defaultArgs = config.get<string[]>('languageServer.arguments', ['--stdio']);

        if (workspaceFolder) {
            const lspServerPath = path.join(workspaceFolder.uri.fsPath, 'src', 'nexuslang', 'lsp', 'server.py');
            if (fs.existsSync(lspServerPath)) {
                args = [lspServerPath, ...defaultArgs];
            } else {
                args = ['-m', 'nexuslang.lsp', ...defaultArgs];
            }
        } else {
            args = ['-m', 'nexuslang.lsp', ...defaultArgs];
        }
    } else {
        // Server path was manually configured
        args = config.get<string[]>('languageServer.arguments', []);
    }
    
    if (config.get<boolean>('languageServer.debug', false)) {
        args.push('--debug');
    }

    const logFile = config.get<string>('languageServer.logFile', '');
    if (logFile) {
        args.push('--log-file', logFile);
    }

    console.log('[NexusLang] Server command:', serverPath);
    console.log('[NexusLang] Server args:', args);

    // Server options with PYTHONPATH for workspace
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    const pythonPath = workspaceFolder ? path.join(workspaceFolder.uri.fsPath, 'src') : process.env.PYTHONPATH;
    
    console.log('[NexusLang] PYTHONPATH:', pythonPath);
    
    const serverOptions: ServerOptions = {
        command: serverPath,
        args: args,
        transport: TransportKind.stdio,
        options: {
            env: {
                ...process.env,
                PYTHONPATH: pythonPath
            }
        }
    };

    // Client options
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'nlpl' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.nxl')
        },
        initializationOptions: {
            linting: {
                enabled: config.get<boolean>('languageServer.linting.enabled', false),
                strict: config.get<boolean>('languageServer.linting.strict', false),
                errorsOnly: config.get<boolean>('languageServer.linting.errorsOnly', false),
            }
        }
    };

    // Create the language client
    console.log('[NexusLang] Creating language client...');
    
    client = new LanguageClient(
        'nlplLanguageServer',
        'NLPL Language Server',
        serverOptions,
        clientOptions
    );

    // Start the client (async)
    console.log('[NexusLang] Starting language client...');
    
    // Add timeout to prevent hanging forever
    const startPromise = client.start();
    const timeoutPromise = new Promise((_, reject) => 
        setTimeout(() => reject(new Error('LSP server start timeout after 10 seconds')), 10000)
    );
    
    try {
        await Promise.race([startPromise, timeoutPromise]);
        console.log('[NexusLang] Language server started successfully!');
        vscode.window.showInformationMessage('NLPL Language Server started successfully!');
    } catch (error) {
        console.error('[NexusLang] Failed to start language server:', error);
        vscode.window.showErrorMessage(`NLPL Language Server failed to start: ${error}`);
        
        // Show detailed diagnostic
        const diagnose = await vscode.window.showErrorMessage(
            'NLPL LSP failed to start. Check if Python and src/nlpl/lsp/server.py exist?',
            'Show Logs'
        );
        if (diagnose === 'Show Logs') {
            vscode.commands.executeCommand('workbench.action.output.show');
        }
    }
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
