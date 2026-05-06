"""
NLPL Standard Library - XML Module
XML parsing, generation, and manipulation
"""

import xml.etree.ElementTree as ET
from xml.dom import minidom
from typing import Any, Dict, List, Optional


def register_xml_functions(runtime: Any) -> None:
    """Register XML functions with the runtime."""
    
    # Parsing
    runtime.register_function("parse_xml", parse_xml)
    runtime.register_function("xml_parse", parse_xml)  # Alias
    runtime.register_function("parse_xml_file", parse_xml_file)
    runtime.register_function("xml_from_string", parse_xml)  # Alias
    
    # Generation
    runtime.register_function("create_xml_element", create_xml_element)
    runtime.register_function("to_xml_string", to_xml_string)
    runtime.register_function("xml_to_string", to_xml_string)  # Alias
    runtime.register_function("pretty_xml", pretty_xml)
    
    # File operations
    runtime.register_function("write_xml_file", write_xml_file)
    runtime.register_function("read_xml_file", parse_xml_file)  # Alias
    
    # Element operations
    runtime.register_function("xml_find", xml_find)
    runtime.register_function("xml_findall", xml_findall)
    runtime.register_function("xml_get_text", xml_get_text)
    runtime.register_function("xml_set_text", xml_set_text)
    runtime.register_function("xml_get_attr", xml_get_attr)
    runtime.register_function("xml_set_attr", xml_set_attr)
    runtime.register_function("xml_add_child", xml_add_child)
    runtime.register_function("xml_remove_child", xml_remove_child)
    
    # Navigation
    runtime.register_function("xml_get_tag", xml_get_tag)
    runtime.register_function("xml_get_children", xml_get_children)
    runtime.register_function("xml_get_parent", xml_get_parent)
    
    # Validation
    runtime.register_function("is_valid_xml", is_valid_xml)


# =======================
# Parsing
# =======================

def parse_xml(xml_string: str) -> ET.Element:
    """Parse XML string to Element object."""
    try:
        return ET.fromstring(xml_string)
    except ET.ParseError as e:
        raise RuntimeError(f"Invalid XML: {e}")
    except Exception as e:
        raise RuntimeError(f"Error parsing XML: {e}")


def parse_xml_file(filepath: str) -> ET.Element:
    """Parse XML file to Element object."""
    try:
        tree = ET.parse(filepath)
        return tree.getroot()
    except FileNotFoundError:
        raise RuntimeError(f"File not found: {filepath}")
    except ET.ParseError as e:
        raise RuntimeError(f"Invalid XML in {filepath}: {e}")
    except Exception as e:
        raise RuntimeError(f"Error reading XML file {filepath}: {e}")


# =======================
# Generation
# =======================

def create_xml_element(tag: str, text: str = "", attributes: Optional[Dict[str, str]] = None) -> ET.Element:
    """Create new XML element with optional text and attributes."""
    try:
        elem = ET.Element(tag, attrib=attributes or {})
        if text:
            elem.text = text
        return elem
    except Exception as e:
        raise RuntimeError(f"Error creating XML element: {e}")


def to_xml_string(element: ET.Element, encoding: str = "unicode") -> str:
    """Convert Element to XML string."""
    try:
        return ET.tostring(element, encoding=encoding, method="xml")
    except Exception as e:
        raise RuntimeError(f"Error converting to XML string: {e}")


def pretty_xml(element: ET.Element) -> str:
    """Convert Element to pretty-printed XML string."""
    try:
        rough_string = ET.tostring(element, encoding="unicode")
        reparsed = minidom.parseString(rough_string)
        return reparsed.toprettyxml(indent="  ")
    except Exception as e:
        raise RuntimeError(f"Error creating pretty XML: {e}")


# =======================
# File Operations
# =======================

def write_xml_file(filepath: str, element: ET.Element, pretty: bool = True) -> bool:
    """Write Element to XML file."""
    try:
        # Create parent directories if needed
        import os
        parent = os.path.dirname(filepath)
        if parent and not os.path.exists(parent):
            os.makedirs(parent, exist_ok=True)
        
        if pretty:
            xml_string = pretty_xml(element)
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(xml_string)
        else:
            tree = ET.ElementTree(element)
            tree.write(filepath, encoding='utf-8', xml_declaration=True)
        
        return True
    except Exception as e:
        raise RuntimeError(f"Error writing XML file {filepath}: {e}")


# =======================
# Element Operations
# =======================

def xml_find(element: ET.Element, path: str) -> Optional[ET.Element]:
    """
    Find first matching element using XPath.
    Returns None if not found.
    """
    try:
        return element.find(path)
    except Exception as e:
        raise RuntimeError(f"Error finding XML element: {e}")


def xml_findall(element: ET.Element, path: str) -> List[ET.Element]:
    """Find all matching elements using XPath."""
    try:
        return element.findall(path)
    except Exception as e:
        raise RuntimeError(f"Error finding XML elements: {e}")


def xml_get_text(element: ET.Element) -> str:
    """Get text content of element."""
    return element.text or ""


def xml_set_text(element: ET.Element, text: str) -> bool:
    """Set text content of element."""
    try:
        element.text = text
        return True
    except Exception as e:
        raise RuntimeError(f"Error setting XML text: {e}")


def xml_get_attr(element: ET.Element, name: str, default: str = "") -> str:
    """Get attribute value from element."""
    return element.get(name, default)


def xml_set_attr(element: ET.Element, name: str, value: str) -> bool:
    """Set attribute on element."""
    try:
        element.set(name, value)
        return True
    except Exception as e:
        raise RuntimeError(f"Error setting XML attribute: {e}")


def xml_add_child(parent: ET.Element, child: ET.Element) -> bool:
    """Add child element to parent."""
    try:
        parent.append(child)
        return True
    except Exception as e:
        raise RuntimeError(f"Error adding XML child: {e}")


def xml_remove_child(parent: ET.Element, child: ET.Element) -> bool:
    """Remove child element from parent."""
    try:
        parent.remove(child)
        return True
    except Exception as e:
        raise RuntimeError(f"Error removing XML child: {e}")


# =======================
# Navigation
# =======================

def xml_get_tag(element: ET.Element) -> str:
    """Get tag name of element."""
    return element.tag


def xml_get_children(element: ET.Element) -> List[ET.Element]:
    """Get all direct children of element."""
    return list(element)


def xml_get_parent(element: ET.Element, root: ET.Element) -> Optional[ET.Element]:
    """
    Find parent of element within root tree.
    Returns None if not found or element is root.
    """
    try:
        for parent in root.iter():
            if element in parent:
                return parent
        return None
    except Exception:
        return None


# =======================
# Validation
# =======================

def is_valid_xml(xml_string: str) -> bool:
    """Check if string is valid XML."""
    try:
        ET.fromstring(xml_string)
        return True
    except (ET.ParseError, ValueError):
        return False
