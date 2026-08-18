use crate::error::{MsgError, Result};
use crate::headers::Headers;

/// Caps applied while parsing. A public MTA parses bytes chosen by strangers,
/// so every loop here is bounded. Defaults are generous enough for real mail
/// and small enough that one message cannot exhaust memory.
#[derive(Debug, Clone, Copy)]
pub struct Limits {
    pub max_message: usize,
    pub max_headers: usize,
    pub max_header_count: usize,
}

impl Default for Limits {
    fn default() -> Self {
        Self {
            // 50 MiB: larger than any sane message, and the SMTP layer will
            // advertise a smaller SIZE anyway.
            max_message: 50 * 1024 * 1024,
            max_headers: 1024 * 1024,
            // Header-count floods are a real denial-of-service technique.
            max_header_count: 2_000,
        }
    }
}

/// A parsed message: the header block, and the body exactly as it appeared.
///
/// The body is kept as raw bytes rather than a string. Mail is not
/// necessarily UTF-8, is frequently mislabelled, and re-encoding it here would
/// break DKIM, whose signature covers the bytes that arrived.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedMessage {
    pub headers: Headers,
    pub body: Vec<u8>,
}

/// Split a message into headers and body, unfolding folded header lines.
///
/// Deliberately lenient, because real mail is. It accepts bare LF line endings
/// (extremely common), a missing trailing newline, and continuation lines that
/// use either space or tab. It skips lines in the header block that contain no
/// colon instead of rejecting the message: a single broken header is not a
/// reason to lose someone's mail.
pub fn parse(raw: &[u8], limits: Limits) -> Result<ParsedMessage> {
    if raw.len() > limits.max_message {
        return Err(MsgError::MessageTooLarge { limit: limits.max_message });
    }

    let split = find_separator(raw).ok_or(MsgError::NoSeparator)?;
    let (header_bytes, body) = raw.split_at(split.headers_end);
    if header_bytes.len() > limits.max_headers {
        return Err(MsgError::HeadersTooLarge { limit: limits.max_headers });
    }
    let body = &body[split.body_offset..];

    let mut headers = Headers::new();
    let mut current: Option<(String, String)> = None;

    for line in split_lines(header_bytes) {
        // Header values are decoded lossily: a header carrying invalid UTF-8
        // is malformed, but dropping the whole message over it would lose
        // mail that every other client displays fine.
        let text = String::from_utf8_lossy(line);
        let text = text.trim_end_matches(['\r', '\n']);
        if text.is_empty() {
            continue;
        }

        // A leading space or tab continues the previous header.
        if text.starts_with(' ') || text.starts_with('\t') {
            if let Some((_, value)) = current.as_mut() {
                value.push(' ');
                value.push_str(text.trim_start());
            }
            // A continuation with nothing to continue is garbage; drop it.
            continue;
        }

        if let Some((name, value)) = current.take() {
            headers.push(name, value);
            if headers.len() >= limits.max_header_count {
                return Err(MsgError::HeadersTooLarge { limit: limits.max_headers });
            }
        }

        match text.split_once(':') {
            Some((name, value)) => {
                current = Some((name.trim_end().to_string(), value.trim_start().to_string()));
            }
            // No colon: not a header. Skip it rather than abandoning the message.
            None => current = None,
        }
    }
    if let Some((name, value)) = current.take() {
        headers.push(name, value);
    }

    Ok(ParsedMessage { headers, body: body.to_vec() })
}

struct Separator {
    headers_end: usize,
    body_offset: usize,
}

/// Find the blank line ending the header block, accepting CRLFCRLF, LFLF, and
/// the mixed forms that appear when something along the path rewrites line
/// endings badly.
fn find_separator(raw: &[u8]) -> Option<Separator> {
    let mut i = 0;
    while i < raw.len() {
        if raw[i] == b'\n' {
            // \n\n
            if raw.get(i + 1) == Some(&b'\n') {
                return Some(Separator { headers_end: i + 1, body_offset: 1 });
            }
            // \n\r\n
            if raw.get(i + 1) == Some(&b'\r') && raw.get(i + 2) == Some(&b'\n') {
                return Some(Separator { headers_end: i + 1, body_offset: 2 });
            }
        }
        i += 1;
    }
    None
}

fn split_lines(bytes: &[u8]) -> impl Iterator<Item = &[u8]> {
    bytes.split(|b| *b == b'\n')
}
