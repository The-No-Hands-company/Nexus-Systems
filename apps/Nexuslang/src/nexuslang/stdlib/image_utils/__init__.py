"""
Image processing module for NexusLang.

Provides image loading, manipulation, and format conversion using PIL/Pillow.

Features:
- Image Loading: Load images from various formats (JPEG, PNG, GIF, BMP, TIFF)
- Image Manipulation: Resize, crop, rotate, flip, convert formats
- Image Filters: Blur, sharpen, edge detection, brightness, contrast
- Image Info: Get dimensions, format, mode
- Drawing: Add shapes, text to images
- Optional dependency: Pillow

Example usage in NexusLang:
    # Load and resize image
    set img to img_load with "photo.jpg"
    set resized to img_resize with img and 800 and 600
    img_save with resized and "photo_small.jpg"
    
    # Apply filter
    set blurred to img_blur with img and 5
    img_save with blurred and "photo_blur.jpg"
"""

from ...runtime.runtime import Runtime

# Optional import with fallback
try:
    from PIL import Image, ImageFilter, ImageEnhance, ImageDraw, ImageFont
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# Global storage for image objects
_images = {}  # {image_id: PIL.Image object}
_image_counter = 0


def _check_pil():
    """Check if PIL/Pillow is available."""
    if not HAS_PIL:
        raise RuntimeError(
            "Pillow is not installed. Install it with: pip install Pillow"
        )


# ============================================================================
# Image Loading & Saving
# ============================================================================

def img_load(filename):
    """
    Load an image from file.
    
    Args:
        filename: Path to image file
    
    Returns:
        Image ID for further operations
    """
    _check_pil()
    global _image_counter
    
    img = Image.open(filename)
    
    # Store image
    image_id = _image_counter
    _images[image_id] = img
    _image_counter += 1
    
    return image_id


def img_create(width, height, mode="RGB", color="#FFFFFF"):
    """
    Create a new blank image.
    
    Args:
        width: Image width
        height: Image height
        mode: Color mode (RGB, RGBA, L for grayscale)
        color: Background color (hex or name)
    
    Returns:
        Image ID
    """
    _check_pil()
    global _image_counter
    
    img = Image.new(mode, (width, height), color)
    
    # Store image
    image_id = _image_counter
    _images[image_id] = img
    _image_counter += 1
    
    return image_id


def img_save(image_id, filename, format=None, quality=95):
    """
    Save image to file.
    
    Args:
        image_id: Image ID
        filename: Output filename
        format: Image format (JPEG, PNG, GIF, etc.) - auto-detected if None
        quality: JPEG quality (1-100)
    """
    _check_pil()
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    
    if format:
        img.save(filename, format=format, quality=quality)
    else:
        img.save(filename, quality=quality)


def img_close(image_id):
    """
    Close image and release resources.
    
    Args:
        image_id: Image ID
    """
    _check_pil()
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    img.close()
    
    # Remove from storage
    del _images[image_id]


# ============================================================================
# Image Information
# ============================================================================

def img_get_size(image_id):
    """
    Get image dimensions.
    
    Args:
        image_id: Image ID
    
    Returns:
        Dictionary with width and height
    """
    _check_pil()
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    width, height = img.size
    
    return {"width": width, "height": height}


def img_get_format(image_id):
    """
    Get image format.
    
    Args:
        image_id: Image ID
    
    Returns:
        Image format (JPEG, PNG, etc.)
    """
    _check_pil()
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    return img.format or "Unknown"


def img_get_mode(image_id):
    """
    Get image color mode.
    
    Args:
        image_id: Image ID
    
    Returns:
        Color mode (RGB, RGBA, L, etc.)
    """
    _check_pil()
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    return img.mode


# ============================================================================
# Image Transformations
# ============================================================================

def img_resize(image_id, width, height, resample="lanczos"):
    """
    Resize image to specified dimensions.
    
    Args:
        image_id: Image ID
        width: New width
        height: New height
        resample: Resampling filter (lanczos, bilinear, nearest)
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    
    # Map resample methods
    resample_map = {
        "lanczos": Image.Resampling.LANCZOS,
        "bilinear": Image.Resampling.BILINEAR,
        "nearest": Image.Resampling.NEAREST,
    }
    resample_method = resample_map.get(resample.lower(), Image.Resampling.LANCZOS)
    
    # Resize
    new_img = img.resize((int(width), int(height)), resample_method)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_crop(image_id, left, top, right, bottom):
    """
    Crop image to specified rectangle.
    
    Args:
        image_id: Image ID
        left: Left coordinate
        top: Top coordinate
        right: Right coordinate
        bottom: Bottom coordinate
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.crop((left, top, right, bottom))
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_rotate(image_id, angle, expand=False):
    """
    Rotate image by specified angle.
    
    Args:
        image_id: Image ID
        angle: Rotation angle in degrees (counter-clockwise)
        expand: Expand image to fit rotated content
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.rotate(angle, expand=expand)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_flip_horizontal(image_id):
    """
    Flip image horizontally.
    
    Args:
        image_id: Image ID
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_flip_vertical(image_id):
    """
    Flip image vertically.
    
    Args:
        image_id: Image ID
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_convert(image_id, mode):
    """
    Convert image to different color mode.
    
    Args:
        image_id: Image ID
        mode: Target mode (RGB, RGBA, L for grayscale, 1 for binary)
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.convert(mode)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


# ============================================================================
# Image Filters & Enhancements
# ============================================================================

def img_blur(image_id, radius=2):
    """
    Apply Gaussian blur to image.
    
    Args:
        image_id: Image ID
        radius: Blur radius
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.filter(ImageFilter.GaussianBlur(radius))
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_sharpen(image_id):
    """
    Apply sharpening filter to image.
    
    Args:
        image_id: Image ID
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.filter(ImageFilter.SHARPEN)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_edge_detect(image_id):
    """
    Apply edge detection filter.
    
    Args:
        image_id: Image ID
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    new_img = img.filter(ImageFilter.FIND_EDGES)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_brightness(image_id, factor):
    """
    Adjust image brightness.
    
    Args:
        image_id: Image ID
        factor: Brightness factor (1.0 = original, <1 darker, >1 brighter)
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    enhancer = ImageEnhance.Brightness(img)
    new_img = enhancer.enhance(factor)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


def img_contrast(image_id, factor):
    """
    Adjust image contrast.
    
    Args:
        image_id: Image ID
        factor: Contrast factor (1.0 = original, <1 less, >1 more)
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id]
    enhancer = ImageEnhance.Contrast(img)
    new_img = enhancer.enhance(factor)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = new_img
    _image_counter += 1
    
    return new_id


# ============================================================================
# Drawing Operations
# ============================================================================

def img_draw_text(image_id, text, x, y, color="#000000", size=20):
    """
    Draw text on image.
    
    Args:
        image_id: Image ID
        text: Text to draw
        x: X coordinate
        y: Y coordinate
        color: Text color (hex or name)
        size: Font size
    
    Returns:
        New image ID (modified copy)
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id].copy()
    draw = ImageDraw.Draw(img)
    
    # Use default font
    try:
        font = ImageFont.truetype("arial.ttf", size)
    except:
        font = ImageFont.load_default()
    
    draw.text((x, y), text, fill=color, font=font)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = img
    _image_counter += 1
    
    return new_id


def img_draw_rectangle(image_id, x1, y1, x2, y2, outline="#000000", fill=None, width=1):
    """
    Draw rectangle on image.
    
    Args:
        image_id: Image ID
        x1, y1: Top-left corner
        x2, y2: Bottom-right corner
        outline: Outline color
        fill: Fill color (None for no fill)
        width: Line width
    
    Returns:
        New image ID
    """
    _check_pil()
    global _image_counter
    
    if image_id not in _images:
        raise ValueError(f"Invalid image ID: {image_id}")
    
    img = _images[image_id].copy()
    draw = ImageDraw.Draw(img)
    
    draw.rectangle([(x1, y1), (x2, y2)], outline=outline, fill=fill, width=width)
    
    # Store new image
    new_id = _image_counter
    _images[new_id] = img
    _image_counter += 1
    
    return new_id


# ============================================================================
# Registration
# ============================================================================

def register_image_functions(runtime: Runtime) -> None:
    """Register image processing functions with the runtime."""
    
    # Loading & Saving
    runtime.register_function("img_load", img_load)
    runtime.register_function("img_create", img_create)
    runtime.register_function("img_save", img_save)
    runtime.register_function("img_close", img_close)
    
    # Information
    runtime.register_function("img_get_size", img_get_size)
    runtime.register_function("img_get_format", img_get_format)
    runtime.register_function("img_get_mode", img_get_mode)
    
    # Transformations
    runtime.register_function("img_resize", img_resize)
    runtime.register_function("img_crop", img_crop)
    runtime.register_function("img_rotate", img_rotate)
    runtime.register_function("img_flip_horizontal", img_flip_horizontal)
    runtime.register_function("img_flip_vertical", img_flip_vertical)
    runtime.register_function("img_convert", img_convert)
    
    # Filters & Enhancements
    runtime.register_function("img_blur", img_blur)
    runtime.register_function("img_sharpen", img_sharpen)
    runtime.register_function("img_edge_detect", img_edge_detect)
    runtime.register_function("img_brightness", img_brightness)
    runtime.register_function("img_contrast", img_contrast)
    
    # Drawing
    runtime.register_function("img_draw_text", img_draw_text)
    runtime.register_function("img_draw_rectangle", img_draw_rectangle)
