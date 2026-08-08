# Commands and Phrases

## Core Commands

### Creation Commands

#### Create

Creates new objects, windows, or game elements:

```nxl
Create a window called "My Game"
Create a player at position 100, 200
Create three enemies
```

#### Add

Adds elements to existing objects or scenes:

```nxl
Add a button to the menu
Add collision detection to player
Add background music "theme.mp3"
```

### Modification Commands

#### Set

Changes properties or values:

```nxl
Set window size to 1280 by 720
Set player speed to 5
Set background color to blue
```

#### Make

Performs actions or changes states:

```nxl
Make the player jump
Make the button glow
Make the enemy follow player
```

### Movement Commands

#### Move

Controls object movement:

```text
Move player left by 5 pixels
Move camera smoothly to target
Move all enemies toward player
```

#### Position

Sets object positions:

```text
Position player at screen center
Position menu at top of screen
Position enemies randomly
```

## Event Phrases

### When

Handles events and conditions:

```nxl
When space key is pressed:
When player touches enemy:
When health is below 20:
```

### Every

Handles recurring events:

```nxl
Every frame:
Every 2 seconds:
Every game tick:
```

## Property Phrases

### Size and Dimensions

```nxl
Set size to 100 pixels
Make width 200 pixels
Scale to twice current size
```

### Colors and Appearance

```nxl
Set color to bright red
Make background transparent
Change opacity to 50%
```

### Physics Properties

```nxl
Enable gravity
Set bounce factor to 0.5
Make object solid
```

## Control Flow Phrases

### Conditionals

```nxl
If player has powerup:
When score reaches 100:
If enemy is nearby:
```

### Loops

```nxl
Repeat forever:
Repeat 5 times:
While player is moving:
```

## Common Combinations

### Game Setup

```nxl
Create a new game:
    Set window size to 1280 by 720
    Enable fullscreen
    Set frame rate to 60
```

### Object Behaviors

```nxl
Create enemy behavior:
    Move toward player
    When health is zero:
        Play death animation
        Remove from game
```

### UI Elements

```nxl
Create menu screen:
    Add title "Main Menu"
    Add start button:
        When clicked:
            Start new game
    Add quit button:
        When clicked:
            Exit game
```

## Best Practices

1. **Use Clear Actions**
   - Be specific about what you want to happen
   - Use descriptive verbs
   - Avoid ambiguous phrases

2. **Maintain Natural Flow**
   - Write commands as you would explain them
   - Keep related commands together
   - Use appropriate indentation

3. **Be Consistent**
   - Use similar phrases for similar actions
   - Maintain consistent naming
   - Follow established patterns
