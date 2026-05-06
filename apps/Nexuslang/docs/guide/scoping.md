# Scoping and Blocks

## Understanding Blocks

In NexusLang, blocks are groups of related commands that are indented together. They define the structure and flow of your program.

### Basic Block Structure
```
Create a game object:
    Set position to center
    Set size to 100 pixels
    Add collision detection
```

## Types of Blocks

### 1. Object Definition Blocks
Used when creating or configuring objects:
```
Create a player:
    Use image "player.png"
    Set starting health to 100
    Enable physics
    When damaged:
        Show hit animation
        Play hurt sound
```

### 2. Event Blocks
Triggered when specific conditions are met:
```
When space key is pressed:
    Make player jump
    Play jump sound
    Create dust effect
```

### 3. Loop Blocks
Execute commands repeatedly:
```
Every frame:
    Update player position
    Check for collisions
    Update score display
```

## Scope Rules

### 1. Object Scope
Objects are accessible within their creation block and any nested blocks:
```
Create a game scene:
    Create a player:
        Set speed to 5
    Create three enemies:
        Set behavior to "chase player"  # Player is accessible here
```

### 2. Variable Scope
Variables are accessible within their block and nested blocks:
```
Create score counter with value 0

When coin collected:
    Add 10 to score counter  # Score counter is accessible here
    Show score on screen
```

### 3. Behavior Scope
Behaviors can access their parent object's properties:
```
Create enemy behavior "Patrol":
    Move right for 2 seconds
    Move left for 2 seconds
    # Can access enemy properties here
```

## Nesting Rules

### 1. Indentation
Each nested level uses 4 spaces or 1 tab:
```
Create menu screen:
    Add start button:
        When clicked:
            Start new game
            Hide menu screen
```

### 2. Multiple Levels
Blocks can be nested multiple levels deep:
```
Create game manager:
    When game starts:
        Create player:
            Set position to start point
            When space pressed:
                If can jump:
                    Perform jump
```

## Best Practices

### 1. Maintain Clear Structure
- Use consistent indentation
- Keep related commands together
- Don't nest too deeply (maximum 3-4 levels recommended)

### 2. Logical Grouping
```
Create player:
    # Physical properties
    Set size to 64 pixels
    Set weight to 1

    # Behaviors
    When moving:
        Play walk animation
    When jumping:
        Play jump animation
```

### 3. Scope Management
- Keep objects in appropriate scope
- Don't create unnecessary nested blocks
- Use clear names for different scopes

## Common Patterns

### 1. State Management
```
Create game states:
    State "Menu":
        Show menu items
        Handle menu input
    State "Playing":
        Update game world
        Handle player input
    State "Paused":
        Show pause menu
        Freeze game updates
```

### 2. Component Organization
```
Create player:
    # Visual components
    Add sprite renderer:
        Use image "player.png"
        Set layer to 1

    # Physics components
    Add collision detection:
        Set hitbox size to 32 by 32
        Enable trigger events
```