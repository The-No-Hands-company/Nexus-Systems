use crate::encoding::{decode_base64, decode_charset, decode_quoted_printable};
use crate::headers::Headers;
use crate::parse::{parse, Limits, ParsedMessage};

/// A parsed Content-Type: the type itself plus its parameters.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ContentType {
    pub mime_type: String,
    pub params: Vec<(String, String)>,
}

impl ContentType {
    pub fn parse(raw: &str) -> Self {
        let mut parts = raw.split(';');
        let mime_type = parts
            .next()
            .unwrap_or("text/plain")
            .trim()
            .to_ascii_lowercase();

        let mut params = Vec::new();
        for p in parts {
            if let Some((k, v)) = p.split_once('=') {
                let v = v.trim().trim_matches('"').to_string();
                params.push((k.trim().to_ascii_lowercase(), v));
            }
        }
        Self { mime_type, params }
    }

    pub fn param(&self, name: &str) -> Option<&str> {
        self.params
            .iter()
            .find(|(k, _)| k.eq_ignore_ascii_case(name))
            .map(|(_, v)| v.as_str())
    }

    pub fn is_multipart(&self) -> bool {
        self.mime_type.starts_with("multipart/")
    }
}

/// One MIME part: its headers, its decoded content, and its children.
#[derive(Debug, Clone)]
pub struct Part {
    pub headers: Headers,
    pub content_type: ContentType,
    /// Decoded bytes — transfer encoding already removed. Empty for multipart
    /// containers, whose content is their children.
    pub body: Vec<u8>,
    pub children: Vec<Part>,
}

impl Part {
    pub fn filename(&self) -> Option<String> {
        // RFC 2183 puts it on Content-Disposition; older senders put it on
        // Content-Type. Both appear in the wild.
        if let Some(cd) = self.headers.get("content-disposition") {
            let disp = ContentType::parse(cd);
            if let Some(name) = disp.param("filename") {
                return Some(name.to_string());
            }
        }
        self.content_type.param("name").map(str::to_string)
    }

    /// True when this part is an attachment rather than displayable body.
    pub fn is_attachment(&self) -> bool {
        self.headers
            .get("content-disposition")
            .map(|cd| cd.trim().to_ascii_lowercase().starts_with("attachment"))
            .unwrap_or(false)
            || (self.filename().is_some() && !self.content_type.mime_type.starts_with("text/"))
    }

    /// Body decoded as text using the part's declared charset.
    pub fn text(&self) -> String {
        let charset = self.content_type.param("charset").unwrap_or("utf-8");
        decode_charset(&self.body, charset)
    }

    /// Depth-first walk over this part and everything beneath it.
    pub fn walk(&self) -> Vec<&Part> {
        let mut out = vec![self];
        for child in &self.children {
            out.extend(child.walk());
        }
        out
    }
}

/// How deep a MIME tree may nest.
///
/// Unbounded recursion on attacker-supplied structure is a stack overflow
/// waiting to happen, and a message nested 100 deep is hostile, not unusual.
const MAX_DEPTH: usize = 32;

/// Build the MIME tree for an already-parsed message.
pub fn tree(msg: &ParsedMessage) -> Part {
    build(&msg.headers, &msg.body, 0)
}

fn build(headers: &Headers, body: &[u8], depth: usize) -> Part {
    let content_type = ContentType::parse(
        headers.get("content-type").unwrap_or("text/plain"),
    );

    if content_type.is_multipart() && depth < MAX_DEPTH {
        if let Some(boundary) = content_type.param("boundary") {
            let children = split_multipart(body, boundary)
                .into_iter()
                .map(|raw| match parse(&raw, Limits::default()) {
                    Ok(sub) => build(&sub.headers, &sub.body, depth + 1),
                    // A part with no header/body separator is all body. Real
                    // senders produce these; dropping them loses content.
                    Err(_) => Part {
                        headers: Headers::new(),
                        content_type: ContentType::parse("text/plain"),
                        body: raw,
                        children: Vec::new(),
                    },
                })
                .collect();

            return Part {
                headers: headers.clone(),
                content_type,
                body: Vec::new(),
                children,
            };
        }
    }

    let encoding = headers
        .get("content-transfer-encoding")
        .unwrap_or("7bit")
        .trim()
        .to_ascii_lowercase();

    let decoded = match encoding.as_str() {
        "base64" => decode_base64(body),
        "quoted-printable" => decode_quoted_printable(body),
        // 7bit, 8bit, binary, and anything unrecognised: the bytes are the
        // content. Guessing at an unknown encoding corrupts more than it fixes.
        _ => body.to_vec(),
    };

    Part {
        headers: headers.clone(),
        content_type,
        body: decoded,
        children: Vec::new(),
    }
}

/// Split a multipart body on its boundary.
fn split_multipart(body: &[u8], boundary: &str) -> Vec<Vec<u8>> {
    let delim = format!("--{boundary}");
    let delim = delim.as_bytes();
    let mut parts = Vec::new();
    let mut current: Option<Vec<u8>> = None;

    for line in body.split(|b| *b == b'\n') {
        let trimmed = line.strip_suffix(b"\r").unwrap_or(line);

        if trimmed.starts_with(delim) {
            if let Some(part) = current.take() {
                parts.push(part);
            }
            // "--boundary--" closes the multipart; anything after it is epilogue.
            if trimmed[delim.len()..].starts_with(b"--") {
                return parts;
            }
            current = Some(Vec::new());
            continue;
        }

        if let Some(buf) = current.as_mut() {
            buf.extend_from_slice(line);
            buf.push(b'\n');
        }
        // Before the first boundary is the preamble, which is not a part.
    }

    if let Some(part) = current.take() {
        parts.push(part);
    }
    parts
}
