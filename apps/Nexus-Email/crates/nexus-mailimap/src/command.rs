/// An IMAP command, tagged as the protocol requires.
///
/// Every command carries a tag the client chose, and every response must echo
/// it back. A client that receives a response with the wrong tag treats the
/// connection as broken, so the tag is threaded through everything here.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tagged {
    pub tag: String,
    pub command: Command,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Command {
    Capability,
    Noop,
    Logout,
    Login { user: String, password: String },
    List { reference: String, pattern: String },
    Select { mailbox: String },
    Examine { mailbox: String },
    /// UID FETCH <set> <items>
    UidFetch { set: String, items: String },
    /// UID STORE <set> <±FLAGS[.SILENT]> <flags>
    UidStore { set: String, action: String, flags: String },
    UidSearch { criteria: String },
    Close,
    Unknown(String),
}

/// Parse one command line.
///
/// Literals (`{123}` continuations) are not supported: they matter for APPEND
/// and for passwords containing awkward bytes, and this server implements
/// neither. An unsupported form becomes `Unknown` and earns a tagged BAD
/// rather than being half-interpreted.
pub fn parse(line: &str) -> Option<Tagged> {
    let line = line.trim_end_matches(['\r', '\n']);
    let mut parts = line.splitn(3, ' ');
    let tag = parts.next()?.to_string();
    if tag.is_empty() {
        return None;
    }
    let verb = parts.next().unwrap_or("").to_ascii_uppercase();
    let rest = parts.next().unwrap_or("").trim().to_string();

    let command = match verb.as_str() {
        "CAPABILITY" => Command::Capability,
        "NOOP" => Command::Noop,
        "LOGOUT" => Command::Logout,
        "CLOSE" => Command::Close,
        "LOGIN" => {
            let (user, password) = split_two(&rest)?;
            Command::Login { user: unquote(&user), password: unquote(&password) }
        }
        "LIST" => {
            let (reference, pattern) = split_two(&rest)?;
            Command::List { reference: unquote(&reference), pattern: unquote(&pattern) }
        }
        "SELECT" => Command::Select { mailbox: unquote(rest.trim()) },
        "EXAMINE" => Command::Examine { mailbox: unquote(rest.trim()) },
        "UID" => {
            let mut sub = rest.splitn(3, ' ');
            let op = sub.next().unwrap_or("").to_ascii_uppercase();
            let a = sub.next().unwrap_or("").to_string();
            let b = sub.next().unwrap_or("").to_string();
            match op.as_str() {
                "FETCH" => Command::UidFetch { set: a, items: b },
                "SEARCH" => Command::UidSearch { criteria: format!("{a} {b}").trim().to_string() },
                "STORE" => {
                    let mut s = b.splitn(2, ' ');
                    let action = s.next().unwrap_or("").to_string();
                    let flags = s.next().unwrap_or("").to_string();
                    Command::UidStore { set: a, action, flags }
                }
                other => Command::Unknown(other.to_string()),
            }
        }
        other => Command::Unknown(other.to_string()),
    };

    Some(Tagged { tag, command })
}

fn split_two(s: &str) -> Option<(String, String)> {
    // Quoted strings may contain spaces, so a naive split is wrong for
    // passwords and mailbox names alike.
    if let Some(after_quote) = s.strip_prefix('"') {
        let end = after_quote.find('"')?;
        let first = after_quote[..end].to_string();
        let rest = after_quote[end + 1..].trim().to_string();
        return Some((first, rest));
    }
    let mut it = s.splitn(2, ' ');
    Some((it.next()?.to_string(), it.next().unwrap_or("").trim().to_string()))
}

fn unquote(s: &str) -> String {
    s.trim().trim_matches('"').to_string()
}

/// Expand a UID set — `1:5`, `3`, `1,3:7`, `2:*` — against the known UIDs.
///
/// `*` is the highest UID that exists, not "unbounded": a client asking for
/// `1:*` on an empty mailbox must get nothing rather than an error, and one
/// asking for `100:*` when the highest is 50 must get the highest message,
/// which is what RFC 3501 requires and what clients rely on for "fetch new".
pub fn expand_set(set: &str, known: &[i64]) -> Vec<i64> {
    let Some(&max) = known.iter().max() else {
        return Vec::new();
    };

    let mut out = Vec::new();
    for part in set.split(',') {
        let part = part.trim();
        if let Some((lo, hi)) = part.split_once(':') {
            let lo = parse_point(lo, max);
            let hi = parse_point(hi, max);
            let (lo, hi) = if lo <= hi { (lo, hi) } else { (hi, lo) };
            out.extend(known.iter().copied().filter(|u| *u >= lo && *u <= hi));
        } else if let Some(u) = parse_point_opt(part, max) {
            if known.contains(&u) {
                out.push(u);
            }
        }
    }
    out.sort_unstable();
    out.dedup();
    out
}

fn parse_point(s: &str, max: i64) -> i64 {
    parse_point_opt(s, max).unwrap_or(max)
}

fn parse_point_opt(s: &str, max: i64) -> Option<i64> {
    let s = s.trim();
    if s == "*" {
        Some(max)
    } else {
        s.parse().ok()
    }
}
