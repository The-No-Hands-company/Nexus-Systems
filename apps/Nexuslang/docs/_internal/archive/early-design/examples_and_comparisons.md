# NexusLang (NexusLang) Examples and Comparisons

This document provides concrete examples of how programs would be written in the proposed NexusLang (NexusLang) compared to their C++ equivalents. These examples demonstrate the readability, expressiveness, and power of NexusLang while showing how it maintains the capabilities of C++.

## Basic Examples

### 1. Hello World

**C++:**
```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
```

**NLPL:**
```
Display "Hello, World!".
```

### 2. Variables and Basic Arithmetic

**C++:**
```cpp
#include <iostream>

int main() {
    int a = 5;
    int b = 10;
    int sum = a + b;
    int product = a * b;
    
    std::cout << "Sum: " << sum << std::endl;
    std::cout << "Product: " << product << std::endl;
    
    return 0;
}
```

**NLPL:**
```
Set a to 5.
Set b to 10.
Set sum to a plus b.
Set product to a times b.

Display "Sum: " followed by sum.
Display "Product: " followed by product.
```

### 3. Conditional Statements

**C++:**
```cpp
#include <iostream>

int main() {
    int x = 15;
    
    if (x > 10) {
        std::cout << "x is greater than 10" << std::endl;
    } else if (x == 10) {
        std::cout << "x is equal to 10" << std::endl;
    } else {
        std::cout << "x is less than 10" << std::endl;
    }
    
    return 0;
}
```

**NLPL:**
```
Set x to 15.

If x is greater than 10, then
    Display "x is greater than 10".
Otherwise if x equals 10, then
    Display "x is equal to 10".
Otherwise
    Display "x is less than 10".
End if.
```

### 4. Loops

**C++:**
```cpp
#include <iostream>

int main() {
    // For loop
    for (int i = 1; i <= 5; i++) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
    
    // While loop
    int j = 1;
    while (j <= 5) {
        std::cout << j << " ";
        j++;
    }
    std::cout << std::endl;
    
    return 0;
}
```

**NLPL:**
```
// For loop
For each number i from 1 to 5, do
    Display i followed by " ".
End for.
Display a new line.

// While loop
Set j to 1.
While j is less than or equal to 5, do
    Display j followed by " ".
    Increment j by 1.
End while.
Display a new line.
```

## Intermediate Examples

### 5. Functions

**C++:**
```cpp
#include <iostream>

int add(int a, int b) {
    return a + b;
}

double calculateArea(double length, double width) {
    return length * width;
}

int main() {
    int sum = add(5, 10);
    std::cout << "Sum: " << sum << std::endl;
    
    double area = calculateArea(4.5, 2.5);
    std::cout << "Area: " << area << std::endl;
    
    return 0;
}
```

**NLPL:**
```
Define a function called add that takes a and b.
    Return a plus b.
End function.

Define a function called calculateArea that takes length and width and returns a number.
    Return length times width.
End function.

Set sum to add(5, 10).
Display "Sum: " followed by sum.

Set area to calculateArea(4.5, 2.5).
Display "Area: " followed by area.
```

### 6. Arrays and Collections

**C++:**
```cpp
#include <iostream>
#include <vector>

int main() {
    // Array
    int numbers[5] = {1, 2, 3, 4, 5};
    int sum = 0;
    
    for (int i = 0; i < 5; i++) {
        sum += numbers[i];
    }
    std::cout << "Sum of array: " << sum << std::endl;
    
    // Vector
    std::vector<int> vec = {10, 20, 30, 40, 50};
    int vecSum = 0;
    
    for (int num : vec) {
        vecSum += num;
    }
    std::cout << "Sum of vector: " << vecSum << std::endl;
    
    return 0;
}
```

**NLPL:**
```
// Array
Create an array of integers called numbers with values 1, 2, 3, 4, and 5.
Set sum to 0.

For each number i from 0 to 4, do
    Add numbers[i] to sum.
End for.
Display "Sum of array: " followed by sum.

// List (equivalent to vector)
Create a list of integers called vec with values 10, 20, 30, 40, and 50.
Set vecSum to 0.

For each num in vec, do
    Add num to vecSum.
End for.
Display "Sum of vector: " followed by vecSum.
```

### 7. String Manipulation

**C++:**
```cpp
#include <iostream>
#include <string>

int main() {
    std::string firstName = "John";
    std::string lastName = "Doe";
    std::string fullName = firstName + " " + lastName;
    
    std::cout << "Full name: " << fullName << std::endl;
    std::cout << "Length: " << fullName.length() << std::endl;
    std::cout << "Uppercase: ";
    
    for (char c : fullName) {
        std::cout << (char)toupper(c);
    }
    std::cout << std::endl;
    
    return 0;
}
```

**NLPL:**
```
Set firstName to "John".
Set lastName to "Doe".
Set fullName to firstName followed by " " followed by lastName.

Display "Full name: " followed by fullName.
Display "Length: " followed by the length of fullName.
Display "Uppercase: ".

For each character c in fullName, do
    Display the uppercase version of c.
End for.
Display a new line.
```

## Advanced Examples

### 8. Object-Oriented Programming

**C++:**
```cpp
#include <iostream>
#include <string>

class Person {
private:
    std::string name;
    int age;
    
public:
    Person(std::string n, int a) : name(n), age(a) {}
    
    void introduce() {
        std::cout << "My name is " << name << " and I am " << age << " years old." << std::endl;
    }
    
    void celebrateBirthday() {
        age++;
        std::cout << "Happy Birthday! Now I am " << age << " years old." << std::endl;
    }
};

int main() {
    Person john("John Doe", 30);
    john.introduce();
    john.celebrateBirthday();
    
    return 0;
}
```

**NLPL:**
```
Create a class called Person.
    Add a private property called name.
    Add a private property called age.
    
    Define a constructor that takes n and a.
        Set this person's name to n.
        Set this person's age to a.
    End constructor.
    
    Define a method called introduce that takes no parameters.
        Display "My name is " followed by this person's name followed by " and I am " followed by this person's age followed by " years old.".
    End method.
    
    Define a method called celebrateBirthday that takes no parameters.
        Increment this person's age by 1.
        Display "Happy Birthday! Now I am " followed by this person's age followed by " years old.".
    End method.
End class.

Create a Person called john with name "John Doe" and age 30.
Tell john to introduce.
Tell john to celebrateBirthday.
```

### 9. Memory Management

**C++:**
```cpp
#include <iostream>

int main() {
    // Dynamic memory allocation
    int* ptr = new int(10);
    std::cout << "Value: " << *ptr << std::endl;
    
    // Modify the value
    *ptr = 20;
    std::cout << "New value: " << *ptr << std::endl;
    
    // Free memory
    delete ptr;
    
    // Array allocation
    int* arr = new int[5];
    for (int i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }
    
    for (int i = 0; i < 5; i++) {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
    
    // Free array
    delete[] arr;
    
    return 0;
}
```

**NLPL:**
```
// Dynamic memory allocation
Allocate memory for an integer called ptr with value 10.
Display "Value: " followed by the value at ptr.

// Modify the value
Set the value at ptr to 20.
Display "New value: " followed by the value at ptr.

// Free memory
Free the memory used by ptr.

// Array allocation
Allocate memory for an array of 5 integers called arr.
For each number i from 0 to 4, do
    Set arr[i] to i times 10.
End for.

For each number i from 0 to 4, do
    Display arr[i] followed by " ".
End for.
Display a new line.

// Free array
Free the memory used by arr.
```

### 10. Error Handling

**C++:**
```cpp
#include <iostream>
#include <stdexcept>
#include <fstream>

int divide(int a, int b) {
    if (b == 0) {
        throw std::runtime_error("Division by zero");
    }
    return a / b;
}

int main() {
    try {
        std::cout << divide(10, 2) << std::endl;
        std::cout << divide(10, 0) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    try {
        std::ifstream file("nonexistent.txt");
        if (!file) {
            throw std::runtime_error("Could not open file");
        }
        // Read from file
    } catch (const std::exception& e) {
        std::cerr << "File error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

**NLPL:**
```
Define a function called divide that takes a and b.
    If b equals 0, then
        Throw an error with message "Division by zero".
    End if.
    Return a divided by b.
End function.

Try to
    Display divide(10, 2).
    Display divide(10, 0).
If something goes wrong, then
    Display "Error: " followed by the error message.
End try.

Try to
    Open the file "nonexistent.txt" for reading.
    If the file could not be opened, then
        Throw an error with message "Could not open file".
    End if.
    // Read from file
If something goes wrong, then
    Display "File error: " followed by the error message.
End try.
```

## Complex Examples

### 11. Data Processing Algorithm

**C++:**
```cpp
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

std::vector<double> normalizeData(const std::vector<double>& data) {
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    double mean = sum / data.size();
    
    double sq_sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / data.size() - mean * mean);
    
    std::vector<double> normalized(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        normalized[i] = (data[i] - mean) / stdev;
    }
    
    return normalized;
}

int main() {
    std::vector<double> data = {15.3, 12.7, 18.2, 14.5, 16.1, 13.8};
    
    std::cout << "Original data: ";
    for (double val : data) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    std::vector<double> normalized = normalizeData(data);
    
    std::cout << "Normalized data: ";
    for (double val : normalized) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    return 0;
}
```

**NLPL:**
```
Define a function called normalizeData that takes data and returns a list.
    Set sum to the sum of all values in data.
    Set mean to sum divided by the size of data.
    
    Set squaredSum to the sum of squares of all values in data.
    Set stdev to the square root of (squaredSum divided by the size of data minus mean squared).
    
    Create a list called normalized with the same size as data.
    For each index i from 0 to the size of data minus 1, do
        Set normalized[i] to (data[i] minus mean) divided by stdev.
    End for.
    
    Return normalized.
End function.

Create a list of numbers called data with values 15.3, 12.7, 18.2, 14.5, 16.1, and 13.8.

Display "Original data: ".
For each val in data, do
    Display val followed by " ".
End for.
Display a new line.

Set normalized to normalizeData(data).

Display "Normalized data: ".
For each val in normalized, do
    Display val followed by " ".
End for.
Display a new line.
```

### 12. Multithreaded Application

**C++:**
```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

std::mutex mtx;
int counter = 0;

void incrementCounter(int id, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            counter++;
            std::cout << "Thread " << id << " incremented counter to " << counter << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    const int numThreads = 3;
    const int iterationsPerThread = 5;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread(incrementCounter, i+1, iterationsPerThread));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final counter value: " << counter << std::endl;
    
    return 0;
}
```

**NLPL:**
```
Create a mutex called mtx.
Set counter to 0.

Define a function called incrementCounter that takes id and iterations.
    For each number i from 0 to iterations minus 1, do
        Lock the mutex mtx.
        Increment counter by 1.
        Display "Thread " followed by id followed by " incremented counter to " followed by counter.
        Unlock the mutex mtx.
        
        Sleep for 100 milliseconds.
    End for.
End function.

Set numThreads to 3.
Set iterationsPerThread to 5.

Create a list of threads called threads.

For each number i from 0 to numThreads minus 1, do
    Create a thread that runs incrementCounter with parameters (i plus 1) and iterationsPerThread.
    Add this thread to threads.
End for.

For each thread t in threads, do
    Wait for t to complete.
End for.

Display "Final counter value: " followed by counter.
```

## Comparison Summary

These examples demonstrate several key advantages of NexusLang over traditional C++:

1. **Reduced Boilerplate**: NexusLang eliminates the need for header includes, main function declarations, and return statements, focusing on the actual logic.

2. **Natural Readability**: NexusLang code reads like English prose, making it accessible to non-programmers while maintaining the precision needed for programming.

3. **Flexible Expression**: NexusLang allows multiple ways to express the same operation, accommodating different writing styles and preferences.

4. **Implicit Simplifications**: Common operations like displaying text or iterating through collections are simplified with more natural constructs.

5. **Maintained Power**: Despite its natural language syntax, NexusLang retains all the power of C++, including object-oriented programming, memory management, and multithreading.

The examples also show how NexusLang handles various programming paradigms:

- **Procedural Programming**: Basic operations, functions, and control flow
- **Object-Oriented Programming**: Classes, methods, and inheritance
- **Memory Management**: Both automatic and manual memory control
- **Concurrency**: Thread creation and synchronization
- **Error Handling**: Exception throwing and catching

NLPL achieves its goal of making programming more accessible without sacrificing the power and flexibility that makes C++ valuable for systems programming and performance-critical applications.
