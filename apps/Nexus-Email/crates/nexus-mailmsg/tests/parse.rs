//! Parsing tests. Most of these are drawn from how real mail is actually
//! malformed, not from how RFC 5322 says it should look.

use nexus_mailmsg::{parse, Limits, MsgError};

fn p(raw: &str) -> nexus_mailmsg::ParsedMessage {
    parse(raw.as_bytes(), Limits::default()).expect("should parse")
}

#[test]
fn folded_headers_are_unfolded() {
    // A long Subject is split across lines with leading whitespace. Failing to
    // rejoin them truncates the subject at the fold.
    let m = p("Subject: this is a very\r\n long subject\r\n\ttabbed too\r\n\r\nbody");
    assert_eq!(m.headers.get("subject").unwrap(), "this is a very long subject tabbed too");
}

#[test]
fn bare_lf_line_endings_are_accepted() {
    // Extremely common: something in the path rewrote CRLF to LF.
    let m = p("From: a@b.test\nSubject: hi\n\nbody here");
    assert_eq!(m.headers.get("from").unwrap(), "a@b.test");
    assert_eq!(m.body, b"body here");
}

#[test]
fn header_names_are_case_insensitive() {
    let m = p("SUBJECT: shouty\r\n\r\nx");
    assert_eq!(m.headers.get("Subject").unwrap(), "shouty");
    assert_eq!(m.headers.get("subject").unwrap(), "shouty");
}

#[test]
fn repeated_headers_are_all_kept_in_order() {
    // Received: is one per hop and the order IS the delivery path. Collapsing
    // duplicates would destroy the only record of how a message reached us.
    let m = p("Received: hop1\r\nReceived: hop2\r\nReceived: hop3\r\n\r\nx");
    let hops: Vec<_> = m.headers.get_all("received").collect();
    assert_eq!(hops, vec!["hop1", "hop2", "hop3"]);
}

#[test]
fn a_single_broken_header_does_not_lose_the_message() {
    // A line with no colon is not a header. Rejecting the whole message over
    // it would lose mail that every other client displays.
    let m = p("From: a@b.test\r\nthis line has no colon\r\nSubject: fine\r\n\r\nbody");
    assert_eq!(m.headers.get("from").unwrap(), "a@b.test");
    assert_eq!(m.headers.get("subject").unwrap(), "fine");
    assert_eq!(m.body, b"body");
}

#[test]
fn a_message_with_no_body_is_still_a_message() {
    let m = p("Subject: empty\r\n\r\n");
    assert_eq!(m.headers.get("subject").unwrap(), "empty");
    assert!(m.body.is_empty());
}

#[test]
fn missing_separator_is_the_one_unrecoverable_error() {
    let err = parse(b"Subject: no body separator at all", Limits::default()).unwrap_err();
    assert_eq!(err, MsgError::NoSeparator);
}

#[test]
fn the_body_is_returned_byte_for_byte() {
    // DKIM signs the bytes that arrived. Normalising the body here would break
    // every signature we later try to verify.
    let raw = b"Subject: x\r\n\r\nline1\r\nline2\r\n\r\n  trailing  ";
    let m = parse(raw, Limits::default()).unwrap();
    assert_eq!(m.body, b"line1\r\nline2\r\n\r\n  trailing  ");
}

#[test]
fn oversized_input_is_refused_rather_than_buffered() {
    let limits = Limits { max_message: 64, ..Default::default() };
    let big = format!("Subject: x\r\n\r\n{}", "A".repeat(1000));
    assert!(matches!(
        parse(big.as_bytes(), limits),
        Err(MsgError::MessageTooLarge { .. })
    ));
}

#[test]
fn a_header_count_flood_is_refused() {
    // Denial of service by header count is a real technique.
    let limits = Limits { max_header_count: 10, ..Default::default() };
    let mut raw = String::new();
    for i in 0..500 {
        raw.push_str(&format!("X-Flood-{i}: v\r\n"));
    }
    raw.push_str("\r\nbody");
    assert!(parse(raw.as_bytes(), limits).is_err());
}

#[test]
fn invalid_utf8_in_a_header_does_not_panic() {
    // The parser faces bytes chosen by strangers. It must be total.
    let mut raw: Vec<u8> = b"Subject: ".to_vec();
    raw.extend_from_slice(&[0xff, 0xfe, 0x80]);
    raw.extend_from_slice(b"\r\n\r\nbody");
    let m = parse(&raw, Limits::default()).expect("must not fail");
    assert!(m.headers.get("subject").is_some());
}
