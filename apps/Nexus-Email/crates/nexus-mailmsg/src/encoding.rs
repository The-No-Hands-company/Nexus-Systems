use base64::{engine::general_purpose::STANDARD, Engine};

/// Decode Quoted-Printable (RFC 2045 §6.7).
///
/// Written out rather than pulled from a crate because it is a mail protocol
/// decision, not a cryptographic primitive: what to do with a malformed escape
/// is a choice, and the choice here is to pass it through literally, which is
/// what every mail client does and what keeps a stray `=` from destroying a
/// message body.
pub fn decode_quoted_printable(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(input.len());
    let mut i = 0;
    while i < input.len() {
        match input[i] {
            b'=' => {
                // Soft line break: "=" at end of line means "no break here".
                if input.get(i + 1) == Some(&b'\r') && input.get(i + 2) == Some(&b'\n') {
                    i += 3;
                } else if input.get(i + 1) == Some(&b'\n') {
                    i += 2;
                } else if let (Some(h), Some(l)) = (input.get(i + 1), input.get(i + 2)) {
                    match (hex_val(*h), hex_val(*l)) {
                        (Some(h), Some(l)) => {
                            out.push(h << 4 | l);
                            i += 3;
                        }
                        // Not a valid escape — emit the '=' as-is.
                        _ => {
                            out.push(b'=');
                            i += 1;
                        }
                    }
                } else {
                    out.push(b'=');
                    i += 1;
                }
            }
            b => {
                out.push(b);
                i += 1;
            }
        }
    }
    out
}

pub fn encode_quoted_printable(input: &[u8]) -> String {
    let mut out = String::new();
    let mut line_len = 0;
    for &b in input {
        let literal = matches!(b, 33..=60 | 62..=126 | b' ' | b'\t');
        let chunk_len = if literal { 1 } else { 3 };
        // RFC 2045 caps encoded lines at 76 characters including the soft break.
        if line_len + chunk_len > 75 {
            out.push_str("=\r\n");
            line_len = 0;
        }
        if literal {
            out.push(b as char);
        } else {
            out.push_str(&format!("={:02X}", b));
        }
        line_len += chunk_len;
    }
    out
}

fn hex_val(b: u8) -> Option<u8> {
    match b {
        b'0'..=b'9' => Some(b - b'0'),
        b'A'..=b'F' => Some(b - b'A' + 10),
        b'a'..=b'f' => Some(b - b'a' + 10),
        _ => None,
    }
}

pub fn decode_base64(input: &[u8]) -> Vec<u8> {
    // Mail wraps base64 across lines, and some senders pad wrongly. Strip
    // whitespace and decode what remains, falling back to the raw bytes rather
    // than losing the part entirely.
    let cleaned: Vec<u8> = input
        .iter()
        .copied()
        .filter(|b| !b.is_ascii_whitespace())
        .collect();
    STANDARD.decode(&cleaned).unwrap_or_else(|_| input.to_vec())
}

pub fn encode_base64(input: &[u8]) -> String {
    let encoded = STANDARD.encode(input);
    // Wrapped at 76 characters, as RFC 2045 requires.
    encoded
        .as_bytes()
        .chunks(76)
        .map(|c| String::from_utf8_lossy(c).into_owned())
        .collect::<Vec<_>>()
        .join("\r\n")
}

/// Decode RFC 2047 encoded-words: `=?charset?B?...?=` / `=?charset?Q?...?=`.
///
/// This is how a Subject carries anything that is not ASCII. Text outside an
/// encoded word is passed through, and an encoded word we cannot decode is
/// left exactly as it was — showing `=?utf-8?B?...?=` is ugly, but inventing
/// replacement characters silently corrupts the subject.
pub fn decode_encoded_words(input: &str) -> String {
    let mut out = String::new();
    let mut rest = input;

    while let Some(start) = rest.find("=?") {
        out.push_str(&rest[..start]);
        let after = &rest[start + 2..];

        let Some(decoded_end) = decode_one_word(after) else {
            out.push_str("=?");
            rest = after;
            continue;
        };
        let (decoded, consumed) = decoded_end;
        out.push_str(&decoded);
        rest = &after[consumed..];
    }
    out.push_str(rest);
    out
}

/// Decode a single encoded word body (everything after the opening `=?`),
/// returning the text and how many bytes were consumed including the `?=`.
fn decode_one_word(after: &str) -> Option<(String, usize)> {
    let mut parts = after.splitn(3, '?');
    let charset = parts.next()?;
    let encoding = parts.next()?;
    let remainder = parts.next()?;
    let end = remainder.find("?=")?;
    let payload = &remainder[..end];

    let bytes = match encoding.to_ascii_uppercase().as_str() {
        "B" => decode_base64(payload.as_bytes()),
        // In the Q encoding, underscore means space — a difference from
        // ordinary quoted-printable that is easy to miss and shows up as
        // words_run_together.
        "Q" => decode_quoted_printable(&payload.replace('_', " ").into_bytes()),
        _ => return None,
    };

    let label = charset.split('*').next().unwrap_or(charset);
    let text = decode_charset(&bytes, label);
    let consumed = charset.len() + 1 + encoding.len() + 1 + end + 2;
    Some((text, consumed))
}

/// Interpret bytes in a named charset. Mail predates UTF-8 and still carries
/// ISO-8859-*, Windows-125*, Shift_JIS and more; refusing them would mean
/// mangling perfectly ordinary mail from outside the Anglosphere.
pub fn decode_charset(bytes: &[u8], label: &str) -> String {
    match encoding_rs::Encoding::for_label(label.as_bytes()) {
        Some(enc) => enc.decode(bytes).0.into_owned(),
        None => String::from_utf8_lossy(bytes).into_owned(),
    }
}
