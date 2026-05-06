"""
NLPL Standard Library - Templates Module
String templating and text generation
"""

import re
from string import Template
from typing import Any, Dict, List, Optional


def register_template_functions(runtime: Any) -> None:
    """Register template functions with the runtime."""
    
    # Template rendering
    runtime.register_function("template_render", template_render)
    runtime.register_function("render_template", template_render)  # Alias
    runtime.register_function("template_substitute", template_substitute)
    runtime.register_function("template_safe_substitute", template_safe_substitute)
    
    # File operations
    runtime.register_function("load_template", load_template)
    runtime.register_function("render_template_file", render_template_file)
    
    # Advanced templating
    runtime.register_function("template_format", template_format)
    runtime.register_function("template_replace", template_replace)
    runtime.register_function("template_loop", template_loop)


# =======================
# Basic Templating
# =======================

def template_render(template_str: str, variables: Dict[str, Any]) -> str:
    """
    Render template with variables using ${variable} syntax.
    Raises error if variable is missing.
    """
    try:
        template = Template(template_str)
        return template.substitute(variables)
    except KeyError as e:
        raise RuntimeError(f"Missing template variable: {e}")
    except Exception as e:
        raise RuntimeError(f"Template rendering error: {e}")


def template_substitute(template_str: str, variables: Dict[str, Any]) -> str:
    """Alias for template_render."""
    return template_render(template_str, variables)


def template_safe_substitute(template_str: str, variables: Dict[str, Any]) -> str:
    """
    Render template with variables using ${variable} syntax.
    Missing variables are left as-is (no error).
    """
    try:
        template = Template(template_str)
        return template.safe_substitute(variables)
    except Exception as e:
        raise RuntimeError(f"Template rendering error: {e}")


# =======================
# File Operations
# =======================

def load_template(filepath: str) -> str:
    """Load template from file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except FileNotFoundError:
        raise RuntimeError(f"Template file not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error loading template: {e}")


def render_template_file(filepath: str, variables: Dict[str, Any]) -> str:
    """Load and render template file."""
    try:
        template_str = load_template(filepath)
        return template_render(template_str, variables)
    except Exception as e:
        raise RuntimeError(f"Error rendering template file: {e}")


# =======================
# Advanced Templating
# =======================

def template_format(template_str: str, **kwargs: Any) -> str:
    """
    Render template using {variable} syntax (Python str.format).
    More flexible than ${variable} syntax.
    """
    try:
        return template_str.format(**kwargs)
    except KeyError as e:
        raise RuntimeError(f"Missing template variable: {e}")
    except Exception as e:
        raise RuntimeError(f"Template formatting error: {e}")


def template_replace(template_str: str, replacements: Dict[str, str]) -> str:
    """
    Simple find-and-replace in template.
    Each key in replacements is replaced with its value.
    """
    try:
        result = template_str
        for key, value in replacements.items():
            result = result.replace(key, str(value))
        return result
    except Exception as e:
        raise RuntimeError(f"Template replace error: {e}")


def template_loop(template_str: str, items: List[Dict[str, Any]], 
                  item_separator: str = "\n") -> str:
    """
    Render template multiple times for list of items.
    Each item dict is used to render the template once.
    Results are joined with separator.
    """
    try:
        results = []
        for item in items:
            rendered = template_render(template_str, item)
            results.append(rendered)
        return item_separator.join(results)
    except Exception as e:
        raise RuntimeError(f"Template loop error: {e}")


# =======================
# Custom Template Engine (Simple Jinja-like)
# =======================

class SimpleTemplate:
    """
    Simple template engine with basic Jinja2-like syntax.
    Supports: {{ variable }}, {% if condition %}, {% for item in list %}
    """
    
    def __init__(self, template_str: str):
        self.template = template_str
    
    def render(self, context: Dict[str, Any]) -> str:
        """Render template with context variables."""
        result = self.template
        
        # Replace variables: {{ variable }}
        var_pattern = r'\{\{\s*(\w+)\s*\}\}'
        def replace_var(match):
            var_name = match.group(1)
            return str(context.get(var_name, ''))
        
        result = re.sub(var_pattern, replace_var, result)
        
        # Handle simple if statements: {% if condition %} ... {% endif %}
        if_pattern = r'\{%\s*if\s+(\w+)\s*%\}(.*?)\{%\s*endif\s*%\}'
        def replace_if(match):
            condition_var = match.group(1)
            content = match.group(2)
            if context.get(condition_var):
                return content
            return ''
        
        result = re.sub(if_pattern, replace_if, result, flags=re.DOTALL)
        
        # Handle simple for loops: {% for item in items %} ... {% endfor %}
        for_pattern = r'\{%\s*for\s+(\w+)\s+in\s+(\w+)\s*%\}(.*?)\{%\s*endfor\s*%\}'
        def replace_for(match):
            item_name = match.group(1)
            list_name = match.group(2)
            content = match.group(3)
            
            items = context.get(list_name, [])
            results = []
            
            for item in items:
                # Create new context with loop item
                loop_context = context.copy()
                loop_context[item_name] = item
                
                # Render content with item context
                item_result = content
                var_pattern = r'\{\{\s*' + item_name + r'\.(\w+)\s*\}\}'
                def replace_item_var(m):
                    attr = m.group(1)
                    if isinstance(item, dict):
                        return str(item.get(attr, ''))
                    return str(getattr(item, attr, ''))
                
                item_result = re.sub(var_pattern, replace_item_var, item_result)
                results.append(item_result)
            
            return ''.join(results)
        
        result = re.sub(for_pattern, replace_for, result, flags=re.DOTALL)
        
        return result


def create_simple_template(template_str: str) -> SimpleTemplate:
    """Create a SimpleTemplate for variable, conditional, and loop rendering."""
    return SimpleTemplate(template_str)


def render_simple_template(template: SimpleTemplate, context: Dict[str, Any]) -> str:
    """Render SimpleTemplate with context."""
    return template.render(context)
