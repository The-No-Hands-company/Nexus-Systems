/// An SMTP reply code, and what it means for a delivery attempt.
///
/// The 4xx/5xx split is the whole of mail's retry contract: 4xx means try
/// again, 5xx means stop and tell the sender. Getting it backwards either
/// loses mail that would have been delivered, or hammers a server that has
/// already said no — and mistaking a 4xx for a 5xx is the more damaging of the
/// two, because the mail is gone.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Disposition {
    Ok,
    /// Try again later.
    Temporary,
    /// Never going to work.
    Permanent,
}

pub fn classify(code: u16) -> Disposition {
    match code {
        200..=299 | 354 => Disposition::Ok,
        // 4xx is explicitly "try again". So is anything we cannot make sense
        // of: treating an unreadable reply as permanent would throw mail away
        // on the strength of a guess.
        400..=499 => Disposition::Temporary,
        500..=599 => Disposition::Permanent,
        _ => Disposition::Temporary,
    }
}

/// Parse the leading three-digit code from a reply line.
pub fn code_of(line: &str) -> Option<u16> {
    let digits: String = line.chars().take(3).collect();
    digits.parse().ok()
}

/// True when this line is the last of a multi-line reply.
///
/// Every line but the last has a hyphen in the fourth position. A client that
/// ignores this falls one reply behind for the rest of the conversation and
/// misreads every response after EHLO.
pub fn is_final_line(line: &str) -> bool {
    line.as_bytes().get(3) != Some(&b'-')
}
