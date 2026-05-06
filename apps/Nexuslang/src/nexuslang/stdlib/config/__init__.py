"""
Configuration file handling for NexusLang.
INI, YAML-like, and simple key-value config files.
"""

import configparser
import json
from typing import Dict, Any, Optional, List
from ...runtime.runtime import Runtime


class Config:
    """Configuration handler."""
    
    def __init__(self, config_type: str = "ini"):
        self.config_type = config_type
        self.data = {}
        if config_type == "ini":
            self.parser = configparser.ConfigParser()
    
    def load_file(self, filepath: str) -> bool:
        """Load configuration from file."""
        try:
            if self.config_type == "ini":
                self.parser.read(filepath)
                # Convert to dict
                for section in self.parser.sections():
                    self.data[section] = dict(self.parser.items(section))
                return True
            elif self.config_type == "json":
                with open(filepath, 'r') as f:
                    self.data = json.load(f)
                return True
            elif self.config_type == "simple":
                # Simple key=value format
                with open(filepath, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith('#'):
                            if '=' in line:
                                key, value = line.split('=', 1)
                                self.data[key.strip()] = value.strip()
                return True
            return False
        except Exception as e:
            print(f"Failed to load config: {e}")
            return False
    
    def save_file(self, filepath: str) -> bool:
        """Save configuration to file."""
        try:
            if self.config_type == "ini":
                with open(filepath, 'w') as f:
                    self.parser.write(f)
                return True
            elif self.config_type == "json":
                with open(filepath, 'w') as f:
                    json.dump(self.data, f, indent=2)
                return True
            elif self.config_type == "simple":
                with open(filepath, 'w') as f:
                    for key, value in self.data.items():
                        f.write(f"{key}={value}\n")
                return True
            return False
        except Exception as e:
            print(f"Failed to save config: {e}")
            return False
    
    def get(self, key: str, section: Optional[str] = None, default: Any = None) -> Any:
        """Get configuration value."""
        try:
            if self.config_type == "ini" and section:
                return self.data.get(section, {}).get(key, default)
            else:
                return self.data.get(key, default)
        except:
            return default
    
    def set(self, key: str, value: Any, section: Optional[str] = None) -> None:
        """Set configuration value."""
        if self.config_type == "ini" and section:
            if section not in self.data:
                self.data[section] = {}
                if section not in self.parser.sections():
                    self.parser.add_section(section)
            self.data[section][key] = str(value)
            self.parser.set(section, key, str(value))
        else:
            self.data[key] = value
    
    def has(self, key: str, section: Optional[str] = None) -> bool:
        """Check if key exists."""
        if self.config_type == "ini" and section:
            return section in self.data and key in self.data[section]
        return key in self.data
    
    def sections(self) -> List[str]:
        """Get all sections (for INI files)."""
        if self.config_type == "ini":
            return list(self.data.keys())
        return []
    
    def keys(self, section: Optional[str] = None) -> List[str]:
        """Get all keys in section or root."""
        if self.config_type == "ini" and section:
            return list(self.data.get(section, {}).keys())
        return list(self.data.keys())
    
    def as_dict(self) -> Dict:
        """Get all config as dictionary."""
        return self.data.copy()


def create_config(config_type: str = "ini") -> Config:
    """
    Create configuration handler.
    Types: 'ini', 'json', 'simple'
    """
    return Config(config_type)


def load_config_file(config: Config, filepath: str) -> bool:
    """Load configuration from file."""
    return config.load_file(filepath)


def save_config_file(config: Config, filepath: str) -> bool:
    """Save configuration to file."""
    return config.save_file(filepath)


def get_config_value(config: Config, key: str, section: Optional[str] = None, 
                     default: Any = None) -> Any:
    """Get configuration value."""
    return config.get(key, section, default)


def set_config_value(config: Config, key: str, value: Any, 
                     section: Optional[str] = None) -> None:
    """Set configuration value."""
    config.set(key, value, section)


def has_config_key(config: Config, key: str, section: Optional[str] = None) -> bool:
    """Check if configuration key exists."""
    return config.has(key, section)


def get_config_sections(config: Config) -> List[str]:
    """Get all configuration sections."""
    return config.sections()


def get_config_keys(config: Config, section: Optional[str] = None) -> List[str]:
    """Get all keys in section."""
    return config.keys(section)


def config_as_dict(config: Config) -> Dict:
    """Get configuration as dictionary."""
    return config.as_dict()


def register_config_functions(runtime: Runtime) -> None:
    """Register configuration functions with the runtime."""
    
    # Config management
    runtime.register_function("create_config", create_config)
    runtime.register_function("load_config_file", load_config_file)
    runtime.register_function("save_config_file", save_config_file)
    runtime.register_function("get_config_value", get_config_value)
    runtime.register_function("set_config_value", set_config_value)
    runtime.register_function("has_config_key", has_config_key)
    runtime.register_function("get_config_sections", get_config_sections)
    runtime.register_function("get_config_keys", get_config_keys)
    runtime.register_function("config_as_dict", config_as_dict)
