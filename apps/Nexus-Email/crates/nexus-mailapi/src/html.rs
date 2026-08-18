//! Making a stranger's HTML safe to show.
//!
//! This is the one place in the mail client where getting it wrong is a
//! session compromise rather than a display glitch: the webmail lives on
//! app.<domain>, the same origin that holds the session cookie, so script
//! execution there is account takeover.
//!
//! Sanitising is done with `ammonia`, not by hand. Writing an HTML sanitiser is
//! the same category of mistake as writing a TLS stack — the attack surface is
//! enormous, the edge cases are adversarial, and "looks fine to me" is not a
//! standard anything can be judged against. We own protocol decisions; we do
//! not own parsing hostile markup.
//!
//! Defence in depth, because sanitising alone has been defeated before: the UI
//! also renders the result inside a sandboxed iframe with no script permission
//! and its own CSP.

use std::collections::{HashMap, HashSet};

/// Remote images are stripped by default, and that is a privacy decision
/// rather than a security one.
///
/// A `<img src="https://tracker/pixel?id=you">` tells the sender the moment a
/// message was opened, from which IP, on what client. Loading them silently is
/// the single most common way mail clients leak their users, and a
/// privacy-first product cannot do it by default.
pub struct Sanitised {
    pub html: String,
    /// True when at least one remote reference was removed, so the UI can offer
    /// to load them rather than pretending the message looked like this.
    pub blocked_remote: bool,
}

pub fn sanitise(html: &str, allow_remote: bool) -> Sanitised {
    let mut builder = ammonia::Builder::default();

    // No scripts, no styles, no frames, no forms, no objects. A mail message
    // is a document to read, not an application to run.
    let mut tags: HashSet<&str> = [
        "a", "b", "blockquote", "br", "code", "div", "em", "h1", "h2", "h3", "h4", "h5", "h6",
        "hr", "i", "li", "ol", "p", "pre", "span", "strong", "sub", "sup", "table", "tbody",
        "td", "tfoot", "th", "thead", "tr", "u", "ul",
    ]
    .into_iter()
    .collect();
    if allow_remote {
        tags.insert("img");
    }
    builder.tags(tags);

    let mut attrs: HashMap<&str, HashSet<&str>> = HashMap::new();
    attrs.insert("a", ["href", "title"].into_iter().collect());
    if allow_remote {
        attrs.insert("img", ["src", "alt", "title", "width", "height"].into_iter().collect());
    }
    builder.tag_attributes(attrs);

    // Only schemes that cannot execute. javascript: and data: are the classic
    // routes to script execution through a link.
    builder.url_schemes(["http", "https", "mailto"].into_iter().collect());

    // Every link opens in a new context with no reference back to us; without
    // noopener the opened page can navigate this one.
    builder.link_rel(Some("noopener noreferrer nofollow"));

    let cleaned = builder.clean(html).to_string();

    // Whether anything remote was present is judged on the original, since the
    // sanitiser has removed the evidence by the time it returns.
    let blocked_remote = !allow_remote && mentions_remote_content(html);

    Sanitised { html: cleaned, blocked_remote }
}

fn mentions_remote_content(html: &str) -> bool {
    let lower = html.to_ascii_lowercase();
    lower.contains("<img")
        || lower.contains("background:url")
        || lower.contains("background: url")
        || lower.contains("<video")
        || lower.contains("<iframe")
}
