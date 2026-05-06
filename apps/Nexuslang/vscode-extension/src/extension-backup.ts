import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';
import { activateDebugSupport } from './debugAdapter';

let client: LanguageClient;

export async function activate(context: vscode.ExtensionContext) {
    console.log('[NexusLang] Extension activation started');
    vscode.window.showInformationMessage('NLPL extension is activating...');

    // Activate debug support
    activateDebugSupport(context);

    // Get configuration
    const config = vscode.workspace.getConfiguration('nlpl');
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
        // Default: use Python to run LSP server from workspace
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
        if (workspaceFolder) {
            const pythonPath = config.get<string>('languageServer.pythonPath', 'python3');
            const lspServerPath = path.join(workspaceFolder.uri.fsPath, 'src', 'nlpl', 'lsp', 'server.py');
            serverPath = pythonPath;
            // Run server.py directly (simpler than -m)
            args = [lspServerPath, '--stdio'];
        } else {
            // Fallback to system installation
            serverPath = 'nlpl-lsp';
            args = config.get<string[]>('languageServer.arguments', ['--stdio']);
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
    try {
        await client.start();
        console.log('[NexusLang] Language server started successfully!');
        vscode.window.showInformationMessage('NLPL Language Server started successfully!');
    } catch (error) {
        console.error('[NexusLang] Failed to start language server:', error);
        vscode.window.showErrorMessage(`NLPL Language Server failed to start: ${error}`);
    }
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
