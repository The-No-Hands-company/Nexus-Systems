"""
PDF utilities module for NexusLang.

Provides PDF generation, parsing, and text extraction capabilities.
Uses reportlab for PDF creation and PyPDF2 for PDF reading/manipulation.

Features:
- PDF Creation: Create PDFs with text, images, tables, shapes
- PDF Reading: Extract text, metadata, page info from existing PDFs
- PDF Manipulation: Merge, split, rotate, encrypt PDFs
- Optional dependencies: reportlab, PyPDF2

Example usage in NexusLang:
    # Create a PDF
    set pdf to pdf_create with "output.pdf"
    pdf_add_text with pdf and "Hello, World!" and 100 and 750
    pdf_save with pdf
    
    # Read a PDF
    set reader to pdf_open with "document.pdf"
    set page_count to pdf_get_page_count with reader
    set text to pdf_extract_text with reader and 0
    pdf_close with reader
"""

from ...runtime.runtime import Runtime

# Optional imports with fallback
try:
    from reportlab.pdfgen import canvas
    from reportlab.lib.pagesizes import letter, A4, legal
    from reportlab.lib.units import inch, cm
    from reportlab.lib.colors import HexColor
    from reportlab.platypus import SimpleDocTemplate, Table, TableStyle, Paragraph
    from reportlab.lib.styles import getSampleStyleSheet
    HAS_REPORTLAB = True
except ImportError:
    HAS_REPORTLAB = False

try:
    from PyPDF2 import PdfReader, PdfWriter, PdfMerger
    HAS_PYPDF2 = True
except ImportError:
    HAS_PYPDF2 = False

# Global storage for PDF objects
_pdf_canvases = {}  # {canvas_id: canvas_object}
_pdf_readers = {}   # {reader_id: PdfReader_object}
_pdf_writers = {}   # {writer_id: PdfWriter_object}
_pdf_mergers = {}   # {merger_id: PdfMerger_object}
_pdf_counter = 0


def _check_reportlab():
    """Check if reportlab is available."""
    if not HAS_REPORTLAB:
        raise RuntimeError(
            "reportlab is not installed. Install it with: pip install reportlab"
        )


def _check_pypdf2():
    """Check if PyPDF2 is available."""
    if not HAS_PYPDF2:
        raise RuntimeError(
            "PyPDF2 is not installed. Install it with: pip install PyPDF2"
        )


# ============================================================================
# PDF Creation (reportlab)
# ============================================================================

def pdf_create(filename, page_size="letter"):
    """
    Create a new PDF document.
    
    Args:
        filename: Path to save the PDF
        page_size: Page size ("letter", "a4", "legal")
    
    Returns:
        Canvas ID for further operations
    """
    _check_reportlab()
    global _pdf_counter
    
    # Map page size names
    size_map = {
        "letter": letter,
        "a4": A4,
        "legal": legal,
    }
    pagesize = size_map.get(page_size.lower(), letter)
    
    # Create canvas
    c = canvas.Canvas(filename, pagesize=pagesize)
    
    # Store canvas
    canvas_id = _pdf_counter
    _pdf_canvases[canvas_id] = c
    _pdf_counter += 1
    
    return canvas_id


def pdf_add_text(canvas_id, text, x, y, font_name="Helvetica", font_size=12):
    """
    Add text to PDF at specified position.
    
    Args:
        canvas_id: Canvas ID from pdf_create
        text: Text to add
        x: X coordinate
        y: Y coordinate
        font_name: Font name (default: Helvetica)
        font_size: Font size (default: 12)
    """
    _check_reportlab()
    
    if canvas_id not in _pdf_canvases:
        raise ValueError(f"Invalid canvas ID: {canvas_id}")
    
    c = _pdf_canvases[canvas_id]
    c.setFont(font_name, font_size)
    c.drawString(x, y, str(text))


def pdf_add_line(canvas_id, x1, y1, x2, y2, width=1, color="#000000"):
    """
    Draw a line on the PDF.
    
    Args:
        canvas_id: Canvas ID
        x1, y1: Start coordinates
        x2, y2: End coordinates
        width: Line width (default: 1)
        color: Line color in hex (default: black)
    """
    _check_reportlab()
    
    if canvas_id not in _pdf_canvases:
        raise ValueError(f"Invalid canvas ID: {canvas_id}")
    
    c = _pdf_canvases[canvas_id]
    c.setStrokeColor(HexColor(color))
    c.setLineWidth(width)
    c.line(x1, y1, x2, y2)


def pdf_add_rectangle(canvas_id, x, y, width, height, fill=False, color="#000000"):
    """
    Draw a rectangle on the PDF.
    
    Args:
        canvas_id: Canvas ID
        x, y: Bottom-left corner coordinates
        width: Rectangle width
        height: Rectangle height
        fill: Whether to fill the rectangle
        color: Color in hex
    """
    _check_reportlab()
    
    if canvas_id not in _pdf_canvases:
        raise ValueError(f"Invalid canvas ID: {canvas_id}")
    
    c = _pdf_canvases[canvas_id]
    
    if fill:
        c.setFillColor(HexColor(color))
        c.rect(x, y, width, height, fill=1)
    else:
        c.setStrokeColor(HexColor(color))
        c.rect(x, y, width, height, fill=0)


def pdf_add_page(canvas_id):
    """
    Add a new page to the PDF.
    
    Args:
        canvas_id: Canvas ID
    """
    _check_reportlab()
    
    if canvas_id not in _pdf_canvases:
        raise ValueError(f"Invalid canvas ID: {canvas_id}")
    
    c = _pdf_canvases[canvas_id]
    c.showPage()


def pdf_save(canvas_id):
    """
    Save and close the PDF.
    
    Args:
        canvas_id: Canvas ID
    """
    _check_reportlab()
    
    if canvas_id not in _pdf_canvases:
        raise ValueError(f"Invalid canvas ID: {canvas_id}")
    
    c = _pdf_canvases[canvas_id]
    c.save()
    
    # Remove from storage
    del _pdf_canvases[canvas_id]


# ============================================================================
# PDF Reading (PyPDF2)
# ============================================================================

def pdf_open(filename):
    """
    Open a PDF file for reading.
    
    Args:
        filename: Path to PDF file
    
    Returns:
        Reader ID for further operations
    """
    _check_pypdf2()
    global _pdf_counter
    
    reader = PdfReader(filename)
    
    # Store reader
    reader_id = _pdf_counter
    _pdf_readers[reader_id] = reader
    _pdf_counter += 1
    
    return reader_id


def pdf_get_page_count(reader_id):
    """
    Get number of pages in PDF.
    
    Args:
        reader_id: Reader ID from pdf_open
    
    Returns:
        Number of pages
    """
    _check_pypdf2()
    
    if reader_id not in _pdf_readers:
        raise ValueError(f"Invalid reader ID: {reader_id}")
    
    reader = _pdf_readers[reader_id]
    return len(reader.pages)


def pdf_extract_text(reader_id, page_number):
    """
    Extract text from a specific page.
    
    Args:
        reader_id: Reader ID
        page_number: Page number (0-indexed)
    
    Returns:
        Extracted text
    """
    _check_pypdf2()
    
    if reader_id not in _pdf_readers:
        raise ValueError(f"Invalid reader ID: {reader_id}")
    
    reader = _pdf_readers[reader_id]
    
    if page_number < 0 or page_number >= len(reader.pages):
        raise ValueError(f"Invalid page number: {page_number}")
    
    page = reader.pages[page_number]
    return page.extract_text()


def pdf_get_metadata(reader_id):
    """
    Get PDF metadata (title, author, etc.).
    
    Args:
        reader_id: Reader ID
    
    Returns:
        Dictionary with metadata
    """
    _check_pypdf2()
    
    if reader_id not in _pdf_readers:
        raise ValueError(f"Invalid reader ID: {reader_id}")
    
    reader = _pdf_readers[reader_id]
    metadata = reader.metadata
    
    # Convert to regular dict
    result = {}
    if metadata:
        for key, value in metadata.items():
            # Remove leading '/' from keys
            clean_key = key.lstrip('/')
            result[clean_key] = value
    
    return result


def pdf_close(reader_id):
    """
    Close PDF reader and release resources.
    
    Args:
        reader_id: Reader ID
    """
    _check_pypdf2()
    
    if reader_id not in _pdf_readers:
        raise ValueError(f"Invalid reader ID: {reader_id}")
    
    # Remove from storage
    del _pdf_readers[reader_id]


# ============================================================================
# PDF Manipulation (PyPDF2)
# ============================================================================

def pdf_merge(output_filename, input_files):
    """
    Merge multiple PDF files into one.
    
    Args:
        output_filename: Path for merged PDF
        input_files: List of PDF file paths to merge
    """
    _check_pypdf2()
    
    merger = PdfMerger()
    
    # Add all PDFs
    for pdf_file in input_files:
        merger.append(pdf_file)
    
    # Write merged PDF
    merger.write(output_filename)
    merger.close()


def pdf_split_pages(input_filename, start_page, end_page, output_filename):
    """
    Extract a range of pages from PDF.
    
    Args:
        input_filename: Source PDF
        start_page: Starting page (0-indexed)
        end_page: Ending page (0-indexed, exclusive)
        output_filename: Output PDF path
    """
    _check_pypdf2()
    
    reader = PdfReader(input_filename)
    writer = PdfWriter()
    
    # Add pages in range
    for i in range(start_page, end_page):
        if i < len(reader.pages):
            writer.add_page(reader.pages[i])
    
    # Write output
    with open(output_filename, 'wb') as output_file:
        writer.write(output_file)


def pdf_rotate_page(input_filename, page_number, rotation, output_filename):
    """
    Rotate a specific page in PDF.
    
    Args:
        input_filename: Source PDF
        page_number: Page to rotate (0-indexed)
        rotation: Rotation angle (90, 180, 270)
        output_filename: Output PDF path
    """
    _check_pypdf2()
    
    reader = PdfReader(input_filename)
    writer = PdfWriter()
    
    # Copy all pages, rotating the specified one
    for i, page in enumerate(reader.pages):
        if i == page_number:
            page.rotate(rotation)
        writer.add_page(page)
    
    # Write output
    with open(output_filename, 'wb') as output_file:
        writer.write(output_file)


def pdf_encrypt(input_filename, output_filename, password):
    """
    Encrypt a PDF with a password.
    
    Args:
        input_filename: Source PDF
        output_filename: Encrypted PDF path
        password: Encryption password
    """
    _check_pypdf2()
    
    reader = PdfReader(input_filename)
    writer = PdfWriter()
    
    # Copy all pages
    for page in reader.pages:
        writer.add_page(page)
    
    # Encrypt
    writer.encrypt(password)
    
    # Write output
    with open(output_filename, 'wb') as output_file:
        writer.write(output_file)


# ============================================================================
# Registration
# ============================================================================

def register_pdf_functions(runtime: Runtime) -> None:
    """Register PDF utility functions with the runtime."""
    
    # PDF Creation
    runtime.register_function("pdf_create", pdf_create)
    runtime.register_function("pdf_add_text", pdf_add_text)
    runtime.register_function("pdf_add_line", pdf_add_line)
    runtime.register_function("pdf_add_rectangle", pdf_add_rectangle)
    runtime.register_function("pdf_add_page", pdf_add_page)
    runtime.register_function("pdf_save", pdf_save)
    
    # PDF Reading
    runtime.register_function("pdf_open", pdf_open)
    runtime.register_function("pdf_get_page_count", pdf_get_page_count)
    runtime.register_function("pdf_extract_text", pdf_extract_text)
    runtime.register_function("pdf_get_metadata", pdf_get_metadata)
    runtime.register_function("pdf_close", pdf_close)
    
    # PDF Manipulation
    runtime.register_function("pdf_merge", pdf_merge)
    runtime.register_function("pdf_split_pages", pdf_split_pages)
    runtime.register_function("pdf_rotate_page", pdf_rotate_page)
    runtime.register_function("pdf_encrypt", pdf_encrypt)
