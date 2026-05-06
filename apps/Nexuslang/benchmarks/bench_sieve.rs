// Sieve of Eratosthenes benchmark - matches bench_sieve.nlpl (primes up to 1000)
use std::time::Instant;

fn sieve_of_eratosthenes(limit: usize) -> usize {
    let mut primes = vec![true; limit + 1];
    if limit >= 0 {
        primes[0] = false;
    }
    if limit >= 1 {
        primes[1] = false;
    }
    let mut p = 2;
    while p * p <= limit {
        if primes[p] {
            let mut multiple = p * p;
            while multiple <= limit {
                primes[multiple] = false;
                multiple += p;
            }
        }
        p += 1;
    }
    primes[2..].iter().filter(|&&v| v).count()
}

fn main() {
    let start = Instant::now();
    let result = sieve_of_eratosthenes(1000);
    let elapsed = start.elapsed();
    println!("Found {} primes up to 1000", result);
    println!("Time: {:.9} seconds", elapsed.as_secs_f64());
}
