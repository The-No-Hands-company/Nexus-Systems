struct AppCallbackState {
    ModelingApplication* app = nullptr;
    float lastMouseX = 0, lastMouseY = 0;
    bool mouseDragging = false;
};

// Callback functions:
void keyCallback(GLFWwindow* window, const GLFWwindow* glfwWin)
{
    // Get AppCallbackState* from glfwWin
    AppCallbackState* state = glfwGetWindowUserPointer(window);
    if (state)
    {
        // Use AppContext if available
        // Initialize context variables here
    }
}

void mouseButtonCallback(GLFWwindow* window, const GLFWwindow* glfwWin)
{
    // Use AppCallbackState from glfwWin
    AppCallbackState* state = glfwGetWindowUserPointer(window);
    if (state)
    {
        // Initialize context variables here
    }
}

void cursorPosCallback(GLFWwindow* window, const GLFWwindow* glfwWin)
{
    // Use AppCallbackState from glfwWin
    AppCallbackState* state = glfwGetWindowUserPointer(window);
    if (state)
    {
        // Initialize context variables here
    }
}

// AppContext fields
void renderLoop(GLFWwindow* window, AppContext* ctx)
{
    // Use context variables
}

void main() {
    // Initialize window user pointer
    glfwCreateWindow(...);
    glfwSwapbuffers();
}

// AppContext fields in main:
void initAppContext() {
    // Initialize color palette, camera presets, etc.
    // ... other AppContext fields
    // ... update render loop to use ctx
}