use crate::encoding::{encode_base64, encode_quoted_printable};
use chrono::{DateTime, Utc};

/// An attachment to be included in a generated message.
pub struct Attachment {
    pub filename: String,
    pub mime_type: String,
    pub data: Vec<u8>,
}

/// Builder for an RFC 5322 message.
///
/// Generates CRLF line endings throughout, because SMTP requires them and a
/// message assembled with bare LF is rejected or silently rewritten in transit
/// — which breaks a DKIM signature computed over the original bytes.
pub struct MessageBuilder {
    from: String,
    to: Vec<String>,
    cc: Vec<String>,
    subject: String,
    date: DateTime<Utc>,
    message_id: String,
    in_reply_to: Option<String>,
    references: Vec<String>,
    text: String,
    html: Option<String>,
    attachments: Vec<Attachment>,
    extra: Vec<(String, String)>,
}

impl MessageBuilder {
    pub fn new(from: impl Into<String>, message_id: impl Into<String>) -> Self {
        Self {
            from: from.into(),
            to: Vec::new(),
            cc: Vec::new(),
            subject: String::new(),
            date: Utc::now(),
            message_id: message_id.into(),
            in_reply_to: None,
            references: Vec::new(),
            text: String::new(),
            html: None,
            attachments: Vec::new(),
            extra: Vec::new(),
        }
    }

    pub fn to(mut self, addr: impl Into<String>) -> Self {
        self.to.push(addr.into());
        self
    }

    pub fn cc(mut self, addr: impl Into<String>) -> Self {
        self.cc.push(addr.into());
        self
    }

    pub fn subject(mut self, s: impl Into<String>) -> Self {
        self.subject = s.into();
        self
    }

    pub fn date(mut self, d: DateTime<Utc>) -> Self {
        self.date = d;
        self
    }

    pub fn in_reply_to(mut self, id: impl Into<String>) -> Self {
        self.in_reply_to = Some(id.into());
        self
    }

    pub fn references(mut self, ids: Vec<String>) -> Self {
        self.references = ids;
        self
    }

    pub fn text(mut self, body: impl Into<String>) -> Self {
        self.text = body.into();
        self
    }

    pub fn html(mut self, body: impl Into<String>) -> Self {
        self.html = Some(body.into());
        self
    }

    pub fn attach(mut self, a: Attachment) -> Self {
        self.attachments.push(a);
        self
    }

    pub fn header(mut self, name: impl Into<String>, value: impl Into<String>) -> Self {
        self.extra.push((name.into(), value.into()));
        self
    }

    pub fn build(self) -> Vec<u8> {
        let mut out = String::new();

        out.push_str(&format!("From: {}\r\n", self.from));
        if !self.to.is_empty() {
            out.push_str(&format!("To: {}\r\n", self.to.join(", ")));
        }
        if !self.cc.is_empty() {
            out.push_str(&format!("Cc: {}\r\n", self.cc.join(", ")));
        }
        out.push_str(&format!("Subject: {}\r\n", encode_header_value(&self.subject)));
        // RFC 5322 date format. Not ISO 8601: mail has its own, older, format
        // and clients that parse strictly will reject anything else.
        out.push_str(&format!(
            "Date: {}\r\n",
            self.date.format("%a, %d %b %Y %H:%M:%S +0000")
        ));
        out.push_str(&format!("Message-ID: {}\r\n", self.message_id));
        if let Some(irt) = &self.in_reply_to {
            out.push_str(&format!("In-Reply-To: {irt}\r\n"));
        }
        if !self.references.is_empty() {
            out.push_str(&format!("References: {}\r\n", self.references.join(" ")));
        }
        for (name, value) in &self.extra {
            out.push_str(&format!("{name}: {value}\r\n"));
        }
        out.push_str("MIME-Version: 1.0\r\n");

        let boundary = format!("=_nexus_{:x}", self.date.timestamp_nanos_opt().unwrap_or(0));
        let alt_boundary = format!("{boundary}_alt");

        match (self.html.is_some(), self.attachments.is_empty()) {
            // Plain text only.
            (false, true) => {
                out.push_str("Content-Type: text/plain; charset=utf-8\r\n");
                out.push_str("Content-Transfer-Encoding: quoted-printable\r\n\r\n");
                out.push_str(&encode_quoted_printable(self.text.as_bytes()));
            }
            // Text and HTML, no attachments.
            (true, true) => {
                out.push_str(&format!(
                    "Content-Type: multipart/alternative; boundary=\"{alt_boundary}\"\r\n\r\n"
                ));
                push_alternative(&mut out, &alt_boundary, &self.text, self.html.as_deref());
            }
            // Anything with attachments becomes multipart/mixed, with the
            // displayable content as the first part.
            (_, false) => {
                out.push_str(&format!(
                    "Content-Type: multipart/mixed; boundary=\"{boundary}\"\r\n\r\n"
                ));
                out.push_str(&format!("--{boundary}\r\n"));
                if let Some(html) = self.html.as_deref() {
                    out.push_str(&format!(
                        "Content-Type: multipart/alternative; boundary=\"{alt_boundary}\"\r\n\r\n"
                    ));
                    push_alternative(&mut out, &alt_boundary, &self.text, Some(html));
                } else {
                    out.push_str("Content-Type: text/plain; charset=utf-8\r\n");
                    out.push_str("Content-Transfer-Encoding: quoted-printable\r\n\r\n");
                    out.push_str(&encode_quoted_printable(self.text.as_bytes()));
                }
                out.push_str("\r\n");

                for a in &self.attachments {
                    out.push_str(&format!("--{boundary}\r\n"));
                    out.push_str(&format!(
                        "Content-Type: {}; name=\"{}\"\r\n",
                        a.mime_type, a.filename
                    ));
                    out.push_str("Content-Transfer-Encoding: base64\r\n");
                    out.push_str(&format!(
                        "Content-Disposition: attachment; filename=\"{}\"\r\n\r\n",
                        a.filename
                    ));
                    out.push_str(&encode_base64(&a.data));
                    out.push_str("\r\n");
                }
                out.push_str(&format!("--{boundary}--\r\n"));
            }
        }

        out.into_bytes()
    }
}

fn push_alternative(out: &mut String, boundary: &str, text: &str, html: Option<&str>) {
    out.push_str(&format!("--{boundary}\r\n"));
    out.push_str("Content-Type: text/plain; charset=utf-8\r\n");
    out.push_str("Content-Transfer-Encoding: quoted-printable\r\n\r\n");
    out.push_str(&encode_quoted_printable(text.as_bytes()));
    out.push_str("\r\n");

    if let Some(html) = html {
        out.push_str(&format!("--{boundary}\r\n"));
        out.push_str("Content-Type: text/html; charset=utf-8\r\n");
        out.push_str("Content-Transfer-Encoding: quoted-printable\r\n\r\n");
        out.push_str(&encode_quoted_printable(html.as_bytes()));
        out.push_str("\r\n");
    }
    out.push_str(&format!("--{boundary}--\r\n"));
}

/// Encode a header value as an RFC 2047 encoded word when it is not pure
/// ASCII. Headers are an ASCII protocol; a raw UTF-8 subject is mangled by
/// anything strict.
fn encode_header_value(value: &str) -> String {
    if value.is_ascii() {
        return value.to_string();
    }
    format!("=?utf-8?B?{}?=", base64_encode_plain(value.as_bytes()))
}

fn base64_encode_plain(bytes: &[u8]) -> String {
    use base64::{engine::general_purpose::STANDARD, Engine};
    STANDARD.encode(bytes)
}
