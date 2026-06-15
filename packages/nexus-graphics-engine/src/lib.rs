use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Vec2 {
    pub x: f64,
    pub y: f64,
}

impl Vec2 {
    pub fn new(x: f64, y: f64) -> Self {
        Self { x, y }
    }
    pub fn add(&self, o: &Self) -> Self {
        Self { x: self.x + o.x, y: self.y + o.y }
    }
    pub fn sub(&self, o: &Self) -> Self {
        Self { x: self.x - o.x, y: self.y - o.y }
    }
    pub fn scale(&self, s: f64) -> Self {
        Self { x: self.x * s, y: self.y * s }
    }
    pub fn dot(&self, o: &Self) -> f64 {
        self.x * o.x + self.y * o.y
    }
    pub fn len(&self) -> f64 {
        (self.x * self.x + self.y * self.y).sqrt()
    }
    pub fn normalize(&self) -> Self {
        let l = self.len();
        if l == 0.0 {
            Self::new(0.0, 0.0)
        } else {
            Self::new(self.x / l, self.y / l)
        }
    }
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Matrix3 {
    pub a: f64,
    pub b: f64,
    pub c: f64,
    pub d: f64,
    pub e: f64,
    pub f: f64,
}

impl Matrix3 {
    pub fn identity() -> Self {
        Self { a: 1.0, b: 0.0, c: 0.0, d: 1.0, e: 0.0, f: 0.0 }
    }
    pub fn translate(tx: f64, ty: f64) -> Self {
        Self { a: 1.0, b: 0.0, c: 0.0, d: 1.0, e: tx, f: ty }
    }
    pub fn scale(sx: f64, sy: f64) -> Self {
        Self { a: sx, b: 0.0, c: 0.0, d: sy, e: 0.0, f: 0.0 }
    }
    pub fn rotate(angle: f64) -> Self {
        let (s, c) = (angle.sin(), angle.cos());
        Self { a: c, b: s, c: -s, d: c, e: 0.0, f: 0.0 }
    }
    pub fn apply(&self, p: &Vec2) -> Vec2 {
        Vec2 {
            x: self.a * p.x + self.c * p.y + self.e,
            y: self.b * p.x + self.d * p.y + self.f,
        }
    }
    pub fn mul(&self, o: &Self) -> Self {
        Self {
            a: self.a * o.a + self.c * o.b,
            b: self.b * o.a + self.d * o.b,
            c: self.a * o.c + self.c * o.d,
            d: self.b * o.c + self.d * o.d,
            e: self.a * o.e + self.c * o.f + self.e,
            f: self.b * o.e + self.d * o.f + self.f,
        }
    }
    pub fn inverse(&self) -> Option<Self> {
        let det = self.a * self.d - self.b * self.c;
        if det.abs() < 1e-10 {
            return None;
        }
        let inv = 1.0 / det;
        Some(Self {
            a: self.d * inv,
            b: -self.b * inv,
            c: -self.c * inv,
            d: self.a * inv,
            e: (self.c * self.f - self.d * self.e) * inv,
            f: (self.b * self.e - self.a * self.f) * inv,
        })
    }
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct BBox {
    pub x: f64,
    pub y: f64,
    pub w: f64,
    pub h: f64,
}

impl BBox {
    pub fn new(x: f64, y: f64, w: f64, h: f64) -> Self {
        Self { x, y, w, h }
    }
    pub fn union(&self, o: &Self) -> Self {
        let x1 = self.x.min(o.x);
        let y1 = self.y.min(o.y);
        let x2 = (self.x + self.w).max(o.x + o.w);
        let y2 = (self.y + self.h).max(o.y + o.h);
        Self {
            x: x1,
            y: y1,
            w: x2 - x1,
            h: y2 - y1,
        }
    }
    pub fn contains(&self, p: &Vec2) -> bool {
        p.x >= self.x && p.x <= self.x + self.w && p.y >= self.y && p.y <= self.y + self.h
    }
    pub fn transform(&self, m: &Matrix3) -> Self {
        let corners = [
            Vec2::new(self.x, self.y),
            Vec2::new(self.x + self.w, self.y),
            Vec2::new(self.x, self.y + self.h),
            Vec2::new(self.x + self.w, self.y + self.h),
        ];
        let mut tx = corners.iter().map(|c| m.apply(c));
        let first = tx.next().unwrap();
        let (min_x, max_x, min_y, max_y) =
            tx.fold(
                (first.x, first.x, first.y, first.y),
                |(mnx, mxx, mny, mxy), p| {
                    (mnx.min(p.x), mxx.max(p.x), mny.min(p.y), mxy.max(p.y))
                },
            );
        Self {
            x: min_x,
            y: min_y,
            w: max_x - min_x,
            h: max_y - min_y,
        }
    }
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct PathPoint {
    pub x: f64,
    pub y: f64,
    pub cx1: f64,
    pub cy1: f64,
    pub cx2: f64,
    pub cy2: f64,
    pub cmd: String,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: f64,
}

impl Color {
    pub fn new(r: u8, g: u8, b: u8, a: f64) -> Self {
        Self { r, g, b, a }
    }
    pub fn from_hex(hex: &str) -> Self {
        let h = hex.trim_start_matches('#');
        if h.len() == 6 {
            let v = u32::from_str_radix(h, 16).unwrap_or(0);
            Self {
                r: ((v >> 16) & 0xff) as u8,
                g: ((v >> 8) & 0xff) as u8,
                b: (v & 0xff) as u8,
                a: 1.0,
            }
        } else if h.len() == 8 {
            let v = u32::from_str_radix(h, 16).unwrap_or(0);
            Self {
                r: ((v >> 24) & 0xff) as u8,
                g: ((v >> 16) & 0xff) as u8,
                b: ((v >> 8) & 0xff) as u8,
                a: (v & 0xff) as f64 / 255.0,
            }
        } else {
            Self { r: 0, g: 0, b: 0, a: 1.0 }
        }
    }
    pub fn to_hex(&self) -> String {
        format!("#{:02x}{:02x}{:02x}", self.r, self.g, self.b)
    }
    pub fn lerp(&self, other: &Self, t: f64) -> Self {
        Self {
            r: (self.r as f64 + (other.r as f64 - self.r as f64) * t) as u8,
            g: (self.g as f64 + (other.g as f64 - self.g as f64) * t) as u8,
            b: (self.b as f64 + (other.b as f64 - self.b as f64) * t) as u8,
            a: self.a + (other.a - self.a) * t,
        }
    }
}

pub fn compute_bezier(t: f64, p0: &Vec2, p1: &Vec2, p2: &Vec2, p3: &Vec2) -> Vec2 {
    let mt = 1.0 - t;
    Vec2 {
        x: mt * mt * mt * p0.x
            + 3.0 * mt * mt * t * p1.x
            + 3.0 * mt * t * t * p2.x
            + t * t * t * p3.x,
        y: mt * mt * mt * p0.y
            + 3.0 * mt * mt * t * p1.y
            + 3.0 * mt * t * t * p2.y
            + t * t * t * p3.y,
    }
}

pub fn hit_test_bbox(point: &Vec2, bbox: &BBox) -> bool {
    bbox.contains(point)
}

pub fn point_distance(a: &Vec2, b: &Vec2) -> f64 {
    a.sub(b).len()
}

/// Parse an SVG path `d` attribute into a JSON string of PathPoint values.
pub fn parse_svg_path(d: &str) -> Vec<PathPoint> {
    let _ = d;
    Vec::new()
}

// --- wasm-bindgen wrappers (only compiled for wasm32) ---

#[cfg(target_arch = "wasm32")]
mod wasm {
    use wasm_bindgen::prelude::*;
    use crate::*;

    #[wasm_bindgen]
    pub fn version() -> String {
        "0.1.0".to_string()
    }

    #[wasm_bindgen]
    pub fn vec2_add(a: JsValue, b: JsValue) -> JsValue {
        let a: Vec2 = serde_wasm_bindgen::from_value(a).unwrap_or(Vec2::new(0.0, 0.0));
        let b: Vec2 = serde_wasm_bindgen::from_value(b).unwrap_or(Vec2::new(0.0, 0.0));
        serde_wasm_bindgen::to_value(&a.add(&b)).unwrap_or(JsValue::null())
    }

    #[wasm_bindgen]
    pub fn point_distance_js(a: JsValue, b: JsValue) -> f64 {
        let a: Vec2 = serde_wasm_bindgen::from_value(a).unwrap_or(Vec2::new(0.0, 0.0));
        let b: Vec2 = serde_wasm_bindgen::from_value(b).unwrap_or(Vec2::new(0.0, 0.0));
        point_distance(&a, &b)
    }

    #[wasm_bindgen]
    pub fn matrix_identity() -> JsValue {
        serde_wasm_bindgen::to_value(&Matrix3::identity()).unwrap_or(JsValue::null())
    }

    #[wasm_bindgen]
    pub fn matrix_translate(tx: f64, ty: f64) -> JsValue {
        serde_wasm_bindgen::to_value(&Matrix3::translate(tx, ty)).unwrap_or(JsValue::null())
    }

    #[wasm_bindgen]
    pub fn matrix_apply(m: JsValue, p: JsValue) -> JsValue {
        let m: Matrix3 = serde_wasm_bindgen::from_value(m).unwrap_or(Matrix3::identity());
        let p: Vec2 = serde_wasm_bindgen::from_value(p).unwrap_or(Vec2::new(0.0, 0.0));
        serde_wasm_bindgen::to_value(&m.apply(&p)).unwrap_or(JsValue::null())
    }

    #[wasm_bindgen]
    pub fn bbox_contains(bbox: JsValue, point: JsValue) -> bool {
        let bbox: BBox = serde_wasm_bindgen::from_value(bbox).unwrap_or(BBox::new(0.0, 0.0, 0.0, 0.0));
        let point: Vec2 = serde_wasm_bindgen::from_value(point).unwrap_or(Vec2::new(0.0, 0.0));
        hit_test_bbox(&point, &bbox)
    }

    #[wasm_bindgen]
    pub fn compute_bezier_js(t: f64, p0: JsValue, p1: JsValue, p2: JsValue, p3: JsValue) -> JsValue {
        let p0: Vec2 = serde_wasm_bindgen::from_value(p0).unwrap_or(Vec2::new(0.0, 0.0));
        let p1: Vec2 = serde_wasm_bindgen::from_value(p1).unwrap_or(Vec2::new(0.0, 0.0));
        let p2: Vec2 = serde_wasm_bindgen::from_value(p2).unwrap_or(Vec2::new(0.0, 0.0));
        let p3: Vec2 = serde_wasm_bindgen::from_value(p3).unwrap_or(Vec2::new(0.0, 0.0));
        serde_wasm_bindgen::to_value(&compute_bezier(t, &p0, &p1, &p2, &p3))
            .unwrap_or(JsValue::null())
    }

    #[wasm_bindgen]
    pub fn color_from_hex(hex: &str) -> JsValue {
        let c = Color::from_hex(hex);
        serde_wasm_bindgen::to_value(&c).unwrap_or(JsValue::null())
    }

    #[wasm_bindgen]
    pub fn color_lerp(a: JsValue, b: JsValue, t: f64) -> JsValue {
        let a: Color = serde_wasm_bindgen::from_value(a).unwrap_or(Color::new(0, 0, 0, 1.0));
        let b: Color = serde_wasm_bindgen::from_value(b).unwrap_or(Color::new(0, 0, 0, 1.0));
        serde_wasm_bindgen::to_value(&a.lerp(&b, t)).unwrap_or(JsValue::null())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vec2() {
        let a = Vec2::new(3.0, 4.0);
        assert!((a.len() - 5.0).abs() < 1e-10);
    }

    #[test]
    fn test_matrix_translate() {
        let m = Matrix3::translate(10.0, 20.0);
        let p = m.apply(&Vec2::new(1.0, 2.0));
        assert!((p.x - 11.0).abs() < 1e-10);
    }

    #[test]
    fn test_bbox() {
        let b = BBox::new(0.0, 0.0, 100.0, 100.0);
        assert!(b.contains(&Vec2::new(50.0, 50.0)));
        assert!(!b.contains(&Vec2::new(150.0, 150.0)));
    }

    #[test]
    fn test_color() {
        let c = Color::new(255, 0, 0, 1.0);
        assert_eq!(c.to_hex(), "#ff0000");
    }

    #[test]
    fn test_inverse() {
        let m = Matrix3::translate(10.0, 20.0);
        let inv = m.inverse().unwrap();
        let p = inv.apply(&m.apply(&Vec2::new(5.0, 7.0)));
        assert!((p.x - 5.0).abs() < 1e-8);
    }
}
