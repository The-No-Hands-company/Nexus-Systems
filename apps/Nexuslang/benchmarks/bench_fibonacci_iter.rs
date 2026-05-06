// Iterative Fibonacci benchmark - matches bench_fibonacci.nlpl (Fibonacci(1000))
use std::time::Instant;

fn fibonacci(n: u64) -> u64 {
    if n <= 1 {
        return n;
    }
    let (mut a, mut b) = (0u64, 1u64);
    for _ in 2..=n {
        let temp = a + b;
        a = b;
        b = temp;
    }
    b
}

fn main() {
    let start = Instant::now();
    let result = fibonacci(1000);
    let elapsed = start.elapsed();
    println!("Fibonacci(1000) = {}", result);
    println!("Time: {:.9} seconds", elapsed.as_secs_f64());
}
