//! DKIM canonicalization (RFC 6376 §3.4).
//!
//! This is where DKIM implementations go wrong, and the failure is
//! asymmetric: a signature that verifies here but nowhere else means every
//! message we send is treated as forged. So both algorithms are implemented
//! exactly as specified and tested against the RFC's own example vectors
//! rather than against our own expectations.

/// Which canonicalization to apply.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Canon {
    /// Byte-for-byte, tolerating nothing.
    Simple,
    /// Tolerates the whitespace and case changes mail systems inflict in
    /// transit. Nearly everyone signs with this because `simple` breaks the
    /// moment any relay reflows a header.
    Relaxed,
}

impl Canon {
    pub fn as_str(self) -> &'static str {
        match self {
            Canon::Simple => "simple",
            Canon::Relaxed => "relaxed",
        }
    }

    pub fn parse(s: &str) -> Option<Self> {
        match s.trim().to_ascii_lowercase().as_str() {
            "simple" => Some(Canon::Simple),
            "relaxed" => Some(Canon::Relaxed),
            _ => None,
        }
    }
}

/// Canonicalize one header field.
///
/// `name` is the field name as it appeared; `value` is everything after the
/// colon, already unfolded into one line.
pub fn header(canon: Canon, name: &str, value: &str) -> String {
    match canon {
        // Simple keeps the field exactly as it arrived, including its original
        // case and internal whitespace.
        Canon::Simple => format!("{name}:{value}\r\n"),
        Canon::Relaxed => {
            // Lowercase the name, drop whitespace around the colon, collapse
            // runs of whitespace inside the value, strip trailing whitespace.
            let name = name.trim().to_ascii_lowercase();
            let collapsed = collapse_wsp(value.trim());
            format!("{name}:{collapsed}\r\n")
        }
    }
}

/// Canonicalize the message body.
pub fn body(canon: Canon, raw: &[u8]) -> Vec<u8> {
    let mut lines: Vec<Vec<u8>> = split_crlf_lines(raw);

    if canon == Canon::Relaxed {
        // Strip trailing whitespace on every line and collapse internal runs.
        for line in lines.iter_mut() {
            let text = String::from_utf8_lossy(line).into_owned();
            *line = collapse_wsp(text.trim_end()).into_bytes();
        }
    }

    // Both algorithms delete trailing empty lines, then end with exactly one
    // CRLF. An empty body canonicalizes to nothing at all under relaxed, and
    // to a single CRLF under simple — a difference that trips people up.
    while lines.last().map(|l| l.is_empty()).unwrap_or(false) {
        lines.pop();
    }

    if lines.is_empty() {
        return match canon {
            Canon::Simple => b"\r\n".to_vec(),
            Canon::Relaxed => Vec::new(),
        };
    }

    let mut out = Vec::with_capacity(raw.len() + 2);
    for line in lines {
        out.extend_from_slice(&line);
        out.extend_from_slice(b"\r\n");
    }
    out
}

/// Split on CRLF or bare LF, dropping the terminators.
fn split_crlf_lines(raw: &[u8]) -> Vec<Vec<u8>> {
    let mut lines = Vec::new();
    let mut current = Vec::new();
    let mut i = 0;
    while i < raw.len() {
        match raw[i] {
            b'\r' if raw.get(i + 1) == Some(&b'\n') => {
                lines.push(std::mem::take(&mut current));
                i += 2;
            }
            b'\n' => {
                lines.push(std::mem::take(&mut current));
                i += 1;
            }
            b => {
                current.push(b);
                i += 1;
            }
        }
    }
    // Content after the last line ending is still a line.
    if !current.is_empty() {
        lines.push(current);
    }
    lines
}

/// Collapse every run of spaces and tabs into a single space.
fn collapse_wsp(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut in_wsp = false;
    for ch in s.chars() {
        if ch == ' ' || ch == '\t' {
            if !in_wsp {
                out.push(' ');
                in_wsp = true;
            }
        } else {
            out.push(ch);
            in_wsp = false;
        }
    }
    out
}
