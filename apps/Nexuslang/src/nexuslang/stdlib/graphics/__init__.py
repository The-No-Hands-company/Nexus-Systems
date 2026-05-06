"""
Graphics module for NexusLang - Modern OpenGL/GLFW wrapper
Provides window management, shader compilation, VAO/VBO, and texture loading
"""

import ctypes
from typing import List, Tuple, Optional

try:
    import numpy as np
    import glfw
    from OpenGL.GL import *
    from OpenGL.GL.shaders import compileProgram, compileShader
    GLFW_AVAILABLE = True
except ImportError:
    np = None
    GLFW_AVAILABLE = False
    print("Warning: GLFW or PyOpenGL not installed. Graphics module functionality limited.")
    print("Install with: pip install glfw PyOpenGL numpy")
    # Stub out GL constants used as default argument values in class definitions
    # so that the module can be imported even without PyOpenGL installed.
    GL_STATIC_DRAW = 0x88B4
    GL_DYNAMIC_DRAW = 0x88E8
    GL_STREAM_DRAW = 0x88E0
    GL_ARRAY_BUFFER = 0x8892
    GL_ELEMENT_ARRAY_BUFFER = 0x8893
    GL_TRIANGLES = 0x0004
    GL_UNSIGNED_INT = 0x1405
    GL_FLOAT = 0x1406
    GL_TEXTURE_2D = 0x0DE1
    GL_TEXTURE0 = 0x84C0
    GL_LINEAR = 0x2601
    GL_REPEAT = 0x2901
    GL_RGBA = 0x1908
    GL_UNSIGNED_BYTE = 0x1401
    GL_COLOR_BUFFER_BIT = 0x4000
    GL_DEPTH_BUFFER_BIT = 0x0100
    GL_DEPTH_TEST = 0x0B71
    GL_BLEND = 0x0BE2
    GL_SRC_ALPHA = 0x0302
    GL_ONE_MINUS_SRC_ALPHA = 0x0303
    GL_VERTEX_SHADER = 0x8B31
    GL_FRAGMENT_SHADER = 0x8B30

class GraphicsWindow:
    """Represents an OpenGL window with GLFW"""
    
    def __init__(self, width, height, title):
        if not GLFW_AVAILABLE:
            raise RuntimeError("GLFW not available")
        
        if not glfw.init():
            raise RuntimeError("Failed to initialize GLFW")
        
        # Request OpenGL 3.3 core profile
        glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
        glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
        glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
        glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, GL_TRUE)
        
        self.window = glfw.create_window(width, height, title, None, None)
        if not self.window:
            glfw.terminate()
            raise RuntimeError("Failed to create window")
        
        glfw.make_context_current(self.window)
        glfw.show_window(self.window)   # ensure window is visible (some WMs need this)
        glfw.focus_window(self.window)  # bring to front
        glEnable(GL_DEPTH_TEST)  # Enable depth testing by default
        
        self.width = width
        self.height = height
        self.title = title
        
        # Input state tracking
        self.keys = {}  # Current key states
        self.keys_previous = {}  # Previous frame key states
        self.mouse_pos = (0.0, 0.0)  # Current mouse position
        self.mouse_pos_previous = (0.0, 0.0)  # Previous mouse position
        self.mouse_delta = (0.0, 0.0)  # Mouse movement delta
        self.mouse_buttons = {}  # Mouse button states
        self.mouse_buttons_previous = {}  # Previous button states
        self.scroll_offset = (0.0, 0.0)  # Scroll wheel offset
        self.first_mouse = True  # Flag for first mouse movement
        
        # Set up input callbacks
        glfw.set_key_callback(self.window, self._key_callback)
        glfw.set_cursor_pos_callback(self.window, self._cursor_pos_callback)
        glfw.set_mouse_button_callback(self.window, self._mouse_button_callback)
        glfw.set_scroll_callback(self.window, self._scroll_callback)
        
        # Disable cursor for FPS-style camera control (optional)
        # glfw.set_input_mode(self.window, glfw.CURSOR, glfw.CURSOR_DISABLED)
    
    def _key_callback(self, window, key, scancode, action, mods):
        """GLFW key callback"""
        if action == glfw.PRESS:
            self.keys[key] = True
        elif action == glfw.RELEASE:
            self.keys[key] = False
    
    def _cursor_pos_callback(self, window, xpos, ypos):
        """GLFW cursor position callback"""
        if self.first_mouse:
            self.mouse_pos_previous = (xpos, ypos)
            self.first_mouse = False
        
        self.mouse_pos = (xpos, ypos)
        self.mouse_delta = (
            xpos - self.mouse_pos_previous[0],
            self.mouse_pos_previous[1] - ypos  # Reversed Y
        )
    
    def _mouse_button_callback(self, window, button, action, mods):
        """GLFW mouse button callback"""
        if action == glfw.PRESS:
            self.mouse_buttons[button] = True
        elif action == glfw.RELEASE:
            self.mouse_buttons[button] = False
    
    def _scroll_callback(self, window, xoffset, yoffset):
        """GLFW scroll callback"""
        self.scroll_offset = (xoffset, yoffset)
    
    def update_input_state(self):
        """Update input state for next frame"""
        self.keys_previous = self.keys.copy()
        self.mouse_pos_previous = self.mouse_pos
        self.mouse_buttons_previous = self.mouse_buttons.copy()
        self.mouse_delta = (0.0, 0.0)  # Reset delta
        self.scroll_offset = (0.0, 0.0)  # Reset scroll
    
    def is_key_pressed(self, key):
        """Check if key was just pressed this frame"""
        return self.keys.get(key, False) and not self.keys_previous.get(key, False)
    
    def is_key_held(self, key):
        """Check if key is currently held down"""
        return self.keys.get(key, False)
    
    def is_key_released(self, key):
        """Check if key was just released this frame"""
        return not self.keys.get(key, False) and self.keys_previous.get(key, False)
    
    def get_mouse_position(self):
        """Get current mouse position as tuple (x, y)"""
        return self.mouse_pos
    
    def get_mouse_delta(self):
        """Get mouse movement delta since last frame"""
        return self.mouse_delta
    
    def is_mouse_button_pressed(self, button):
        """Check if mouse button was just pressed this frame"""
        return self.mouse_buttons.get(button, False) and not self.mouse_buttons_previous.get(button, False)
    
    def is_mouse_button_held(self, button):
        """Check if mouse button is currently held down"""
        return self.mouse_buttons.get(button, False)
    
    def is_mouse_button_released(self, button):
        """Check if mouse button was just released this frame"""
        return not self.mouse_buttons.get(button, False) and self.mouse_buttons_previous.get(button, False)
    
    def get_scroll_offset(self):
        """Get scroll wheel offset this frame"""
        return self.scroll_offset
    
    def set_cursor_mode(self, mode):
        """Set cursor mode (normal, hidden, disabled)"""
        glfw.set_input_mode(self.window, glfw.CURSOR, mode)

    
    def should_close(self):
        """Check if window should close"""
        return glfw.window_should_close(self.window)
    
    def poll_events(self):
        """Process window events"""
        glfw.poll_events()
    
    def swap_buffers(self):
        """Swap front and back buffers"""
        glfw.swap_buffers(self.window)
    
    def get_key(self, key):
        """Get key state"""
        return glfw.get_key(self.window, key)
    
    def get_framebuffer_size(self):
        """Get framebuffer size (for retina displays)"""
        return glfw.get_framebuffer_size(self.window)
    
    def destroy(self):
        """Cleanup window"""
        glfw.destroy_window(self.window)
        glfw.terminate()


class Shader:
    """Represents a compiled shader program"""
    
    def __init__(self, vertex_src: str, fragment_src: str):
        """Compile and link vertex and fragment shaders"""
        try:
            vertex_shader = compileShader(vertex_src, GL_VERTEX_SHADER)
            fragment_shader = compileShader(fragment_src, GL_FRAGMENT_SHADER)
            self.program = compileProgram(vertex_shader, fragment_shader)
            glDeleteShader(vertex_shader)
            glDeleteShader(fragment_shader)
        except Exception as e:
            raise RuntimeError(f"Shader compilation failed: {e}")
    
    def use(self):
        """Activate this shader program"""
        glUseProgram(self.program)
    
    def set_uniform_mat4(self, name: str, matrix: List[float]):
        """Set a 4x4 matrix uniform"""
        location = glGetUniformLocation(self.program, name)
        if location == -1:
            print(f"Warning: uniform '{name}' not found in shader")
            return
        # Convert to numpy array and transpose (OpenGL uses column-major)
        mat_array = np.array(matrix, dtype=np.float32).reshape(4, 4).T
        glUniformMatrix4fv(location, 1, GL_FALSE, mat_array)
    
    def set_uniform_vec3(self, name: str, x: float, y: float, z: float):
        """Set a vec3 uniform"""
        location = glGetUniformLocation(self.program, name)
        if location == -1:
            print(f"Warning: uniform '{name}' not found in shader")
            return
        glUniform3f(location, x, y, z)
    
    def set_uniform_vec4(self, name: str, x: float, y: float, z: float, w: float):
        """Set a vec4 uniform"""
        location = glGetUniformLocation(self.program, name)
        if location == -1:
            print(f"Warning: uniform '{name}' not found in shader")
            return
        glUniform4f(location, x, y, z, w)
    
    def set_uniform_float(self, name: str, value: float):
        """Set a float uniform"""
        location = glGetUniformLocation(self.program, name)
        if location == -1:
            print(f"Warning: uniform '{name}' not found in shader")
            return
        glUniform1f(location, value)
    
    def set_uniform_int(self, name: str, value: int):
        """Set an int uniform"""
        location = glGetUniformLocation(self.program, name)
        if location == -1:
            print(f"Warning: uniform '{name}' not found in shader")
            return
        glUniform1i(location, value)
    
    def delete(self):
        """Delete shader program"""
        glDeleteProgram(self.program)


class VertexBuffer:
    """Represents a Vertex Buffer Object (VBO)"""
    
    def __init__(self, vertices: List[float], usage=GL_STATIC_DRAW):
        """Create VBO from vertex data"""
        self.vbo = glGenBuffers(1)
        self.vertex_count = len(vertices)
        
        # Convert to numpy array
        vertex_array = np.array(vertices, dtype=np.float32)
        
        glBindBuffer(GL_ARRAY_BUFFER, self.vbo)
        glBufferData(GL_ARRAY_BUFFER, vertex_array.nbytes, vertex_array, usage)
        glBindBuffer(GL_ARRAY_BUFFER, 0)
    
    def bind(self):
        """Bind this VBO"""
        glBindBuffer(GL_ARRAY_BUFFER, self.vbo)
    
    def unbind(self):
        """Unbind VBO"""
        glBindBuffer(GL_ARRAY_BUFFER, 0)
    
    def delete(self):
        """Delete VBO"""
        glDeleteBuffers(1, [self.vbo])


class IndexBuffer:
    """Represents an Element Buffer Object (EBO/IBO)"""
    
    def __init__(self, indices: List[int], usage=GL_STATIC_DRAW):
        """Create EBO from index data"""
        self.ebo = glGenBuffers(1)
        self.index_count = len(indices)
        
        # Convert to numpy array
        index_array = np.array(indices, dtype=np.uint32)
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self.ebo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_array.nbytes, index_array, usage)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
    
    def bind(self):
        """Bind this EBO"""
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self.ebo)
    
    def unbind(self):
        """Unbind EBO"""
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
    
    def delete(self):
        """Delete EBO"""
        glDeleteBuffers(1, [self.ebo])


class VertexArray:
    """Represents a Vertex Array Object (VAO)"""
    
    def __init__(self):
        """Create VAO"""
        self.vao = glGenVertexArrays(1)
        self.vbo = None
        self.ebo = None
    
    def bind(self):
        """Bind this VAO"""
        glBindVertexArray(self.vao)
    
    def unbind(self):
        """Unbind VAO"""
        glBindVertexArray(0)
    
    def set_vertex_buffer(self, vbo: VertexBuffer):
        """Attach VBO to this VAO"""
        self.vbo = vbo
    
    def set_index_buffer(self, ebo: IndexBuffer):
        """Attach EBO to this VAO"""
        self.ebo = ebo
    
    def set_attribute(self, index: int, size: int, stride: int, offset: int):
        """Configure vertex attribute pointer
        
        Args:
            index: Attribute location in shader
            size: Number of components (1-4)
            stride: Bytes between consecutive attributes
            offset: Bytes from start of buffer to first attribute
        """
        self.bind()
        if self.vbo:
            self.vbo.bind()
        glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(offset))
        glEnableVertexAttribArray(index)
        self.unbind()
    
    def draw_arrays(self, mode, first: int, count: int):
        """Draw using vertex arrays"""
        self.bind()
        glDrawArrays(mode, first, count)
        self.unbind()
    
    def draw_elements(self, mode, count: int):
        """Draw using indexed rendering"""
        self.bind()
        if self.ebo:
            self.ebo.bind()
        glDrawElements(mode, count, GL_UNSIGNED_INT, None)
        self.unbind()
    
    def delete(self):
        """Delete VAO and associated buffers"""
        if self.vbo:
            self.vbo.delete()
        if self.ebo:
            self.ebo.delete()
        glDeleteVertexArrays(1, [self.vao])


class Texture:
    """Represents an OpenGL texture"""
    
    def __init__(self, width: int, height: int, data: Optional[bytes] = None):
        """Create texture
        
        Args:
            width: Texture width
            height: Texture height
            data: Optional RGBA pixel data (width*height*4 bytes)
        """
        self.texture = glGenTextures(1)
        self.width = width
        self.height = height
        
        glBindTexture(GL_TEXTURE_2D, self.texture)
        
        # Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        
        if data:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                        GL_RGBA, GL_UNSIGNED_BYTE, data)
            glGenerateMipmap(GL_TEXTURE_2D)
        else:
            # Create empty texture
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, None)
        
        glBindTexture(GL_TEXTURE_2D, 0)
    
    def bind(self, unit: int = 0):
        """Bind texture to texture unit"""
        glActiveTexture(GL_TEXTURE0 + unit)
        glBindTexture(GL_TEXTURE_2D, self.texture)
    
    def unbind(self):
        """Unbind texture"""
        glBindTexture(GL_TEXTURE_2D, 0)
    
    def delete(self):
        """Delete texture"""
        glDeleteTextures(1, [self.texture])


class Cubemap:
    """Represents an OpenGL cubemap texture for skybox"""
    
    def __init__(self, faces_data: List[Tuple[int, int, bytes]]):
        """Create cubemap from 6 face images
        
        Args:
            faces_data: List of 6 tuples (width, height, rgba_bytes) in order:
                        [+X (right), -X (left), +Y (top), -Y (bottom), +Z (front), -Z (back)]
        """
        self.texture = glGenTextures(1)
        glBindTexture(GL_TEXTURE_CUBE_MAP, self.texture)
        
        # Define cubemap face order (OpenGL convention)
        faces = [
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,  # Right
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,  # Left
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,  # Top
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,  # Bottom
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,  # Front
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z   # Back
        ]
        
        # Upload each face
        for i, (width, height, data) in enumerate(faces_data):
            glTexImage2D(faces[i], 0, GL_RGBA, width, height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, data)
        
        # Set texture parameters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE)
        
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0)
    
    def bind(self, unit: int = 0):
        """Bind cubemap to texture unit"""
        glActiveTexture(GL_TEXTURE0 + unit)
        glBindTexture(GL_TEXTURE_CUBE_MAP, self.texture)
    
    def unbind(self):
        """Unbind cubemap"""
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0)
    
    def delete(self):
        """Delete cubemap"""
        glDeleteTextures(1, [self.texture])


class Framebuffer:
    """Represents an OpenGL framebuffer for offscreen rendering"""
    
    def __init__(self, width: int, height: int):
        """Create framebuffer
        
        Args:
            width: Framebuffer width
            height: Framebuffer height
        """
        self.fbo = glGenFramebuffers(1)
        self.width = width
        self.height = height
        self.depth_texture = None
        self.color_texture = None
        
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
    def attach_depth_texture(self, texture_obj):
        """Attach depth texture to framebuffer"""
        self.depth_texture = texture_obj
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                              GL_TEXTURE_2D, texture_obj.texture, 0)
        
        # For depth-only rendering, disable color output
        glDrawBuffer(GL_NONE)
        glReadBuffer(GL_NONE)
        
        # Check framebuffer completeness
        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            raise RuntimeError("Framebuffer is not complete")
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
    def attach_color_texture(self, texture_obj):
        """Attach color texture to framebuffer"""
        self.color_texture = texture_obj
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture_obj.texture, 0)
        
        # Check framebuffer completeness
        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            raise RuntimeError("Framebuffer is not complete")
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
    def bind(self):
        """Bind framebuffer for rendering"""
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glViewport(0, 0, self.width, self.height)
    
    def unbind(self):
        """Unbind framebuffer (return to default)"""
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
    def delete(self):
        """Delete framebuffer"""
        glDeleteFramebuffers(1, [self.fbo])


class GraphicsModule:
    """Main graphics module for NLPL"""
    
    def __init__(self):
        self.windows = {}
        self.shaders = {}
        self.vaos = {}
        self.vbos = {}
        self.ebos = {}
        self.textures = {}
        self.cubemaps = {}
        self.framebuffers = {}
        self.animation_tracks = {}
        
        self.next_window_id = 1
        self.next_shader_id = 1
        self.next_vao_id = 1
        self.next_vbo_id = 1
        self.next_ebo_id = 1
        self.next_texture_id = 1
        self.next_cubemap_id = 1
        self.next_framebuffer_id = 1
        self.next_animation_track_id = 1
        
        self.current_window = None
        self.current_shader = None
    
    # ===== Window Management =====
    
    def create_window(self, width: int, height: int, title: str) -> int:
        """Create a new window and return its ID"""
        window = GraphicsWindow(width, height, title)
        window_id = self.next_window_id
        self.windows[window_id] = window
        self.current_window = window
        self.next_window_id += 1
        return window_id
    
    def window_should_close(self, window_id: int) -> bool:
        """Check if window should close"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].should_close()
    
    def get_key(self, window_id: int, key: int) -> int:
        """Get key state (GLFW key codes)"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].get_key(key)
    
    def get_framebuffer_size(self, window_id: int) -> Tuple[int, int]:
        """Get framebuffer size"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].get_framebuffer_size()
    
    def destroy_window(self, window_id: int):
        """Destroy a window"""
        if window_id in self.windows:
            self.windows[window_id].destroy()
            del self.windows[window_id]
    
    # ===== Input Management =====
    
    def update_input(self, window_id: int):
        """Update input state - call at end of frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        self.windows[window_id].update_input_state()
    
    def is_key_pressed(self, window_id: int, key: int) -> bool:
        """Check if key was just pressed this frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].is_key_pressed(key)
    
    def is_key_held(self, window_id: int, key: int) -> bool:
        """Check if key is currently held down"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].is_key_held(key)
    
    def is_key_released(self, window_id: int, key: int) -> bool:
        """Check if key was just released this frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].is_key_released(key)
    
    def get_mouse_position(self, window_id: int) -> Tuple[float, float]:
        """Get current mouse position"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].get_mouse_position()
    
    def get_mouse_delta(self, window_id: int) -> Tuple[float, float]:
        """Get mouse movement delta since last frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].get_mouse_delta()
    
    def get_mouse_delta_x(self, window_id: int) -> float:
        """Get mouse X movement delta since last frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        delta = self.windows[window_id].get_mouse_delta()
        return delta[0]
    
    def get_mouse_delta_y(self, window_id: int) -> float:
        """Get mouse Y movement delta since last frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        delta = self.windows[window_id].get_mouse_delta()
        return delta[1]
    
    def is_mouse_button_pressed(self, window_id: int, button: int) -> bool:
        """Check if mouse button was just pressed this frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].is_mouse_button_pressed(button)
    
    def is_mouse_button_held(self, window_id: int, button: int) -> bool:
        """Check if mouse button is currently held down"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].is_mouse_button_held(button)
    
    def is_mouse_button_released(self, window_id: int, button: int) -> bool:
        """Check if mouse button was just released this frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].is_mouse_button_released(button)
    
    def get_scroll_offset(self, window_id: int) -> Tuple[float, float]:
        """Get scroll wheel offset this frame"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        return self.windows[window_id].get_scroll_offset()
    
    def set_cursor_mode(self, window_id: int, mode: int):
        """Set cursor mode (GLFW_CURSOR_NORMAL, GLFW_CURSOR_HIDDEN, GLFW_CURSOR_DISABLED)"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        self.windows[window_id].set_cursor_mode(mode)

    
    def begin_frame(self):
        """Begin a new frame - clear buffers"""
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    
    def end_frame(self, window_id: int):
        """End frame - swap buffers and poll events"""
        if window_id not in self.windows:
            raise ValueError(f"Invalid window ID: {window_id}")
        window = self.windows[window_id]
        window.swap_buffers()
        window.poll_events()
    
    def set_clear_color(self, r: float, g: float, b: float, a: float = 1.0):
        """Set the clear color"""
        glClearColor(r, g, b, a)
    
    def set_viewport(self, x: int, y: int, width: int, height: int):
        """Set viewport"""
        glViewport(x, y, width, height)
    
    # ===== Shader Management =====
    
    def create_shader(self, vertex_src: str, fragment_src: str) -> int:
        """Compile shader program and return its ID"""
        shader = Shader(vertex_src, fragment_src)
        shader_id = self.next_shader_id
        self.shaders[shader_id] = shader
        self.next_shader_id += 1
        return shader_id
    
    def load_shader_from_files(self, vertex_path: str, fragment_path: str) -> int:
        """Load and compile shaders from separate .glsl files
        
        Args:
            vertex_path: Path to vertex shader file (.vert or .glsl)
            fragment_path: Path to fragment shader file (.frag or .glsl)
        
        Returns:
            Shader ID
        
        Example:
            set shader to load_shader_from_files with "shaders/skybox.vert" and "shaders/skybox.frag"
        """
        try:
            with open(vertex_path, 'r') as f:
                vertex_src = f.read()
        except FileNotFoundError:
            raise RuntimeError(f"Vertex shader file not found: {vertex_path}")
        except Exception as e:
            raise RuntimeError(f"Error reading vertex shader {vertex_path}: {e}")
        
        try:
            with open(fragment_path, 'r') as f:
                fragment_src = f.read()
        except FileNotFoundError:
            raise RuntimeError(f"Fragment shader file not found: {fragment_path}")
        except Exception as e:
            raise RuntimeError(f"Error reading fragment shader {fragment_path}: {e}")
        
        return self.create_shader(vertex_src, fragment_src)
    
    def use_shader(self, shader_id: int):
        """Activate shader program"""
        if shader_id not in self.shaders:
            raise ValueError(f"Invalid shader ID: {shader_id}")
        self.shaders[shader_id].use()
        self.current_shader = self.shaders[shader_id]
    
    def set_uniform_mat4(self, shader_id: int, name: str, matrix: List[float]):
        """Set 4x4 matrix uniform"""
        if shader_id not in self.shaders:
            raise ValueError(f"Invalid shader ID: {shader_id}")
        self.shaders[shader_id].set_uniform_mat4(name, matrix)
    
    def set_uniform_vec3(self, shader_id: int, name: str, x: float, y: float, z: float):
        """Set vec3 uniform"""
        if shader_id not in self.shaders:
            raise ValueError(f"Invalid shader ID: {shader_id}")
        self.shaders[shader_id].set_uniform_vec3(name, x, y, z)
    
    def set_uniform_vec4(self, shader_id: int, name: str, x: float, y: float, z: float, w: float):
        """Set vec4 uniform"""
        if shader_id not in self.shaders:
            raise ValueError(f"Invalid shader ID: {shader_id}")
        self.shaders[shader_id].set_uniform_vec4(name, x, y, z, w)
    
    def set_uniform_float(self, shader_id: int, name: str, value: float):
        """Set float uniform"""
        if shader_id not in self.shaders:
            raise ValueError(f"Invalid shader ID: {shader_id}")
        self.shaders[shader_id].set_uniform_float(name, value)
    
    def set_uniform_int(self, shader_id: int, name: str, value: int):
        """Set int uniform"""
        if shader_id not in self.shaders:
            raise ValueError(f"Invalid shader ID: {shader_id}")
        self.shaders[shader_id].set_uniform_int(name, value)
    
    def delete_shader(self, shader_id: int):
        """Delete shader program"""
        if shader_id in self.shaders:
            self.shaders[shader_id].delete()
            del self.shaders[shader_id]
    
    # ===== Animation System =====
    
    def lerp(self, start: float, end: float, t: float) -> float:
        """Linear interpolation between two values"""
        return start + (end - start) * t
    
    def lerp_vec3(self, start: List[float], end: List[float], t: float) -> List[float]:
        """Linear interpolation between two 3D vectors"""
        if len(start) != 3 or len(end) != 3:
            raise ValueError("lerp_vec3 requires two 3-component vectors")
        return [
            self.lerp(start[0], end[0], t),
            self.lerp(start[1], end[1], t),
            self.lerp(start[2], end[2], t)
        ]
    
    def cubic_hermite(self, p0: float, p1: float, m0: float, m1: float, t: float) -> float:
        """Cubic Hermite interpolation (smooth curves with tangent control)"""
        t2 = t * t
        t3 = t2 * t
        h00 = 2*t3 - 3*t2 + 1
        h10 = t3 - 2*t2 + t
        h01 = -2*t3 + 3*t2
        h11 = t3 - t2
        return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1
    
    def catmull_rom(self, p0: float, p1: float, p2: float, p3: float, t: float) -> float:
        """Catmull-Rom spline interpolation (smooth curves through points)"""
        # Tangents calculated from neighboring points
        m1 = 0.5 * (p2 - p0)
        m2 = 0.5 * (p3 - p1)
        return self.cubic_hermite(p1, p2, m1, m2, t)
    
    def catmull_rom_vec3(self, p0: List[float], p1: List[float], p2: List[float], 
                          p3: List[float], t: float) -> List[float]:
        """Catmull-Rom spline interpolation for 3D vectors"""
        if len(p0) != 3 or len(p1) != 3 or len(p2) != 3 or len(p3) != 3:
            raise ValueError("catmull_rom_vec3 requires four 3-component vectors")
        return [
            self.catmull_rom(p0[0], p1[0], p2[0], p3[0], t),
            self.catmull_rom(p0[1], p1[1], p2[1], p3[1], t),
            self.catmull_rom(p0[2], p1[2], p2[2], p3[2], t)
        ]
    
    def ease_linear(self, t: float) -> float:
        """Linear easing (no acceleration)"""
        return max(0.0, min(1.0, t))
    
    def ease_in_quad(self, t: float) -> float:
        """Quadratic ease-in (slow start)"""
        t = max(0.0, min(1.0, t))
        return t * t
    
    def ease_out_quad(self, t: float) -> float:
        """Quadratic ease-out (slow end)"""
        t = max(0.0, min(1.0, t))
        return t * (2.0 - t)
    
    def ease_in_out_quad(self, t: float) -> float:
        """Quadratic ease-in-out (slow start and end)"""
        t = max(0.0, min(1.0, t))
        if t < 0.5:
            return 2.0 * t * t
        else:
            return -1.0 + (4.0 - 2.0 * t) * t
    
    def ease_in_cubic(self, t: float) -> float:
        """Cubic ease-in (slower start)"""
        t = max(0.0, min(1.0, t))
        return t * t * t
    
    def ease_out_cubic(self, t: float) -> float:
        """Cubic ease-out (slower end)"""
        t = max(0.0, min(1.0, t))
        t = 1.0 - t
        return 1.0 - (t * t * t)
    
    def ease_in_out_cubic(self, t: float) -> float:
        """Cubic ease-in-out (slower start and end)"""
        t = max(0.0, min(1.0, t))
        if t < 0.5:
            return 4.0 * t * t * t
        else:
            p = 2.0 * t - 2.0
            return 1.0 + 0.5 * p * p * p
    
    def ease_in_out_sine(self, t: float) -> float:
        """Sine ease-in-out (smooth acceleration)"""
        import math
        t = max(0.0, min(1.0, t))
        return 0.5 * (1.0 - math.cos(t * math.pi))
    
    def create_animation_track(self) -> int:
        """Create a new animation track and return its ID"""
        track_id = self.next_animation_track_id
        self.animation_tracks[track_id] = {
            'keyframes': [],
            'playing': False,
            'loop': False,
            'current_time': 0.0,
            'duration': 0.0
        }
        self.next_animation_track_id += 1
        return track_id
    
    def add_keyframe(self, track_id: int, time: float, value: List[float]):
        """Add a keyframe to an animation track (value can be position, rotation, scale, etc.)"""
        if track_id not in self.animation_tracks:
            raise ValueError(f"Invalid animation track ID: {track_id}")
        
        track = self.animation_tracks[track_id]
        keyframe = {'time': time, 'value': value}
        
        # Insert keyframe in chronological order
        inserted = False
        for i, kf in enumerate(track['keyframes']):
            if kf['time'] > time:
                track['keyframes'].insert(i, keyframe)
                inserted = True
                break
        
        if not inserted:
            track['keyframes'].append(keyframe)
        
        # Update duration
        if track['keyframes']:
            track['duration'] = track['keyframes'][-1]['time']
    
    def animation_play(self, track_id: int, loop: bool = False):
        """Start playing an animation track"""
        if track_id not in self.animation_tracks:
            raise ValueError(f"Invalid animation track ID: {track_id}")
        
        track = self.animation_tracks[track_id]
        track['playing'] = True
        track['loop'] = loop
        track['current_time'] = 0.0
    
    def animation_stop(self, track_id: int):
        """Stop playing an animation track"""
        if track_id not in self.animation_tracks:
            raise ValueError(f"Invalid animation track ID: {track_id}")
        
        track = self.animation_tracks[track_id]
        track['playing'] = False
        track['current_time'] = 0.0
    
    def animation_update(self, track_id: int, delta_time: float) -> List[float]:
        """Update animation and return current interpolated value"""
        if track_id not in self.animation_tracks:
            raise ValueError(f"Invalid animation track ID: {track_id}")
        
        track = self.animation_tracks[track_id]
        
        if not track['playing'] or len(track['keyframes']) == 0:
            return track['keyframes'][0]['value'] if track['keyframes'] else [0.0, 0.0, 0.0]
        
        # Update time
        track['current_time'] += delta_time
        
        # Handle loop
        if track['current_time'] > track['duration']:
            if track['loop']:
                track['current_time'] = track['current_time'] % track['duration']
            else:
                track['playing'] = False
                track['current_time'] = track['duration']
        
        # Find keyframe pair to interpolate between
        keyframes = track['keyframes']
        current_time = track['current_time']
        
        # Before first keyframe
        if current_time <= keyframes[0]['time']:
            return keyframes[0]['value']
        
        # After last keyframe
        if current_time >= keyframes[-1]['time']:
            return keyframes[-1]['value']
        
        # Find surrounding keyframes
        for i in range(len(keyframes) - 1):
            kf1 = keyframes[i]
            kf2 = keyframes[i + 1]
            
            if kf1['time'] <= current_time <= kf2['time']:
                # Linear interpolation
                segment_duration = kf2['time'] - kf1['time']
                local_t = (current_time - kf1['time']) / segment_duration
                
                # Interpolate each component
                result = []
                for j in range(len(kf1['value'])):
                    result.append(self.lerp(kf1['value'][j], kf2['value'][j], local_t))
                
                return result
        
        return keyframes[-1]['value']
    
    def animation_evaluate_at(self, track_id: int, time: float, 
                              interpolation: str = "linear") -> List[float]:
        """Evaluate animation at specific time with chosen interpolation method"""
        if track_id not in self.animation_tracks:
            raise ValueError(f"Invalid animation track ID: {track_id}")
        
        track = self.animation_tracks[track_id]
        keyframes = track['keyframes']
        
        if len(keyframes) == 0:
            return [0.0, 0.0, 0.0]
        
        # Clamp time to track duration
        time = max(0.0, min(time, track['duration']))
        
        # Before first keyframe
        if time <= keyframes[0]['time']:
            return keyframes[0]['value']
        
        # After last keyframe
        if time >= keyframes[-1]['time']:
            return keyframes[-1]['value']
        
        # Find surrounding keyframes
        for i in range(len(keyframes) - 1):
            kf1 = keyframes[i]
            kf2 = keyframes[i + 1]
            
            if kf1['time'] <= time <= kf2['time']:
                segment_duration = kf2['time'] - kf1['time']
                local_t = (time - kf1['time']) / segment_duration
                
                if interpolation == "catmull_rom" and len(keyframes) >= 4:
                    # Use Catmull-Rom if we have enough keyframes
                    kf0 = keyframes[max(0, i - 1)]
                    kf3 = keyframes[min(len(keyframes) - 1, i + 2)]
                    return self.catmull_rom_vec3(kf0['value'], kf1['value'], 
                                                 kf2['value'], kf3['value'], local_t)
                else:
                    # Default to linear interpolation
                    result = []
                    for j in range(len(kf1['value'])):
                        result.append(self.lerp(kf1['value'][j], kf2['value'][j], local_t))
                    return result
        
        return keyframes[-1]['value']
    
    def delete_animation_track(self, track_id: int):
        """Delete an animation track"""
        if track_id in self.animation_tracks:
            del self.animation_tracks[track_id]
    
    # ===== Matrix Utilities =====
    
    def create_identity_matrix(self) -> List[float]:
        """Create a 4x4 identity matrix (column-major)"""
        return [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        ]
    
    def create_translation_matrix(self, x: float, y: float, z: float) -> List[float]:
        """Create a translation matrix"""
        return [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            x,   y,   z,   1.0
        ]
    
    def create_scale_matrix(self, x: float, y: float, z: float) -> List[float]:
        """Create a scale matrix"""
        return [
            x,   0.0, 0.0, 0.0,
            0.0, y,   0.0, 0.0,
            0.0, 0.0, z,   0.0,
            0.0, 0.0, 0.0, 1.0
        ]
    
    def create_rotation_x_matrix(self, angle_deg: float) -> List[float]:
        """Create rotation matrix around X axis"""
        import math
        angle_rad = math.radians(angle_deg)
        c = math.cos(angle_rad)
        s = math.sin(angle_rad)
        return [
            1.0, 0.0, 0.0, 0.0,
            0.0, c,   s,   0.0,
            0.0, -s,  c,   0.0,
            0.0, 0.0, 0.0, 1.0
        ]
    
    def create_rotation_y_matrix(self, angle_deg: float) -> List[float]:
        """Create rotation matrix around Y axis"""
        import math
        angle_rad = math.radians(angle_deg)
        c = math.cos(angle_rad)
        s = math.sin(angle_rad)
        return [
            c,   0.0, -s,  0.0,
            0.0, 1.0, 0.0, 0.0,
            s,   0.0, c,   0.0,
            0.0, 0.0, 0.0, 1.0
        ]
    
    def create_rotation_z_matrix(self, angle_deg: float) -> List[float]:
        """Create rotation matrix around Z axis"""
        import math
        angle_rad = math.radians(angle_deg)
        c = math.cos(angle_rad)
        s = math.sin(angle_rad)
        return [
            c,   s,   0.0, 0.0,
            -s,  c,   0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        ]
    
    def create_view_matrix(self, eye: List[float], center: List[float], 
                          up: List[float]) -> List[float]:
        """Create a view (lookAt) matrix"""
        import math
        
        # Forward vector (from eye to center)
        fx = center[0] - eye[0]
        fy = center[1] - eye[1]
        fz = center[2] - eye[2]
        f_len = math.sqrt(fx*fx + fy*fy + fz*fz)
        fx /= f_len
        fy /= f_len
        fz /= f_len
        
        # Right vector (cross product of forward and up)
        rx = fy * up[2] - fz * up[1]
        ry = fz * up[0] - fx * up[2]
        rz = fx * up[1] - fy * up[0]
        r_len = math.sqrt(rx*rx + ry*ry + rz*rz)
        rx /= r_len
        ry /= r_len
        rz /= r_len
        
        # Up vector (cross product of right and forward)
        ux = ry * fz - rz * fy
        uy = rz * fx - rx * fz
        uz = rx * fy - ry * fx
        
        # Create view matrix
        return [
            rx,  ux, -fx, 0.0,
            ry,  uy, -fy, 0.0,
            rz,  uz, -fz, 0.0,
            -(rx*eye[0] + ry*eye[1] + rz*eye[2]),
            -(ux*eye[0] + uy*eye[1] + uz*eye[2]),
            -(-fx*eye[0] + -fy*eye[1] + -fz*eye[2]),
            1.0
        ]
    
    def create_perspective_matrix(self, fov_deg: float, aspect: float, 
                                  near: float, far: float) -> List[float]:
        """Create a perspective projection matrix"""
        import math
        fov_rad = math.radians(fov_deg)
        tan_half_fov = math.tan(fov_rad / 2.0)
        
        return [
            1.0 / (aspect * tan_half_fov), 0.0, 0.0, 0.0,
            0.0, 1.0 / tan_half_fov, 0.0, 0.0,
            0.0, 0.0, -(far + near) / (far - near), -1.0,
            0.0, 0.0, -(2.0 * far * near) / (far - near), 0.0
        ]
    
    def create_orthographic_matrix(self, left: float, right: float, 
                                   bottom: float, top: float,
                                   near: float, far: float) -> List[float]:
        """Create an orthographic projection matrix"""
        return [
            2.0 / (right - left), 0.0, 0.0, 0.0,
            0.0, 2.0 / (top - bottom), 0.0, 0.0,
            0.0, 0.0, -2.0 / (far - near), 0.0,
            -(right + left) / (right - left),
            -(top + bottom) / (top - bottom),
            -(far + near) / (far - near),
            1.0
        ]
    
    def multiply_matrices(self, a: List[float], b: List[float]) -> List[float]:
        """Multiply two 4x4 matrices (column-major order)"""
        result = [0.0] * 16
        for i in range(4):
            for j in range(4):
                for k in range(4):
                    result[i * 4 + j] += a[k * 4 + j] * b[i * 4 + k]
        return result
    
    # ===== Buffer Management =====
    
    def create_vertex_buffer(self, vertices: List[float]) -> int:
        """Create VBO and return its ID"""
        vbo = VertexBuffer(vertices)
        vbo_id = self.next_vbo_id
        self.vbos[vbo_id] = vbo
        self.next_vbo_id += 1
        return vbo_id
    
    def create_index_buffer(self, indices: List[int]) -> int:
        """Create EBO and return its ID"""
        ebo = IndexBuffer(indices)
        ebo_id = self.next_ebo_id
        self.ebos[ebo_id] = ebo
        self.next_ebo_id += 1
        return ebo_id
    
    def delete_vertex_buffer(self, vbo_id: int):
        """Delete VBO"""
        if vbo_id in self.vbos:
            self.vbos[vbo_id].delete()
            del self.vbos[vbo_id]
    
    def delete_index_buffer(self, ebo_id: int):
        """Delete EBO"""
        if ebo_id in self.ebos:
            self.ebos[ebo_id].delete()
            del self.ebos[ebo_id]
    
    # ===== VAO Management =====
    
    def create_vertex_array(self) -> int:
        """Create VAO and return its ID"""
        vao = VertexArray()
        vao_id = self.next_vao_id
        self.vaos[vao_id] = vao
        self.next_vao_id += 1
        return vao_id
    
    def vao_set_vertex_buffer(self, vao_id: int, vbo_id: int):
        """Attach VBO to VAO"""
        if vao_id not in self.vaos:
            raise ValueError(f"Invalid VAO ID: {vao_id}")
        if vbo_id not in self.vbos:
            raise ValueError(f"Invalid VBO ID: {vbo_id}")
        self.vaos[vao_id].set_vertex_buffer(self.vbos[vbo_id])
    
    def vao_set_index_buffer(self, vao_id: int, ebo_id: int):
        """Attach EBO to VAO"""
        if vao_id not in self.vaos:
            raise ValueError(f"Invalid VAO ID: {vao_id}")
        if ebo_id not in self.ebos:
            raise ValueError(f"Invalid EBO ID: {ebo_id}")
        self.vaos[vao_id].set_index_buffer(self.ebos[ebo_id])
    
    def vao_set_attribute(self, vao_id: int, index: int, size: int, stride: int, offset: int):
        """Configure vertex attribute in VAO"""
        if vao_id not in self.vaos:
            raise ValueError(f"Invalid VAO ID: {vao_id}")
        self.vaos[vao_id].set_attribute(index, size, stride, offset)
    
    def vao_draw_arrays(self, vao_id: int, mode: int, first: int, count: int):
        """Draw using vertex arrays"""
        if vao_id not in self.vaos:
            raise ValueError(f"Invalid VAO ID: {vao_id}")
        self.vaos[vao_id].draw_arrays(mode, first, count)
    
    def vao_draw_elements(self, vao_id: int, mode: int, count: int):
        """Draw using indexed rendering"""
        if vao_id not in self.vaos:
            raise ValueError(f"Invalid VAO ID: {vao_id}")
        self.vaos[vao_id].draw_elements(mode, count)
    
    def delete_vertex_array(self, vao_id: int):
        """Delete VAO"""
        if vao_id in self.vaos:
            self.vaos[vao_id].delete()
            del self.vaos[vao_id]
    
    # ===== Texture Management =====
    
    def create_texture(self, width: int, height: int, data: Optional[bytes] = None) -> int:
        """Create texture and return its ID"""
        texture = Texture(width, height, data)
        texture_id = self.next_texture_id
        self.textures[texture_id] = texture
        self.next_texture_id += 1
        return texture_id
    
    def bind_texture(self, texture_id: int, unit: int = 0):
        """Bind texture to texture unit"""
        if texture_id not in self.textures:
            raise ValueError(f"Invalid texture ID: {texture_id}")
        self.textures[texture_id].bind(unit)
    
    def delete_texture(self, texture_id: int):
        """Delete texture"""
        if texture_id in self.textures:
            self.textures[texture_id].delete()
            del self.textures[texture_id]
    
    def load_texture_from_file(self, filepath: str) -> int:
        """Load texture from image file (PNG, JPEG, BMP, etc.)"""
        try:
            from PIL import Image
        except ImportError:
            raise RuntimeError("PIL/Pillow not installed. Install with: pip install Pillow")
        
        # Load and convert image to RGBA
        img = Image.open(filepath).convert('RGBA')
        width, height = img.size
        
        # Get pixel data as bytes (flipped vertically for OpenGL)
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
        data = img.tobytes()
        
        # Create texture
        return self.create_texture(width, height, data)
    
    # ===== Cubemap Management =====
    
    def create_cubemap(self, faces_data: List) -> int:
        """Create cubemap from 6 face data tuples
        
        Args:
            faces_data: List of 6 lists, each containing [width, height, bytes]
                       Order: [+X, -X, +Y, -Y, +Z, -Z]
        
        Returns:
            Cubemap ID
        """
        # Convert faces_data to proper format
        faces = []
        for face in faces_data:
            width = int(face[0])
            height = int(face[1])
            data = face[2]
            faces.append((width, height, data))
        
        cubemap = Cubemap(faces)
        cubemap_id = self.next_cubemap_id
        self.cubemaps[cubemap_id] = cubemap
        self.next_cubemap_id += 1
        return cubemap_id
    
    def load_cubemap_from_files(self, filepaths: List[str]) -> int:
        """Load cubemap from 6 image files
        
        Args:
            filepaths: List of 6 file paths in order:
                      [right, left, top, bottom, front, back]
                      [+X, -X, +Y, -Y, +Z, -Z]
        
        Returns:
            Cubemap ID
        """
        try:
            from PIL import Image
        except ImportError:
            raise RuntimeError("PIL/Pillow not installed. Install with: pip install Pillow")
        
        if len(filepaths) != 6:
            raise ValueError(f"Cubemap requires exactly 6 faces, got {len(filepaths)}")
        
        faces_data = []
        for filepath in filepaths:
            img = Image.open(filepath).convert('RGBA')
            width, height = img.size
            # No vertical flip for cubemaps
            data = img.tobytes()
            faces_data.append((width, height, data))
        
        return self.create_cubemap([[w, h, d] for w, h, d in faces_data])
    
    def bind_cubemap(self, cubemap_id: int, unit: int = 0):
        """Bind cubemap to texture unit"""
        if cubemap_id not in self.cubemaps:
            raise ValueError(f"Invalid cubemap ID: {cubemap_id}")
        self.cubemaps[cubemap_id].bind(unit)
    
    def delete_cubemap(self, cubemap_id: int):
        """Delete cubemap"""
        if cubemap_id in self.cubemaps:
            self.cubemaps[cubemap_id].delete()
            del self.cubemaps[cubemap_id]
    
    def create_skybox_geometry(self) -> int:
        """Create skybox cube VAO (36 vertices, position only)"""
        # Skybox cube vertices (large cube centered at origin)
        vertices = [
            # Positions (no normals/texcoords needed - use direction as texcoord)
            -1.0,  1.0, -1.0,
            -1.0, -1.0, -1.0,
             1.0, -1.0, -1.0,
             1.0, -1.0, -1.0,
             1.0,  1.0, -1.0,
            -1.0,  1.0, -1.0,
            
            -1.0, -1.0,  1.0,
            -1.0, -1.0, -1.0,
            -1.0,  1.0, -1.0,
            -1.0,  1.0, -1.0,
            -1.0,  1.0,  1.0,
            -1.0, -1.0,  1.0,
            
             1.0, -1.0, -1.0,
             1.0, -1.0,  1.0,
             1.0,  1.0,  1.0,
             1.0,  1.0,  1.0,
             1.0,  1.0, -1.0,
             1.0, -1.0, -1.0,
            
            -1.0, -1.0,  1.0,
            -1.0,  1.0,  1.0,
             1.0,  1.0,  1.0,
             1.0,  1.0,  1.0,
             1.0, -1.0,  1.0,
            -1.0, -1.0,  1.0,
            
            -1.0,  1.0, -1.0,
             1.0,  1.0, -1.0,
             1.0,  1.0,  1.0,
             1.0,  1.0,  1.0,
            -1.0,  1.0,  1.0,
            -1.0,  1.0, -1.0,
            
            -1.0, -1.0, -1.0,
            -1.0, -1.0,  1.0,
             1.0, -1.0, -1.0,
             1.0, -1.0, -1.0,
            -1.0, -1.0,  1.0,
             1.0, -1.0,  1.0
        ]
        
        vao = self.create_vertex_array()
        vbo = self.create_vertex_buffer(vertices)
        self.vao_set_vertex_buffer(vao, vbo)
        self.vao_set_attribute(vao, 0, 3, 12, 0)  # Position: 3 floats, stride 12 bytes
        
        return vao
    
    def create_depth_texture(self, width: int, height: int) -> int:
        """Create depth texture for shadow mapping"""
        texture_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, texture_id)
        
        # Depth texture parameters
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
                    GL_DEPTH_COMPONENT, GL_FLOAT, None)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER)
        
        # Border color (outside shadow map = not in shadow)
        border_color = [1.0, 1.0, 1.0, 1.0]
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color)
        
        glBindTexture(GL_TEXTURE_2D, 0)
        
        # Create texture object and store
        tex_obj = type('obj', (object,), {'texture': texture_id, 'width': width, 'height': height})()
        tex_id = self.next_texture_id
        self.textures[tex_id] = tex_obj
        self.next_texture_id += 1
        return tex_id
    
    # ===== Framebuffer Management =====
    
    def create_framebuffer(self, width: int, height: int) -> int:
        """Create framebuffer and return its ID"""
        fbo = Framebuffer(width, height)
        fbo_id = self.next_framebuffer_id
        self.framebuffers[fbo_id] = fbo
        self.next_framebuffer_id += 1
        return fbo_id
    
    def bind_framebuffer(self, fbo_id: int):
        """Bind framebuffer for rendering"""
        if fbo_id not in self.framebuffers:
            raise ValueError(f"Invalid framebuffer ID: {fbo_id}")
        self.framebuffers[fbo_id].bind()
    
    def unbind_framebuffer(self):
        """Unbind framebuffer (return to default framebuffer)"""
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
    def framebuffer_attach_depth_texture(self, fbo_id: int, texture_id: int):
        """Attach depth texture to framebuffer"""
        if fbo_id not in self.framebuffers:
            raise ValueError(f"Invalid framebuffer ID: {fbo_id}")
        if texture_id not in self.textures:
            raise ValueError(f"Invalid texture ID: {texture_id}")
        self.framebuffers[fbo_id].attach_depth_texture(self.textures[texture_id])
    
    def framebuffer_attach_color_texture(self, fbo_id: int, texture_id: int):
        """Attach color texture to framebuffer"""
        if fbo_id not in self.framebuffers:
            raise ValueError(f"Invalid framebuffer ID: {fbo_id}")
        if texture_id not in self.textures:
            raise ValueError(f"Invalid texture ID: {texture_id}")
        self.framebuffers[fbo_id].attach_color_texture(self.textures[texture_id])
    
    def delete_framebuffer(self, fbo_id: int):
        """Delete framebuffer"""
        if fbo_id in self.framebuffers:
            self.framebuffers[fbo_id].delete()
            del self.framebuffers[fbo_id]
    
    def set_viewport(self, x: int, y: int, width: int, height: int):
        """Set OpenGL viewport"""
        glViewport(x, y, width, height)
    
    # ===== Legacy Drawing (for backward compatibility) =====
    
    def draw_rect(self, x: float, y: float, width: float, height: float,
                  r: float, g: float, b: float, a: float = 1.0):
        """Draw a filled rectangle (legacy immediate mode - deprecated)"""
        glColor4f(r, g, b, a)
        glBegin(GL_QUADS)
        glVertex2f(x, y)
        glVertex2f(x + width, y)
        glVertex2f(x + width, y + height)
        glVertex2f(x, y + height)
        glEnd()
    
    # ===== Cleanup =====
    
    def cleanup(self):
        """Cleanup all resources"""
        for vao_id in list(self.vaos.keys()):
            self.delete_vertex_array(vao_id)
        for shader_id in list(self.shaders.keys()):
            self.delete_shader(shader_id)
        for texture_id in list(self.textures.keys()):
            self.delete_texture(texture_id)
        for cubemap_id in list(self.cubemaps.keys()):
            self.delete_cubemap(cubemap_id)
        for fbo_id in list(self.framebuffers.keys()):
            self.delete_framebuffer(fbo_id)
        for window_id in list(self.windows.keys()):
            self.destroy_window(window_id)


def _register_named_functions(runtime, functions):
    for name, func in functions:
        runtime.register_function(name, func)


def _register_graphics_core_functions(runtime, gfx):
    _register_named_functions(runtime, [
        ("create_window", gfx.create_window),
        ("window_should_close", gfx.window_should_close),
        ("destroy_window", gfx.destroy_window),
        ("get_key", gfx.get_key),
        ("get_framebuffer_size", gfx.get_framebuffer_size),
        ("update_input", gfx.update_input),
        ("is_key_pressed", gfx.is_key_pressed),
        ("is_key_held", gfx.is_key_held),
        ("is_key_released", gfx.is_key_released),
        ("get_mouse_position", gfx.get_mouse_position),
        ("get_mouse_delta", gfx.get_mouse_delta),
        ("get_mouse_delta_x", gfx.get_mouse_delta_x),
        ("get_mouse_delta_y", gfx.get_mouse_delta_y),
        ("is_mouse_button_pressed", gfx.is_mouse_button_pressed),
        ("is_mouse_button_held", gfx.is_mouse_button_held),
        ("is_mouse_button_released", gfx.is_mouse_button_released),
        ("get_scroll_offset", gfx.get_scroll_offset),
        ("set_cursor_mode", gfx.set_cursor_mode),
        ("begin_frame", gfx.begin_frame),
        ("end_frame", gfx.end_frame),
        ("set_clear_color", gfx.set_clear_color),
        ("set_viewport", gfx.set_viewport),
    ])


def _register_graphics_pipeline_functions(runtime, gfx):
    _register_named_functions(runtime, [
        ("create_shader", gfx.create_shader),
        ("load_shader_from_files", gfx.load_shader_from_files),
        ("use_shader", gfx.use_shader),
        ("set_uniform_mat4", gfx.set_uniform_mat4),
        ("set_uniform_vec3", gfx.set_uniform_vec3),
        ("set_uniform_vec4", gfx.set_uniform_vec4),
        ("set_uniform_float", gfx.set_uniform_float),
        ("set_uniform_int", gfx.set_uniform_int),
        ("delete_shader", gfx.delete_shader),
        ("create_vertex_buffer", gfx.create_vertex_buffer),
        ("create_index_buffer", gfx.create_index_buffer),
        ("delete_vertex_buffer", gfx.delete_vertex_buffer),
        ("delete_index_buffer", gfx.delete_index_buffer),
        ("create_vertex_array", gfx.create_vertex_array),
        ("vao_set_vertex_buffer", gfx.vao_set_vertex_buffer),
        ("vao_set_index_buffer", gfx.vao_set_index_buffer),
        ("vao_set_attribute", gfx.vao_set_attribute),
        ("vao_draw_arrays", gfx.vao_draw_arrays),
        ("vao_draw_elements", gfx.vao_draw_elements),
        ("delete_vertex_array", gfx.delete_vertex_array),
        ("create_texture", gfx.create_texture),
        ("bind_texture", gfx.bind_texture),
        ("delete_texture", gfx.delete_texture),
        ("load_texture_from_file", gfx.load_texture_from_file),
        ("create_depth_texture", gfx.create_depth_texture),
        ("create_cubemap", gfx.create_cubemap),
        ("load_cubemap_from_files", gfx.load_cubemap_from_files),
        ("bind_cubemap", gfx.bind_cubemap),
        ("delete_cubemap", gfx.delete_cubemap),
        ("create_skybox_geometry", gfx.create_skybox_geometry),
        ("create_framebuffer", gfx.create_framebuffer),
        ("bind_framebuffer", gfx.bind_framebuffer),
        ("unbind_framebuffer", gfx.unbind_framebuffer),
        ("framebuffer_attach_depth_texture", gfx.framebuffer_attach_depth_texture),
        ("framebuffer_attach_color_texture", gfx.framebuffer_attach_color_texture),
        ("delete_framebuffer", gfx.delete_framebuffer),
    ])


def _register_graphics_math_functions(runtime, gfx):
    _register_named_functions(runtime, [
        ("lerp", gfx.lerp),
        ("lerp_vec3", gfx.lerp_vec3),
        ("cubic_hermite", gfx.cubic_hermite),
        ("catmull_rom", gfx.catmull_rom),
        ("catmull_rom_vec3", gfx.catmull_rom_vec3),
        ("ease_linear", gfx.ease_linear),
        ("ease_in_quad", gfx.ease_in_quad),
        ("ease_out_quad", gfx.ease_out_quad),
        ("ease_in_out_quad", gfx.ease_in_out_quad),
        ("ease_in_cubic", gfx.ease_in_cubic),
        ("ease_out_cubic", gfx.ease_out_cubic),
        ("ease_in_out_cubic", gfx.ease_in_out_cubic),
        ("ease_in_out_sine", gfx.ease_in_out_sine),
        ("create_animation_track", gfx.create_animation_track),
        ("add_keyframe", gfx.add_keyframe),
        ("animation_play", gfx.animation_play),
        ("animation_stop", gfx.animation_stop),
        ("animation_update", gfx.animation_update),
        ("animation_evaluate_at", gfx.animation_evaluate_at),
        ("delete_animation_track", gfx.delete_animation_track),
        ("create_identity_matrix", gfx.create_identity_matrix),
        ("create_translation_matrix", gfx.create_translation_matrix),
        ("create_scale_matrix", gfx.create_scale_matrix),
        ("create_rotation_x_matrix", gfx.create_rotation_x_matrix),
        ("create_rotation_y_matrix", gfx.create_rotation_y_matrix),
        ("create_rotation_z_matrix", gfx.create_rotation_z_matrix),
        ("create_view_matrix", gfx.create_view_matrix),
        ("create_perspective_matrix", gfx.create_perspective_matrix),
        ("create_orthographic_matrix", gfx.create_orthographic_matrix),
        ("multiply_matrices", gfx.multiply_matrices),
        ("draw_rect", gfx.draw_rect),
        ("GL_TRIANGLES", lambda: GL_TRIANGLES),
        ("GL_TRIANGLE_STRIP", lambda: GL_TRIANGLE_STRIP),
        ("GL_TRIANGLE_FAN", lambda: GL_TRIANGLE_FAN),
        ("GL_LINES", lambda: GL_LINES),
        ("GL_LINE_STRIP", lambda: GL_LINE_STRIP),
        ("GL_POINTS", lambda: GL_POINTS),
    ])


def _register_glfw_constants(runtime):
    if not GLFW_AVAILABLE:
        return

    _register_named_functions(runtime, [
        ("KEY_W", lambda: glfw.KEY_W),
        ("KEY_A", lambda: glfw.KEY_A),
        ("KEY_S", lambda: glfw.KEY_S),
        ("KEY_D", lambda: glfw.KEY_D),
        ("KEY_Q", lambda: glfw.KEY_Q),
        ("KEY_E", lambda: glfw.KEY_E),
        ("KEY_SPACE", lambda: glfw.KEY_SPACE),
        ("KEY_ESCAPE", lambda: glfw.KEY_ESCAPE),
        ("KEY_ENTER", lambda: glfw.KEY_ENTER),
        ("KEY_TAB", lambda: glfw.KEY_TAB),
        ("KEY_LEFT_SHIFT", lambda: glfw.KEY_LEFT_SHIFT),
        ("KEY_LEFT_CONTROL", lambda: glfw.KEY_LEFT_CONTROL),
        ("KEY_LEFT_ALT", lambda: glfw.KEY_LEFT_ALT),
        ("KEY_UP", lambda: glfw.KEY_UP),
        ("KEY_DOWN", lambda: glfw.KEY_DOWN),
        ("KEY_LEFT", lambda: glfw.KEY_LEFT),
        ("KEY_RIGHT", lambda: glfw.KEY_RIGHT),
        ("KEY_0", lambda: glfw.KEY_0),
        ("KEY_1", lambda: glfw.KEY_1),
        ("KEY_2", lambda: glfw.KEY_2),
        ("KEY_3", lambda: glfw.KEY_3),
        ("KEY_4", lambda: glfw.KEY_4),
        ("KEY_5", lambda: glfw.KEY_5),
        ("KEY_6", lambda: glfw.KEY_6),
        ("KEY_7", lambda: glfw.KEY_7),
        ("KEY_8", lambda: glfw.KEY_8),
        ("KEY_9", lambda: glfw.KEY_9),
        ("MOUSE_BUTTON_LEFT", lambda: glfw.MOUSE_BUTTON_LEFT),
        ("MOUSE_BUTTON_RIGHT", lambda: glfw.MOUSE_BUTTON_RIGHT),
        ("MOUSE_BUTTON_MIDDLE", lambda: glfw.MOUSE_BUTTON_MIDDLE),
        ("CURSOR_NORMAL", lambda: glfw.CURSOR_NORMAL),
        ("CURSOR_HIDDEN", lambda: glfw.CURSOR_HIDDEN),
        ("CURSOR_DISABLED", lambda: glfw.CURSOR_DISABLED),
    ])


def register_graphics_functions(runtime):
    """Register graphics functions with NexusLang runtime"""
    gfx = GraphicsModule()

    def create_checkerboard_texture(width: int, height: int, cell_size: int) -> int:
        """Create a checkerboard texture"""
        data = []
        for y in range(height):
            for x in range(width):
                cell_x = x // cell_size
                cell_y = y // cell_size
                is_white = (cell_x + cell_y) % 2 == 0
                if is_white:
                    data.extend([255, 255, 255, 255])
                else:
                    data.extend([0, 0, 0, 255])
        return gfx.create_texture(width, height, bytes(data))

    def create_gradient_texture(width: int, height: int) -> int:
        """Create a gradient texture (red to blue)"""
        data = []
        for y in range(height):
            for x in range(width):
                r = int((x / width) * 255)
                g = 128
                b = int((y / height) * 255)
                data.extend([r, g, b, 255])
        return gfx.create_texture(width, height, bytes(data))

    _register_graphics_core_functions(runtime, gfx)
    _register_graphics_pipeline_functions(runtime, gfx)
    _register_graphics_math_functions(runtime, gfx)

    _register_named_functions(runtime, [
        ("create_checkerboard_texture", create_checkerboard_texture),
        ("create_gradient_texture", create_gradient_texture),
    ])
    _register_glfw_constants(runtime)

    runtime._graphics_module = gfx
    return gfx
