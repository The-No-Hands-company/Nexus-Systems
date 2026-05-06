# Tutorial 3: Objects and Classes

**Time:** ~60 minutes  
**Prerequisites:** [Variables, Functions, and Control Flow](02-variables-functions-control-flow.md)

---

## Part 1 — Your First Class

A **class** is a blueprint for creating objects.  Each object has its own
copy of the class's fields (data) and can call the class's methods (functions).

```nexuslang
class Point
    public set x to Float
    public set y to Float

    public function initialize with x as Float and y as Float
        set this.x to x
        set this.y to y

    public function to_string returns String
        return "(" plus convert this.x to string plus ", " plus convert this.y to string plus ")"
```

Create an object with `create`:

```nexuslang
set origin to create Point with 0.0 and 0.0
set p      to create Point with 3.0 and 4.0
print text p.to_string()    # (3.0, 4.0)
```

### `this` Refers to the Current Object

Inside a method, `this` is a reference to the object the method is running on:

```nexuslang
public function move_by with dx as Float and dy as Float
    set this.x to this.x plus dx
    set this.y to this.y plus dy
```

---

## Part 2 — Access Modifiers

| Keyword | Meaning |
|---------|---------|
| `public` | Accessible from anywhere |
| `private` | Accessible only inside the class |

Prefer `private` for fields and expose them via getter/setter methods:

```nexuslang
class BankAccount
    private set balance to Float

    public function initialize with initial as Float
        set this.balance to initial

    public function get balance returns Float
        return this.balance

    public function deposit with amount as Float
        if amount is greater than 0.0
            set this.balance to this.balance plus amount

    public function withdraw with amount as Float returns Boolean
        if amount is greater than this.balance
            return false
        set this.balance to this.balance minus amount
        return true
```

```nexuslang
set account to create BankAccount with 100.0
account.deposit with 50.0
set ok to account.withdraw with 30.0
print text "Balance: " plus convert account.get_balance() to string
```

---

## Part 3 — Inheritance

A class that **extends** another class inherits all of its public methods and
can add new ones or override existing ones.

```nexuslang
class Animal
    private set name to String

    public function initialize with name as String
        set this.name to name

    public function get name returns String
        return this.name

    public function speak returns String
        return "..."

class Dog extends Animal
    public function initialize with name as String
        call super.initialize with name

    public function speak returns String
        return "Woof!"

class Cat extends Animal
    public function initialize with name as String
        call super.initialize with name

    public function speak returns String
        return "Meow!"
```

```nexuslang
set animals to [create Dog with "Rex", create Cat with "Whiskers"]
for each animal in animals
    print text animal.get_name() plus " says: " plus animal.speak()
```

Output:

```
Rex says: Woof!
Whiskers says: Meow!
```

`super` refers to the parent class — use it to call the parent's version of
a method.

---

## Part 4 — Interfaces

An **interface** declares a set of methods that any implementing class must
provide.  Interfaces describe *what* a class can do without specifying *how*.

```nexuslang
interface Drawable
    public function draw returns String

interface Resizable
    public function resize with factor as Float

class Circle implements Drawable and Resizable
    private set x to Float
    private set y to Float
    private set radius to Float

    public function initialize with x as Float and y as Float and radius as Float
        set this.x to x
        set this.y to y
        set this.radius to radius

    public function draw returns String
        return "Circle at (" plus convert this.x to string plus ", " plus convert this.y to string plus ") r=" plus convert this.radius to string

    public function resize with factor as Float
        set this.radius to this.radius times factor
```

---

## Part 5 — Abstract Classes

An **abstract class** sits between an interface and a concrete class.  It can
provide some method implementations while leaving others to subclasses.

```nexuslang
abstract class Shape
    public abstract function area returns Float
    public abstract function perimeter returns Float

    public function describe returns String
        return "area=" plus convert this.area() to string plus " perimeter=" plus convert this.perimeter() to string

class Rectangle extends Shape
    private set width to Float
    private set height to Float

    public function initialize with width as Float and height as Float
        set this.width to width
        set this.height to height

    public function area returns Float
        return this.width times this.height

    public function perimeter returns Float
        return 2.0 times (this.width plus this.height)
```

```nexuslang
set r to create Rectangle with 4.0 and 5.0
print text r.describe()   # area=20.0 perimeter=18.0
```

---

## Part 6 — Bringing It All Together

A small inventory system using classes and inheritance:

```nexuslang
abstract class Product
    private set name to String
    private set price to Float

    public function initialize with name as String and price as Float
        set this.name to name
        set this.price to price

    public function get name   returns String  return this.name
    public function get price  returns Float   return this.price

    public abstract function category returns String

    public function summary returns String
        return this.category() plus ": " plus this.name plus " ($" plus convert this.price to string plus ")"

class PhysicalProduct extends Product
    private set weight_kg to Float

    public function initialize with name as String and price as Float and weight_kg as Float
        call super.initialize with name and price
        set this.weight_kg to weight_kg

    public function category returns String
        return "Physical"

class DigitalProduct extends Product
    private set download_url to String

    public function initialize with name as String and price as Float and download_url as String
        call super.initialize with name and price
        set this.download_url to download_url

    public function category returns String
        return "Digital"

set catalog to [
    create PhysicalProduct with "Keyboard" and 79.99 and 0.8,
    create DigitalProduct with "eBook" and 9.99 and "https://example.com/book.pdf",
    create PhysicalProduct with "Monitor" and 299.99 and 3.5
]

for each item in catalog
    print text item.summary()
```

---

## Summary

| Concept | Syntax |
|---------|--------|
| Define class | `class Name … ` |
| Create object | `create ClassName with args` |
| Access field | `object.field` |
| Call method | `object.method()` |
| Inheritance | `class Child extends Parent` |
| Call parent | `call super.method with args` |
| Interface | `interface I … ` |
| Implement interface | `class C implements I` |
| Abstract class | `abstract class C` |
| Abstract method | `public abstract function f` |

**Next:** [Error Handling](04-error-handling.md)
