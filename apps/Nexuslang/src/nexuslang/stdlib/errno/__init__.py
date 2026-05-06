"""
Error number constants and utilities (C errno.h equivalent).

Provides standard error codes and error handling utilities.

Features:
- POSIX error constants (ENOENT, EACCES, etc.)
- Error message lookup
- Error code management

Example usage in NexusLang:
    # Error constants
    set file_not_found to errno_ENOENT()
    set permission_denied to errno_EACCES()
    
    # Error messages
    set msg to errno_strerror with errno_EINVAL()
    
    # Get/set errno
    errno_set with errno_ENOENT()
    set current_errno to errno_get()
"""

from ...runtime.runtime import Runtime
import errno as py_errno
import sys


# Global errno state
_errno_value = 0


# Standard POSIX error numbers
EPERM = 1           # Operation not permitted
ENOENT = 2          # No such file or directory
ESRCH = 3           # No such process
EINTR = 4           # Interrupted system call
EIO = 5             # I/O error
ENXIO = 6           # No such device or address
E2BIG = 7           # Argument list too long
ENOEXEC = 8         # Exec format error
EBADF = 9           # Bad file number
ECHILD = 10         # No child processes
EAGAIN = 11         # Try again
ENOMEM = 12         # Out of memory
EACCES = 13         # Permission denied
EFAULT = 14         # Bad address
ENOTBLK = 15        # Block device required
EBUSY = 16          # Device or resource busy
EEXIST = 17         # File exists
EXDEV = 18          # Cross-device link
ENODEV = 19         # No such device
ENOTDIR = 20        # Not a directory
EISDIR = 21         # Is a directory
EINVAL = 22         # Invalid argument
ENFILE = 23         # File table overflow
EMFILE = 24         # Too many open files
ENOTTY = 25         # Not a typewriter
ETXTBSY = 26        # Text file busy
EFBIG = 27          # File too large
ENOSPC = 28         # No space left on device
ESPIPE = 29         # Illegal seek
EROFS = 30          # Read-only file system
EMLINK = 31         # Too many links
EPIPE = 32          # Broken pipe
EDOM = 33           # Math argument out of domain
ERANGE = 34         # Math result not representable

# Network errors
EWOULDBLOCK = 11    # Operation would block (same as EAGAIN)
EINPROGRESS = 115   # Operation now in progress
EALREADY = 114      # Operation already in progress
ENOTSOCK = 88       # Socket operation on non-socket
EDESTADDRREQ = 89   # Destination address required
EMSGSIZE = 90       # Message too long
EPROTOTYPE = 91     # Protocol wrong type for socket
ENOPROTOOPT = 92    # Protocol not available
EPROTONOSUPPORT = 93  # Protocol not supported
ESOCKTNOSUPPORT = 94  # Socket type not supported
EOPNOTSUPP = 95     # Operation not supported on transport endpoint
EPFNOSUPPORT = 96   # Protocol family not supported
EAFNOSUPPORT = 97   # Address family not supported by protocol
EADDRINUSE = 98     # Address already in use
EADDRNOTAVAIL = 99  # Cannot assign requested address
ENETDOWN = 100      # Network is down
ENETUNREACH = 101   # Network is unreachable
ENETRESET = 102     # Network dropped connection because of reset
ECONNABORTED = 103  # Software caused connection abort
ECONNRESET = 104    # Connection reset by peer
ENOBUFS = 105       # No buffer space available
EISCONN = 106       # Transport endpoint is already connected
ENOTCONN = 107      # Transport endpoint is not connected
ESHUTDOWN = 108     # Cannot send after transport endpoint shutdown
ETOOMANYREFS = 109  # Too many references: cannot splice
ETIMEDOUT = 110     # Connection timed out
ECONNREFUSED = 111  # Connection refused
EHOSTDOWN = 112     # Host is down
EHOSTUNREACH = 113  # No route to host


# Error message mapping
ERROR_MESSAGES = {
    EPERM: "Operation not permitted",
    ENOENT: "No such file or directory",
    ESRCH: "No such process",
    EINTR: "Interrupted system call",
    EIO: "I/O error",
    ENXIO: "No such device or address",
    E2BIG: "Argument list too long",
    ENOEXEC: "Exec format error",
    EBADF: "Bad file number",
    ECHILD: "No child processes",
    EAGAIN: "Try again",
    ENOMEM: "Out of memory",
    EACCES: "Permission denied",
    EFAULT: "Bad address",
    ENOTBLK: "Block device required",
    EBUSY: "Device or resource busy",
    EEXIST: "File exists",
    EXDEV: "Cross-device link",
    ENODEV: "No such device",
    ENOTDIR: "Not a directory",
    EISDIR: "Is a directory",
    EINVAL: "Invalid argument",
    ENFILE: "File table overflow",
    EMFILE: "Too many open files",
    ENOTTY: "Not a typewriter",
    ETXTBSY: "Text file busy",
    EFBIG: "File too large",
    ENOSPC: "No space left on device",
    ESPIPE: "Illegal seek",
    EROFS: "Read-only file system",
    EMLINK: "Too many links",
    EPIPE: "Broken pipe",
    EDOM: "Math argument out of domain of function",
    ERANGE: "Math result not representable",
    EINPROGRESS: "Operation now in progress",
    EALREADY: "Operation already in progress",
    ENOTSOCK: "Socket operation on non-socket",
    EDESTADDRREQ: "Destination address required",
    EMSGSIZE: "Message too long",
    EPROTOTYPE: "Protocol wrong type for socket",
    ENOPROTOOPT: "Protocol not available",
    EPROTONOSUPPORT: "Protocol not supported",
    ESOCKTNOSUPPORT: "Socket type not supported",
    EOPNOTSUPP: "Operation not supported",
    EPFNOSUPPORT: "Protocol family not supported",
    EAFNOSUPPORT: "Address family not supported by protocol",
    EADDRINUSE: "Address already in use",
    EADDRNOTAVAIL: "Cannot assign requested address",
    ENETDOWN: "Network is down",
    ENETUNREACH: "Network is unreachable",
    ENETRESET: "Network dropped connection on reset",
    ECONNABORTED: "Software caused connection abort",
    ECONNRESET: "Connection reset by peer",
    ENOBUFS: "No buffer space available",
    EISCONN: "Transport endpoint is already connected",
    ENOTCONN: "Transport endpoint is not connected",
    ESHUTDOWN: "Cannot send after transport endpoint shutdown",
    ETOOMANYREFS: "Too many references: cannot splice",
    ETIMEDOUT: "Connection timed out",
    ECONNREFUSED: "Connection refused",
    EHOSTDOWN: "Host is down",
    EHOSTUNREACH: "No route to host",
}


def errno_get():
    """
    Get current errno value.
    
    Returns:
        Current errno
    """
    return _errno_value


def errno_set(value):
    """
    Set errno value.
    
    Args:
        value: Error number to set
    """
    global _errno_value
    _errno_value = int(value)


def errno_clear():
    """Clear errno (set to 0)."""
    global _errno_value
    _errno_value = 0


def errno_strerror(errnum):
    """
    Get error message for error number.
    
    Args:
        errnum: Error number
    
    Returns:
        Error message string
    """
    errnum = int(errnum)
    
    # Try NexusLang error messages first
    if errnum in ERROR_MESSAGES:
        return ERROR_MESSAGES[errnum]
    
    # Fall back to Python's errno
    try:
        return py_errno.errorcode.get(errnum, f"Unknown error {errnum}")
    except:
        return f"Unknown error {errnum}"


def errno_perror(message=""):
    """
    Print error message to stderr.
    
    Args:
        message: Prefix message
    """
    error_msg = errno_strerror(_errno_value)
    
    if message:
        print(f"{message}: {error_msg}", file=sys.stderr)
    else:
        print(error_msg, file=sys.stderr)


# Error constant functions
def errno_EPERM(): return EPERM
def errno_ENOENT(): return ENOENT
def errno_ESRCH(): return ESRCH
def errno_EINTR(): return EINTR
def errno_EIO(): return EIO
def errno_ENXIO(): return ENXIO
def errno_E2BIG(): return E2BIG
def errno_ENOEXEC(): return ENOEXEC
def errno_EBADF(): return EBADF
def errno_ECHILD(): return ECHILD
def errno_EAGAIN(): return EAGAIN
def errno_ENOMEM(): return ENOMEM
def errno_EACCES(): return EACCES
def errno_EFAULT(): return EFAULT
def errno_ENOTBLK(): return ENOTBLK
def errno_EBUSY(): return EBUSY
def errno_EEXIST(): return EEXIST
def errno_EXDEV(): return EXDEV
def errno_ENODEV(): return ENODEV
def errno_ENOTDIR(): return ENOTDIR
def errno_EISDIR(): return EISDIR
def errno_EINVAL(): return EINVAL
def errno_ENFILE(): return ENFILE
def errno_EMFILE(): return EMFILE
def errno_ENOTTY(): return ENOTTY
def errno_ETXTBSY(): return ETXTBSY
def errno_EFBIG(): return EFBIG
def errno_ENOSPC(): return ENOSPC
def errno_ESPIPE(): return ESPIPE
def errno_EROFS(): return EROFS
def errno_EMLINK(): return EMLINK
def errno_EPIPE(): return EPIPE
def errno_EDOM(): return EDOM
def errno_ERANGE(): return ERANGE

# Network error constants
def errno_EWOULDBLOCK(): return EWOULDBLOCK
def errno_EINPROGRESS(): return EINPROGRESS
def errno_EALREADY(): return EALREADY
def errno_ENOTSOCK(): return ENOTSOCK
def errno_EDESTADDRREQ(): return EDESTADDRREQ
def errno_EMSGSIZE(): return EMSGSIZE
def errno_EPROTOTYPE(): return EPROTOTYPE
def errno_ENOPROTOOPT(): return ENOPROTOOPT
def errno_EPROTONOSUPPORT(): return EPROTONOSUPPORT
def errno_ESOCKTNOSUPPORT(): return ESOCKTNOSUPPORT
def errno_EOPNOTSUPP(): return EOPNOTSUPP
def errno_EPFNOSUPPORT(): return EPFNOSUPPORT
def errno_EAFNOSUPPORT(): return EAFNOSUPPORT
def errno_EADDRINUSE(): return EADDRINUSE
def errno_EADDRNOTAVAIL(): return EADDRNOTAVAIL
def errno_ENETDOWN(): return ENETDOWN
def errno_ENETUNREACH(): return ENETUNREACH
def errno_ENETRESET(): return ENETRESET
def errno_ECONNABORTED(): return ECONNABORTED
def errno_ECONNRESET(): return ECONNRESET
def errno_ENOBUFS(): return ENOBUFS
def errno_EISCONN(): return EISCONN
def errno_ENOTCONN(): return ENOTCONN
def errno_ESHUTDOWN(): return ESHUTDOWN
def errno_ETOOMANYREFS(): return ETOOMANYREFS
def errno_ETIMEDOUT(): return ETIMEDOUT
def errno_ECONNREFUSED(): return ECONNREFUSED
def errno_EHOSTDOWN(): return EHOSTDOWN
def errno_EHOSTUNREACH(): return EHOSTUNREACH


def register_errno_functions(runtime: Runtime) -> None:
    """Register errno functions with the runtime."""
    
    # errno management
    runtime.register_function("errno_get", errno_get)
    runtime.register_function("errno_set", errno_set)
    runtime.register_function("errno_clear", errno_clear)
    runtime.register_function("errno_strerror", errno_strerror)
    runtime.register_function("errno_perror", errno_perror)
    
    # Standard error constants
    runtime.register_function("errno_EPERM", errno_EPERM)
    runtime.register_function("errno_ENOENT", errno_ENOENT)
    runtime.register_function("errno_ESRCH", errno_ESRCH)
    runtime.register_function("errno_EINTR", errno_EINTR)
    runtime.register_function("errno_EIO", errno_EIO)
    runtime.register_function("errno_ENXIO", errno_ENXIO)
    runtime.register_function("errno_E2BIG", errno_E2BIG)
    runtime.register_function("errno_ENOEXEC", errno_ENOEXEC)
    runtime.register_function("errno_EBADF", errno_EBADF)
    runtime.register_function("errno_ECHILD", errno_ECHILD)
    runtime.register_function("errno_EAGAIN", errno_EAGAIN)
    runtime.register_function("errno_ENOMEM", errno_ENOMEM)
    runtime.register_function("errno_EACCES", errno_EACCES)
    runtime.register_function("errno_EFAULT", errno_EFAULT)
    runtime.register_function("errno_ENOTBLK", errno_ENOTBLK)
    runtime.register_function("errno_EBUSY", errno_EBUSY)
    runtime.register_function("errno_EEXIST", errno_EEXIST)
    runtime.register_function("errno_EXDEV", errno_EXDEV)
    runtime.register_function("errno_ENODEV", errno_ENODEV)
    runtime.register_function("errno_ENOTDIR", errno_ENOTDIR)
    runtime.register_function("errno_EISDIR", errno_EISDIR)
    runtime.register_function("errno_EINVAL", errno_EINVAL)
    runtime.register_function("errno_ENFILE", errno_ENFILE)
    runtime.register_function("errno_EMFILE", errno_EMFILE)
    runtime.register_function("errno_ENOTTY", errno_ENOTTY)
    runtime.register_function("errno_ETXTBSY", errno_ETXTBSY)
    runtime.register_function("errno_EFBIG", errno_EFBIG)
    runtime.register_function("errno_ENOSPC", errno_ENOSPC)
    runtime.register_function("errno_ESPIPE", errno_ESPIPE)
    runtime.register_function("errno_EROFS", errno_EROFS)
    runtime.register_function("errno_EMLINK", errno_EMLINK)
    runtime.register_function("errno_EPIPE", errno_EPIPE)
    runtime.register_function("errno_EDOM", errno_EDOM)
    runtime.register_function("errno_ERANGE", errno_ERANGE)
    
    # Network error constants
    runtime.register_function("errno_EWOULDBLOCK", errno_EWOULDBLOCK)
    runtime.register_function("errno_EINPROGRESS", errno_EINPROGRESS)
    runtime.register_function("errno_EALREADY", errno_EALREADY)
    runtime.register_function("errno_ENOTSOCK", errno_ENOTSOCK)
    runtime.register_function("errno_EDESTADDRREQ", errno_EDESTADDRREQ)
    runtime.register_function("errno_EMSGSIZE", errno_EMSGSIZE)
    runtime.register_function("errno_EPROTOTYPE", errno_EPROTOTYPE)
    runtime.register_function("errno_ENOPROTOOPT", errno_ENOPROTOOPT)
    runtime.register_function("errno_EPROTONOSUPPORT", errno_EPROTONOSUPPORT)
    runtime.register_function("errno_ESOCKTNOSUPPORT", errno_ESOCKTNOSUPPORT)
    runtime.register_function("errno_EOPNOTSUPP", errno_EOPNOTSUPP)
    runtime.register_function("errno_EPFNOSUPPORT", errno_EPFNOSUPPORT)
    runtime.register_function("errno_EAFNOSUPPORT", errno_EAFNOSUPPORT)
    runtime.register_function("errno_EADDRINUSE", errno_EADDRINUSE)
    runtime.register_function("errno_EADDRNOTAVAIL", errno_EADDRNOTAVAIL)
    runtime.register_function("errno_ENETDOWN", errno_ENETDOWN)
    runtime.register_function("errno_ENETUNREACH", errno_ENETUNREACH)
    runtime.register_function("errno_ENETRESET", errno_ENETRESET)
    runtime.register_function("errno_ECONNABORTED", errno_ECONNABORTED)
    runtime.register_function("errno_ECONNRESET", errno_ECONNRESET)
    runtime.register_function("errno_ENOBUFS", errno_ENOBUFS)
    runtime.register_function("errno_EISCONN", errno_EISCONN)
    runtime.register_function("errno_ENOTCONN", errno_ENOTCONN)
    runtime.register_function("errno_ESHUTDOWN", errno_ESHUTDOWN)
    runtime.register_function("errno_ETOOMANYREFS", errno_ETOOMANYREFS)
    runtime.register_function("errno_ETIMEDOUT", errno_ETIMEDOUT)
    runtime.register_function("errno_ECONNREFUSED", errno_ECONNREFUSED)
    runtime.register_function("errno_EHOSTDOWN", errno_EHOSTDOWN)
    runtime.register_function("errno_EHOSTUNREACH", errno_EHOSTUNREACH)
