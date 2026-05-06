"""
Common shader presets for NexusLang graphics applications
Provides ready-to-use vertex and fragment shaders
"""

# Basic color shader - interpolated vertex colors
BASIC_COLOR_VERTEX = """#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aColor;
}
"""

BASIC_COLOR_FRAGMENT = """#version 330 core
out vec4 FragColor;
in vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
"""

# Solid color shader - uniform color
SOLID_COLOR_VERTEX = """#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
"""

SOLID_COLOR_FRAGMENT = """#version 330 core
out vec4 FragColor;

uniform vec4 color;

void main()
{
    FragColor = color;
}
"""

# Basic lighting shader - Phong
PHONG_VERTEX = """#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
"""

PHONG_FRAGMENT = """#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
"""


def register_shader_functions(runtime):
    """Register shader preset functions with NexusLang runtime"""
    
    def get_basic_color_vertex():
        return BASIC_COLOR_VERTEX
    
    def get_basic_color_fragment():
        return BASIC_COLOR_FRAGMENT
    
    def get_solid_color_vertex():
        return SOLID_COLOR_VERTEX
    
    def get_solid_color_fragment():
        return SOLID_COLOR_FRAGMENT
    
    def get_phong_vertex():
        return PHONG_VERTEX
    
    def get_phong_fragment():
        return PHONG_FRAGMENT
    
    runtime.register_function("get_basic_color_vertex", get_basic_color_vertex)
    runtime.register_function("get_basic_color_fragment", get_basic_color_fragment)
    runtime.register_function("get_solid_color_vertex", get_solid_color_vertex)
    runtime.register_function("get_solid_color_fragment", get_solid_color_fragment)
    runtime.register_function("get_phong_vertex", get_phong_vertex)
    runtime.register_function("get_phong_fragment", get_phong_fragment)
