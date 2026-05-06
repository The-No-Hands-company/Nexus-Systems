"""
Simple OBJ mesh loader for NexusLang
Supports vertices, normals, texture coordinates, and faces
"""

class Mesh:
    """Represents a 3D mesh"""
    
    def __init__(self):
        self.vertices = []
        self.normals = []
        self.texcoords = []
        self.indices = []
    
    def get_vertex_data(self):
        """Get interleaved vertex data (position, normal, texcoord)"""
        return self.vertices
    
    def get_indices(self):
        """Get triangle indices"""
        return self.indices


def load_obj(filepath: str) -> Mesh:
    """Load OBJ file and return mesh data
    
    Returns mesh with interleaved vertex data: [x, y, z, nx, ny, nz, u, v, ...]
    """
    positions = []
    normals = []
    texcoords = []
    faces = []
    
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                parts = line.split()
                if not parts:
                    continue
                
                # Vertex position
                if parts[0] == 'v':
                    x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                    positions.append((x, y, z))
                
                # Vertex normal
                elif parts[0] == 'vn':
                    nx, ny, nz = float(parts[1]), float(parts[2]), float(parts[3])
                    normals.append((nx, ny, nz))
                
                # Texture coordinate
                elif parts[0] == 'vt':
                    u, v = float(parts[1]), float(parts[2])
                    texcoords.append((u, v))
                
                # Face (triangle)
                elif parts[0] == 'f':
                    face_vertices = []
                    for i in range(1, len(parts)):
                        # Parse vertex/texcoord/normal indices
                        indices = parts[i].split('/')
                        v_idx = int(indices[0]) - 1  # OBJ indices are 1-based
                        t_idx = int(indices[1]) - 1 if len(indices) > 1 and indices[1] else 0
                        n_idx = int(indices[2]) - 1 if len(indices) > 2 and indices[2] else 0
                        face_vertices.append((v_idx, t_idx, n_idx))
                    
                    # Triangulate if quad (simple fan triangulation)
                    if len(face_vertices) == 3:
                        faces.append(face_vertices)
                    elif len(face_vertices) == 4:
                        # Split quad into two triangles
                        faces.append([face_vertices[0], face_vertices[1], face_vertices[2]])
                        faces.append([face_vertices[0], face_vertices[2], face_vertices[3]])
    
    except FileNotFoundError:
        raise RuntimeError(f"OBJ file not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error loading OBJ file: {e}")
    
    # Build interleaved vertex buffer and index buffer
    mesh = Mesh()
    vertex_map = {}  # Maps (v, t, n) tuple to vertex index
    vertex_index = 0
    
    for face in faces:
        for v_idx, t_idx, n_idx in face:
            key = (v_idx, t_idx, n_idx)
            
            if key not in vertex_map:
                # Add new vertex
                pos = positions[v_idx] if v_idx < len(positions) else (0, 0, 0)
                norm = normals[n_idx] if n_idx < len(normals) else (0, 0, 1)
                tex = texcoords[t_idx] if t_idx < len(texcoords) else (0, 0)
                
                # Interleaved: x, y, z, nx, ny, nz, u, v
                mesh.vertices.extend([
                    pos[0], pos[1], pos[2],
                    norm[0], norm[1], norm[2],
                    tex[0], tex[1]
                ])
                
                vertex_map[key] = vertex_index
                mesh.indices.append(vertex_index)
                vertex_index += 1
            else:
                # Reuse existing vertex
                mesh.indices.append(vertex_map[key])
    
    return mesh


def create_cube_mesh() -> Mesh:
    """Create a simple cube mesh programmatically"""
    mesh = Mesh()
    
    # Cube vertices with normals (no UVs for simplicity)
    vertices = [
        # Front face (z = 0.5)
        -0.5, -0.5, 0.5, 0.0, 0.0, 1.0, 0.0, 0.0,
        0.5, -0.5, 0.5, 0.0, 0.0, 1.0, 1.0, 0.0,
        0.5, 0.5, 0.5, 0.0, 0.0, 1.0, 1.0, 1.0,
        -0.5, 0.5, 0.5, 0.0, 0.0, 1.0, 0.0, 1.0,
        
        # Back face (z = -0.5)
        0.5, -0.5, -0.5, 0.0, 0.0, -1.0, 0.0, 0.0,
        -0.5, -0.5, -0.5, 0.0, 0.0, -1.0, 1.0, 0.0,
        -0.5, 0.5, -0.5, 0.0, 0.0, -1.0, 1.0, 1.0,
        0.5, 0.5, -0.5, 0.0, 0.0, -1.0, 0.0, 1.0,
        
        # Right face (x = 0.5)
        0.5, -0.5, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0,
        0.5, -0.5, -0.5, 1.0, 0.0, 0.0, 1.0, 0.0,
        0.5, 0.5, -0.5, 1.0, 0.0, 0.0, 1.0, 1.0,
        0.5, 0.5, 0.5, 1.0, 0.0, 0.0, 0.0, 1.0,
        
        # Left face (x = -0.5)
        -0.5, -0.5, -0.5, -1.0, 0.0, 0.0, 0.0, 0.0,
        -0.5, -0.5, 0.5, -1.0, 0.0, 0.0, 1.0, 0.0,
        -0.5, 0.5, 0.5, -1.0, 0.0, 0.0, 1.0, 1.0,
        -0.5, 0.5, -0.5, -1.0, 0.0, 0.0, 0.0, 1.0,
        
        # Top face (y = 0.5)
        -0.5, 0.5, 0.5, 0.0, 1.0, 0.0, 0.0, 0.0,
        0.5, 0.5, 0.5, 0.0, 1.0, 0.0, 1.0, 0.0,
        0.5, 0.5, -0.5, 0.0, 1.0, 0.0, 1.0, 1.0,
        -0.5, 0.5, -0.5, 0.0, 1.0, 0.0, 0.0, 1.0,
        
        # Bottom face (y = -0.5)
        -0.5, -0.5, -0.5, 0.0, -1.0, 0.0, 0.0, 0.0,
        0.5, -0.5, -0.5, 0.0, -1.0, 0.0, 1.0, 0.0,
        0.5, -0.5, 0.5, 0.0, -1.0, 0.0, 1.0, 1.0,
        -0.5, -0.5, 0.5, 0.0, -1.0, 0.0, 0.0, 1.0,
    ]
    
    # Indices (6 faces * 2 triangles * 3 vertices)
    indices = [
        0, 1, 2, 2, 3, 0,      # Front
        4, 5, 6, 6, 7, 4,      # Back
        8, 9, 10, 10, 11, 8,   # Right
        12, 13, 14, 14, 15, 12, # Left
        16, 17, 18, 18, 19, 16, # Top
        20, 21, 22, 22, 23, 20  # Bottom
    ]
    
    mesh.vertices = vertices
    mesh.indices = indices
    
    return mesh


def register_mesh_functions(runtime):
    """Register mesh loading functions with NexusLang runtime"""
    
    def nxl_load_obj(filepath: str):
        """Load OBJ file and return vertex/index data as lists"""
        mesh = load_obj(filepath)
        return {
            'vertices': mesh.vertices,
            'indices': mesh.indices,
            'vertex_count': len(mesh.vertices) // 8,  # 8 floats per vertex
            'index_count': len(mesh.indices)
        }
    
    def nxl_create_cube_mesh():
        """Create cube mesh and return data"""
        mesh = create_cube_mesh()
        return {
            'vertices': mesh.vertices,
            'indices': mesh.indices,
            'vertex_count': len(mesh.vertices) // 8,
            'index_count': len(mesh.indices)
        }
    
    runtime.register_function("load_obj_mesh", nxl_load_obj)
    runtime.register_function("create_cube_mesh", nxl_create_cube_mesh)
