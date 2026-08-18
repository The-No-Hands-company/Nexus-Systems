/// An SMTP command, as far as we are willing to understand one.
///
/// Parsing is deliberately narrow: anything not recognised becomes
/// `Unknown` and earns a 500 rather than being guessed at. This is a public
/// port, and a lenient command parser is an attack surface — unlike message
/// bodies, where leniency saves real mail, there is no legitimate sender
/// relying on us to interpret a malformed verb.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Command {
    Ehlo(String),
    Helo(String),
    MailFrom(String),
    RcptTo(String),
    Data,
    Rset,
    Noop,
    Quit,
    StartTls,
    Auth,
    /// A recognised verb we refuse on purpose. VRFY and EXPN let a stranger
    /// enumerate who exists here, which is reconnaissance, so they are
    /// answered but never truthfully.
    Refused(&'static str),
    Unknown,
}

/// RFC 5321 §4.5.3.1.4: command lines are at most 512 bytes including CRLF.
pub const MAX_COMMAND_LINE: usize = 512;

pub fn parse(line: &str) -> Command {
    let line = line.trim_end_matches(['\r', '\n']);
    let (verb, rest) = match line.split_once(' ') {
        Some((v, r)) => (v, r.trim()),
        None => (line, ""),
    };

    match verb.to_ascii_uppercase().as_str() {
        "EHLO" => Command::Ehlo(rest.to_string()),
        "HELO" => Command::Helo(rest.to_string()),
        "MAIL" => match address_in(rest, "FROM:") {
            Some(a) => Command::MailFrom(a),
            None => Command::Unknown,
        },
        "RCPT" => match address_in(rest, "TO:") {
            Some(a) => Command::RcptTo(a),
            None => Command::Unknown,
        },
        "DATA" => Command::Data,
        "RSET" => Command::Rset,
        "NOOP" => Command::Noop,
        "QUIT" => Command::Quit,
        "STARTTLS" => Command::StartTls,
        "AUTH" => Command::Auth,
        "VRFY" => Command::Refused("VRFY"),
        "EXPN" => Command::Refused("EXPN"),
        _ => Command::Unknown,
    }
}

/// Extract the address from `FROM:<a@b>` / `TO:<a@b>`, tolerating the space
/// some clients insert after the colon and any trailing ESMTP parameters.
fn address_in(rest: &str, prefix: &str) -> Option<String> {
    let upper = rest.to_ascii_uppercase();
    let idx = upper.find(prefix)?;
    let after = rest[idx + prefix.len()..].trim_start();

    if let Some(open) = after.find('<') {
        let close = after[open + 1..].find('>')?;
        return Some(after[open + 1..open + 1 + close].trim().to_string());
    }
    // Angle brackets are required by the grammar, but enough senders omit them
    // that refusing would lose real mail. Take the first token.
    let token = after.split_whitespace().next()?;
    (!token.is_empty()).then(|| token.to_string())
}
