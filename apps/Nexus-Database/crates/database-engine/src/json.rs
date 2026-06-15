//! JSON/Document Support — JSONB type with -> and ->> operators.
//!
//! Adds document-store capabilities to the relational engine.
//! JSONB columns store arbitrary nested JSON structures.
//!
//! SQL syntax:
//!   CREATE TABLE users (id INTEGER, profile JSONB);
//!   INSERT INTO users VALUES (1, '{"name":"alice","role":"admin"}');
//!   SELECT profile->>'name' FROM users WHERE profile->>'role' = 'admin';
//!   SELECT * FROM users WHERE profile @> '{"role":"admin"}';

use serde_json::{Value, Map};
use crate::catalog::ColumnType;

// ── JSONB value extraction ─────────────────────────────────────

/// Parse a JSONB byte array into a serde_json::Value.
pub fn parse_jsonb(data: &[u8]) -> Option<Value> {
    serde_json::from_slice(data).ok()
}

/// Extract a field from JSONB using the `->` operator (returns JSON).
/// e.g. `profile->'name'` → `"alice"` (JSON string)
pub fn json_get_field(data: &[u8], field: &str) -> Option<Vec<u8>> {
    let val = parse_jsonb(data)?;
    match val {
        Value::Object(ref map) => {
            map.get(field).map(|v| serde_json::to_vec(v).unwrap_or_default())
        }
        _ => None,
    }
}

/// Extract a field from JSONB using the `->>` operator (returns text).
/// e.g. `profile->>'name'` → `alice` (plain text, no quotes)
pub fn json_get_field_text(data: &[u8], field: &str) -> Option<String> {
    let val = parse_jsonb(data)?;
    match val {
        Value::Object(ref map) => {
            map.get(field).map(|v| match v {
                Value::String(s) => s.clone(),
                Value::Number(n) => n.to_string(),
                Value::Bool(b) => b.to_string(),
                Value::Null => "null".to_string(),
                other => other.to_string(),
            })
        }
        _ => None,
    }
}

/// Check if a JSONB value contains another JSON value (`@>` operator).
/// e.g. `profile @> '{"role":"admin"}'` → true if profile has role=admin
pub fn json_contains(data: &[u8], contained: &[u8]) -> bool {
    let doc = match parse_jsonb(data) { Some(v) => v, None => return false };
    let query = match parse_jsonb(contained) { Some(v) => v, None => return false };

    match (&doc, &query) {
        (Value::Object(doc_map), Value::Object(query_map)) => {
            // Every key-value in query must exist in doc
            query_map.iter().all(|(k, qv)| {
                doc_map.get(k).map(|dv| json_values_equal(dv, qv)).unwrap_or(false)
            })
        }
        (Value::Array(doc_arr), Value::Array(query_arr)) => {
            // Every element in query must exist in doc
            query_arr.iter().all(|qv| doc_arr.iter().any(|dv| json_values_equal(dv, qv)))
        }
        _ => &doc == &query,
    }
}

/// Deep equality check for JSON values.
fn json_values_equal(a: &Value, b: &Value) -> bool {
    match (a, b) {
        (Value::String(sa), Value::String(sb)) => sa == sb,
        (Value::Number(na), Value::Number(nb)) => na.as_f64() == nb.as_f64(),
        (Value::Bool(ba), Value::Bool(bb)) => ba == bb,
        (Value::Null, Value::Null) => true,
        (Value::Array(aa), Value::Array(ab)) => {
            aa.len() == ab.len() && aa.iter().zip(ab.iter()).all(|(x, y)| json_values_equal(x, y))
        }
        (Value::Object(oa), Value::Object(ob)) => {
            oa.len() == ob.len() && oa.iter().all(|(k, v)| {
                ob.get(k).map(|bv| json_values_equal(v, bv)).unwrap_or(false)
            })
        }
        _ => a == b,
    }
}

/// Validate that a byte string is valid JSON (for INSERT/UPDATE validation).
pub fn validate_json(data: &[u8]) -> bool {
    serde_json::from_slice::<Value>(data).is_ok()
}

/// Pretty-print JSONB for display.
pub fn jsonb_display(data: &[u8]) -> String {
    parse_jsonb(data)
        .map(|v| serde_json::to_string(&v).unwrap_or_else(|_| "<invalid>".into()))
        .unwrap_or_else(|| "<invalid>".into())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_valid_json() {
        let data = b"{\"name\":\"alice\",\"age\":30}";
        assert!(validate_json(data));
        let val = parse_jsonb(data).unwrap();
        assert_eq!(val["name"], "alice");
        assert_eq!(val["age"], 30);
    }

    #[test]
    fn test_json_get_field() {
        let data = b"{\"name\":\"alice\",\"role\":\"admin\"}";
        let name = json_get_field_text(data, "name").unwrap();
        assert_eq!(name, "alice");
    }

    #[test]
    fn test_json_field_not_found() {
        let data = b"{\"name\":\"alice\"}";
        assert!(json_get_field_text(data, "missing").is_none());
    }

    #[test]
    fn test_json_contains_object() {
        let doc = b"{\"name\":\"alice\",\"role\":\"admin\",\"age\":30}";
        let query = b"{\"role\":\"admin\"}";
        assert!(json_contains(doc, query));

        let bad_query = b"{\"role\":\"user\"}";
        assert!(!json_contains(doc, bad_query));
    }

    #[test]
    fn test_json_contains_array() {
        let doc = b"[\"a\",\"b\",\"c\"]";
        let query = b"[\"a\"]";
        assert!(json_contains(doc, query));
    }

    #[test]
    fn test_validate_invalid_json() {
        assert!(!validate_json(b"not json"));
        assert!(!validate_json(b"{broken"));
    }
}
