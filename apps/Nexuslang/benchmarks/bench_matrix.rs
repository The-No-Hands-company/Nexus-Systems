// Matrix sum benchmark - matches bench_matrix.nlpl (200x200 matrix)
use std::time::Instant;

fn matrix_sum(size: usize) -> i64 {
    let mut total: i64 = 0;
    for i in 0..size {
        for j in 0..size {
            total += (i * size + j) as i64;
        }
    }
    total
}

fn main() {
    let start = Instant::now();
    let result = matrix_sum(200);
    let elapsed = start.elapsed();
    println!("Matrix sum (200x200): {}", result);
    println!("Time: {:.9} seconds", elapsed.as_secs_f64());
}
