"""
NexusLang Debug Adapter Protocol (DAP) Server
=========================================

Implements Microsoft's Debug Adapter Protocol to enable debugging NexusLang programs
in VS Code and other DAP-compatible editors.

Protocol Reference: https://microsoft.github.io/debug-adapter-protocol/

Features:
- Launch and attach modes
- Breakpoints (line, conditional, function)
- Step execution (in, over, out)
- Variable inspection (locals, globals, scopes)
- Call stack navigation
- Expression evaluation
- Exception breakpoints
- Source mapping

Architecture:
- JSON-RPC 2.0 over stdio (like LSP)
- Wraps existing NexusLang Debugger class
- Event-driven (sends events to client)
"""

import json
import sys
import logging
import os
import threading
from typing import Dict, List, Optional, Any
from pathlib import Path
from dataclasses import dataclass, asdict

from .debugger import Debugger, DebuggerState, Breakpoint, CallFrame


# Configure logging
logging.basicConfig(
    filename='/tmp/nexuslang-dap.log',
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('nexuslang-dap')

# In-process debuggee registry keyed by process id. This enables attach-by-pid
# semantics for sessions started by launch in this process.
_DEBUGGEE_REGISTRY_LOCK = threading.RLock()
_DEBUGGEE_REGISTRY: Dict[int, Dict[str, Any]] = {}


@dataclass
class DAPCapabilities:
    """DAP server capabilities."""
    supportsConfigurationDoneRequest: bool = True
    supportsAttachRequest: bool = True
    supportsFunctionBreakpoints: bool = False  # Future
    supportsConditionalBreakpoints: bool = True
    supportsHitConditionalBreakpoints: bool = False  # Future
    supportsEvaluateForHovers: bool = True
    supportsStepBack: bool = False
    supportsSetVariable: bool = True
    supportsRestartFrame: bool = False
    supportsGotoTargetsRequest: bool = False
    supportsStepInTargetsRequest: bool = False
    supportsCompletionsRequest: bool = False
    supportsModulesRequest: bool = False
    supportsRestartRequest: bool = False
    supportsExceptionOptions: bool = False
    supportsValueFormattingOptions: bool = False
    supportsExceptionInfoRequest: bool = False
    supportTerminateDebuggee: bool = True
    supportsDelayedStackTraceLoading: bool = False
    supportsLoadedSourcesRequest: bool = False
    supportsLogPoints: bool = False
    supportsTerminateThreadsRequest: bool = False
    supportsSetExpression: bool = False
    supportsTerminateRequest: bool = True
    supportsDataBreakpoints: bool = False
    supportsReadMemoryRequest: bool = False
    supportsDisassembleRequest: bool = False
    supportsCancelRequest: bool = False
    supportsBreakpointLocationsRequest: bool = False


class DAPServer:
    """
    Debug Adapter Protocol server for NexusLang.
    
    Handles DAP requests and wraps the NexusLang Debugger.
    """
    
    def __init__(self):
        self.debugger: Optional[Debugger] = None
        self.interpreter = None
        self.runtime = None
        
        # DAP state
        self.seq = 1
        self.client_id: Optional[str] = None
        self.client_name: Optional[str] = None
        self.launch_config: Dict[str, Any] = {}
        
        # Thread tracking (single-threaded for now)
        self.thread_id = 1
        self.stopped = False
        self.stop_reason: Optional[str] = None
        
        # Breakpoint tracking (DAP ID -> NexusLang Breakpoint)
        self.breakpoint_id_counter = 1
        self.breakpoints: Dict[int, Breakpoint] = {}
        
        # Variable references (for complex objects)
        self.variable_ref_counter = 1
        self.variable_refs: Dict[int, Any] = {}
        self._attached_process_id: Optional[int] = None
        
        logger.info("DAP Server initialized")
    
    def run(self):
        """Main server loop - read requests from stdin."""
        logger.info("DAP Server starting...")
        
        while True:
            try:
                # Read Content-Length header
                header = sys.stdin.buffer.readline().decode('utf-8')
                
                if not header:
                    logger.info("Client disconnected")
                    break
                
                if not header.startswith('Content-Length:'):
                    logger.warning(f"Invalid header: {header}")
                    continue
                
                content_length = int(header.split(':')[1].strip())
                
                # Read empty line
                sys.stdin.buffer.readline()
                
                # Read message body
                body = sys.stdin.buffer.read(content_length).decode('utf-8')
                message = json.loads(body)
                
                logger.debug(f"Received: {message}")
                
                # Handle message
                self.handle_message(message)
            
            except KeyboardInterrupt:
                logger.info("Server interrupted")
                break
            except Exception as e:
                logger.error(f"Error in main loop: {e}", exc_info=True)
    
    def handle_message(self, message: Dict[str, Any]):
        """Dispatch message to appropriate handler."""
        msg_type = message.get('type')
        
        if msg_type == 'request':
            command = message.get('command')
            seq = message.get('seq')
            args = message.get('arguments', {})
            
            logger.info(f"Request: {command}")
            
            # Dispatch to handler
            handler_name = f"_handle_{command}"
            handler = getattr(self, handler_name, None)
            
            if handler:
                try:
                    response = handler(seq, args)
                    self.send_response(seq, command, success=True, body=response)
                except Exception as e:
                    logger.error(f"Error in {command}: {e}", exc_info=True)
                    self.send_response(seq, command, success=False, 
                                     message=str(e))
            else:
                logger.warning(f"Unhandled command: {command}")
                self.send_response(seq, command, success=False,
                                 message=f"Command not supported: {command}")
    
    def send_response(self, request_seq: int, command: str, 
                     success: bool = True, message: str = "", body: Any = None):
        """Send response to client."""
        response = {
            'type': 'response',
            'seq': self.seq,
            'request_seq': request_seq,
            'command': command,
            'success': success
        }
        
        if message:
            response['message'] = message
        
        if body is not None:
            response['body'] = body
        
        self.seq += 1
        self._send_message(response)
    
    def send_event(self, event: str, body: Any = None):
        """Send event to client."""
        message = {
            'type': 'event',
            'seq': self.seq,
            'event': event
        }
        
        if body is not None:
            message['body'] = body
        
        self.seq += 1
        self._send_message(message)
        logger.debug(f"Sent event: {event}")
    
    def _send_message(self, message: Dict[str, Any]):
        """Send JSON-RPC message to client."""
        content = json.dumps(message)
        content_bytes = content.encode('utf-8')
        
        header = f"Content-Length: {len(content_bytes)}\r\n\r\n"
        
        sys.stdout.buffer.write(header.encode('utf-8'))
        sys.stdout.buffer.write(content_bytes)
        sys.stdout.buffer.flush()
        
        logger.debug(f"Sent: {message}")
    
    # ============= DAP Request Handlers =============
    
    def _handle_initialize(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize request - first message from client."""
        self.client_id = args.get('clientID')
        self.client_name = args.get('clientName')
        
        logger.info(f"Client: {self.client_name} ({self.client_id})")
        
        # Send initialized event
        self.send_event('initialized')
        
        # Return capabilities
        caps = DAPCapabilities()
        return asdict(caps)
    
    def _handle_launch(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Launch request - start debugging a program."""
        self.launch_config = args
        
        program = args.get('program')
        if not program:
            raise ValueError("No program specified in launch config")
        
        program_path = Path(program)
        if not program_path.exists():
            raise FileNotFoundError(f"Program not found: {program}")
        
        logger.info(f"Launching program: {program}")
        
        # Read source
        with open(program, 'r') as f:
            source = f.read()
        
        # Setup interpreter and debugger
        from ..interpreter.interpreter import Interpreter
        from ..runtime.runtime import Runtime
        from ..stdlib import register_stdlib
        
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)
        
        # Create debugger (non-interactive mode)
        self.debugger = Debugger(self.interpreter, interactive=False)
        
        # Register debugger callbacks
        self.debugger.on_breakpoint = self._on_breakpoint
        self.debugger.on_step = self._on_step
        self.debugger.on_exception = self._on_exception
        
        # Attach debugger to interpreter
        self.interpreter.debugger = self.debugger
        
        # Parse program (but don't run yet - wait for configurationDone)
        from ..parser.lexer import Lexer
        from ..parser.parser import Parser
        
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        self.ast = parser.parse()

        # Make this debuggee attachable by process id for other DAP sessions
        # in the same process.
        self._register_debuggee(os.getpid())
        
        logger.info("Program parsed successfully")
        
        return {}
    
    def _handle_attach(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Attach request - attach to a debug target.

        Current implementation supports file-based attach by accepting a
        NexusLang source path and preparing it exactly like launch mode.
        """
        process_id = args.get('processId')
        if process_id is not None:
            try:
                pid = int(process_id)
            except (TypeError, ValueError) as exc:
                raise ValueError("Attach mode requires numeric processId") from exc

            if pid <= 0:
                raise ValueError("Attach mode requires processId > 0")

            if self._attach_to_registered_debuggee(pid):
                logger.info(f"Attached to existing debuggee process: {pid}")
                return {}

            raise ValueError(
                f"No attachable NexusLang debug session found for processId={pid}. "
                "Launch a program first or provide a source program path for file-based attach."
            )

        program = args.get('program') or args.get('processPath') or args.get('target')
        if not program:
            raise ValueError(
                "Attach mode requires 'program' (or processPath/target) to point to a NexusLang source file"
            )

        logger.info(f"Attach request using program source: {program}")

        attach_args = dict(args)
        attach_args['program'] = program
        return self._handle_launch(seq, attach_args)
    
    def _handle_configurationDone(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Configuration done - all breakpoints set, ready to run."""
        logger.info("Configuration done - starting program execution")

        if self.interpreter is None or not hasattr(self, 'ast'):
            raise ValueError(
                "No program is loaded for debugging. Send a launch request before configurationDone."
            )
        
        # Run program in background (will pause at breakpoints)
        # For now, run synchronously since NexusLang interpreter is single-threaded
        try:
            self.interpreter.interpret(self.ast)
            
            # Program completed successfully
            self.send_event('terminated')
            
        except Exception as e:
            logger.error(f"Program error: {e}", exc_info=True)
            if self.debugger is not None:
                self.debugger.trace_exception(e)
            self.send_event('terminated')
        
        return {}
    
    def _handle_setBreakpoints(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Set breakpoints in a source file."""
        source = args.get('source', {})
        source_path = source.get('path')
        breakpoints_args = args.get('breakpoints', [])
        
        if not source_path:
            return {'breakpoints': []}
        
        logger.info(f"Setting {len(breakpoints_args)} breakpoint(s) in {source_path}")
        
        # Clear existing breakpoints for this file
        if self.debugger:
            self.debugger.clear_breakpoints(source_path)
        
        # Set new breakpoints
        breakpoints_response = []
        
        for bp_args in breakpoints_args:
            line = bp_args.get('line')
            condition = bp_args.get('condition')
            
            if self.debugger:
                bp = self.debugger.add_breakpoint(source_path, line, condition=condition)
                
                # Assign DAP ID
                bp_id = self.breakpoint_id_counter
                self.breakpoint_id_counter += 1
                self.breakpoints[bp_id] = bp
                
                breakpoints_response.append({
                    'id': bp_id,
                    'verified': True,
                    'line': line,
                    'source': {'path': source_path}
                })
            else:
                # No debugger yet (shouldn't happen)
                breakpoints_response.append({
                    'verified': False,
                    'line': line,
                    'message': 'Debugger not initialized'
                })
        
        return {'breakpoints': breakpoints_response}
    
    def _handle_threads(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Return list of threads (single-threaded for now)."""
        return {
            'threads': [
                {'id': self.thread_id, 'name': 'Main Thread'}
            ]
        }
    
    def _handle_stackTrace(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Return call stack frames."""
        if not self.debugger:
            return {'stackFrames': [], 'totalFrames': 0}
        
        frames = []
        
        # Convert NexusLang call stack to DAP stack frames
        for i, frame in enumerate(reversed(self.debugger.call_stack)):
            frames.append({
                'id': i,
                'name': frame.function_name,
                'source': {'path': frame.file},
                'line': frame.line,
                'column': 0
            })
        
        return {'stackFrames': frames, 'totalFrames': len(frames)}
    
    def _handle_scopes(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Return variable scopes for a stack frame."""
        frame_id = args.get('frameId', 0)
        
        scopes = []
        
        # Local scope
        local_ref = self._create_variable_ref('locals', frame_id)
        scopes.append({
            'name': 'Locals',
            'variablesReference': local_ref,
            'expensive': False
        })
        
        # Global scope
        global_ref = self._create_variable_ref('globals', frame_id)
        scopes.append({
            'name': 'Globals',
            'variablesReference': global_ref,
            'expensive': False
        })
        
        return {'scopes': scopes}
    
    def _handle_variables(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Return variables for a scope."""
        variables_ref = args.get('variablesReference')
        
        if not variables_ref or variables_ref not in self.variable_refs:
            return {'variables': []}
        
        scope_info = self.variable_refs[variables_ref]
        scope_type = scope_info['type']
        
        variables = []
        
        if scope_type == 'locals' and self.debugger:
            # Get local variables
            vars_dict = self.debugger.inspect_all_variables()
            
            for name, value in vars_dict.items():
                variables.append({
                    'name': name,
                    'value': str(value),
                    'type': type(value).__name__,
                    'variablesReference': 0  # Simple values (no children)
                })
        
        elif scope_type == 'globals' and self.interpreter:
            # Get global variables from interpreter
            if hasattr(self.interpreter, 'global_scope'):
                for name, value in self.interpreter.global_scope.items():
                    if not name.startswith('_'):
                        variables.append({
                            'name': name,
                            'value': str(value),
                            'type': type(value).__name__,
                            'variablesReference': 0
                        })
        
        return {'variables': variables}
    
    def _handle_continue(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Continue execution."""
        if self.debugger:
            self.debugger.continue_execution()
            self.stopped = False
        
        return {'allThreadsContinued': True}
    
    def _handle_next(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Step over."""
        if self.debugger:
            self.debugger.step_over()
            self.stopped = False
        
        return {}
    
    def _handle_stepIn(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Step into."""
        if self.debugger:
            self.debugger.step_into()
            self.stopped = False
        
        return {}
    
    def _handle_stepOut(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Step out."""
        if self.debugger:
            self.debugger.step_out()
            self.stopped = False
        
        return {}
    
    def _handle_pause(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Pause execution."""
        # Set debugger to stepping mode (will pause on next line)
        if self.debugger:
            self.debugger.step_into()
        
        return {}
    
    def _handle_evaluate(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Evaluate expression."""
        expression = args.get('expression', '')
        context = args.get('context', 'watch')
        
        if not self.debugger or not self.interpreter:
            return {'result': 'Error: debugger not active', 'variablesReference': 0}
        
        try:
            # Try to get variable value
            value = self.debugger.inspect_variable(expression)
            
            if value is not None:
                return {
                    'result': str(value),
                    'type': type(value).__name__,
                    'variablesReference': 0
                }
            else:
                return {
                    'result': f"<undefined: {expression}>",
                    'variablesReference': 0
                }
        
        except Exception as e:
            return {'result': f'Error: {e}', 'variablesReference': 0}
    
    def _handle_disconnect(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Disconnect from debugger."""
        logger.info("Client disconnecting")
        
        if self.debugger:
            self.debugger.state = DebuggerState.FINISHED

        self._unregister_debuggee()
        
        return {}
    
    def _handle_terminate(self, seq: int, args: Dict[str, Any]) -> Dict[str, Any]:
        """Terminate debugging session."""
        logger.info("Terminating debug session")
        
        if self.debugger:
            self.debugger.state = DebuggerState.FINISHED

        self._unregister_debuggee()
        
        self.send_event('terminated')
        
        return {}
    
    # ============= Debugger Callbacks =============
    
    def _on_breakpoint(self, bp: Breakpoint, frame: Optional[CallFrame]):
        """Called when breakpoint is hit."""
        logger.info(f"Breakpoint hit: {bp.file}:{bp.line}")
        
        self.stopped = True
        self.stop_reason = 'breakpoint'
        
        # Send stopped event
        self.send_event('stopped', {
            'reason': 'breakpoint',
            'threadId': self.thread_id,
            'allThreadsStopped': True
        })
    
    def _on_step(self, file: str, line: int):
        """Called on each step."""
        if self.debugger and self.debugger.state in [
            DebuggerState.STEPPING, 
            DebuggerState.STEP_OVER, 
            DebuggerState.STEP_OUT
        ]:
            logger.info(f"Step: {file}:{line}")
            
            self.stopped = True
            self.stop_reason = 'step'
            
            # Send stopped event
            self.send_event('stopped', {
                'reason': 'step',
                'threadId': self.thread_id,
                'allThreadsStopped': True
            })
    
    def _on_exception(self, exception: Exception, frame: Optional[CallFrame]):
        """Called when exception occurs."""
        logger.error(f"Exception: {exception}")
        
        self.stopped = True
        self.stop_reason = 'exception'
        
        # Send stopped event
        self.send_event('stopped', {
            'reason': 'exception',
            'description': str(exception),
            'threadId': self.thread_id,
            'allThreadsStopped': True,
            'text': str(exception)
        })
    
    # ============= Helper Methods =============
    
    def _create_variable_ref(self, scope_type: str, frame_id: int) -> int:
        """Create variable reference for scope."""
        ref = self.variable_ref_counter
        self.variable_ref_counter += 1
        
        self.variable_refs[ref] = {
            'type': scope_type,
            'frameId': frame_id
        }
        
        return ref

    def _register_debuggee(self, process_id: int) -> None:
        """Register this server's debuggee state for attach-by-pid requests."""
        with _DEBUGGEE_REGISTRY_LOCK:
            _DEBUGGEE_REGISTRY[process_id] = {
                'runtime': self.runtime,
                'interpreter': self.interpreter,
                'debugger': self.debugger,
                'ast': getattr(self, 'ast', None),
            }
        self._attached_process_id = process_id

    def _attach_to_registered_debuggee(self, process_id: int) -> bool:
        """Attach this DAP session to a previously launched in-process debuggee."""
        with _DEBUGGEE_REGISTRY_LOCK:
            debuggee = _DEBUGGEE_REGISTRY.get(process_id)

        if not debuggee:
            return False

        self.runtime = debuggee.get('runtime')
        self.interpreter = debuggee.get('interpreter')
        self.debugger = debuggee.get('debugger')
        self.ast = debuggee.get('ast')

        if self.debugger is not None:
            # Redirect debugger callbacks to this DAP session.
            self.debugger.on_breakpoint = self._on_breakpoint
            self.debugger.on_step = self._on_step
            self.debugger.on_exception = self._on_exception

        if self.interpreter is not None and self.debugger is not None:
            self.interpreter.debugger = self.debugger

        self._attached_process_id = process_id
        return True

    def _unregister_debuggee(self) -> None:
        """Remove this server's registered debuggee when the session ends."""
        pid = self._attached_process_id
        if pid is None:
            return

        with _DEBUGGEE_REGISTRY_LOCK:
            current = _DEBUGGEE_REGISTRY.get(pid)
            if current and current.get('debugger') is self.debugger:
                _DEBUGGEE_REGISTRY.pop(pid, None)

        self._attached_process_id = None


def main():
    """Entry point for DAP server."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NexusLang Debug Adapter Protocol Server')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    parser.add_argument('--log-file', default='/tmp/nexuslang-dap.log', 
                       help='Log file path')
    
    args = parser.parse_args()
    
    # Configure logging
    if args.debug:
        logging.basicConfig(
            filename=args.log_file,
            level=logging.DEBUG,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            force=True
        )
    
    # Create and run server
    server = DAPServer()
    
    try:
        server.run()
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Server error: {e}", exc_info=True)
        raise


if __name__ == '__main__':
    main()
