"""
NexusLang Language Server - Main Entry Point
============================================

Run the NexusLang Language Server Protocol server.

Usage:
    python -m nexuslang.lsp
    python -m nexuslang.lsp --stdio (default)
    python -m nexuslang.lsp --tcp --port 5007
"""

import sys
import argparse
import logging
from .server import NexusLangLanguageServer


def main():
    """Main entry point for the NexusLang Language Server."""
    parser = argparse.ArgumentParser(
        description='NexusLang Language Server Protocol Implementation',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python -m nexuslang.lsp                    # Start LSP server on stdio
  python -m nexuslang.lsp --tcp --port 5007  # Start LSP server on TCP port
  python -m nexuslang.lsp --debug            # Enable debug logging
        """
    )
    
    parser.add_argument(
        '--stdio',
        action='store_true',
        default=True,
        help='Use stdio for communication (default)'
    )
    
    parser.add_argument(
        '--tcp',
        action='store_true',
        help='Use TCP socket for communication'
    )
    
    parser.add_argument(
        '--port',
        type=int,
        default=5007,
        help='TCP port to listen on (default: 5007)'
    )
    
    parser.add_argument(
        '--host',
        type=str,
        default='127.0.0.1',
        help='TCP host to bind to (default: 127.0.0.1)'
    )
    
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Enable debug logging to /tmp/nexuslang-lsp.log'
    )
    
    parser.add_argument(
        '--log-file',
        type=str,
        default='/tmp/nexuslang-lsp.log',
        help='Log file path (default: /tmp/nexuslang-lsp.log)'
    )
    
    args = parser.parse_args()
    
    # Configure logging
    if args.debug:
        logging.basicConfig(
            filename=args.log_file,
            level=logging.DEBUG,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        logger = logging.getLogger('nexuslang-lsp')
        logger.info(f"Starting NexusLang Language Server (debug mode)")
        logger.info(f"Args: {args}")
    
    # Create and start server
    server = NexusLangLanguageServer()
    
    if args.tcp:
        # TCP mode - not yet implemented
        print(f"TCP mode not yet implemented. Use --stdio (default).", file=sys.stderr)
        sys.exit(1)
    else:
        # STDIO mode (default)
        try:
            server.start()
        except KeyboardInterrupt:
            if args.debug:
                logger = logging.getLogger('nexuslang-lsp')
                logger.info("Server interrupted by user")
            sys.exit(0)
        except Exception as e:
            if args.debug:
                logger = logging.getLogger('nexuslang-lsp')
                logger.error(f"Server error: {e}", exc_info=True)
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)


if __name__ == '__main__':
    main()
