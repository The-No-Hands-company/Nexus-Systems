//! Canonicalization, checked against RFC 6376's own example vectors (§3.4.5)
//! rather than against our expectations. A signature that verifies only in our
//! implementation means every message we send is treated as forged.

use nexus_mailauth::canon::{body, header, Canon};

/// The RFC's example message, with the exact whitespace it specifies:
///
/// ```text
/// A: X
/// B : Y<TAB>
/// <TAB>Z<SP><SP>
///
/// <SP>C<SP>
/// D<SP><TAB><SP>E<CRLF>
/// <CRLF>
/// <CRLF>
/// ```
const EXAMPLE_BODY: &[u8] = b" C \r\nD \t E\r\n\r\n\r\n";

#[test]
fn relaxed_body_matches_the_rfc_vector() {
    // RFC 6376 §3.4.5: " C\r\nD E\r\n"
    assert_eq!(body(Canon::Relaxed, EXAMPLE_BODY), b" C\r\nD E\r\n");
}

#[test]
fn simple_body_matches_the_rfc_vector() {
    // RFC 6376 §3.4.5: " C \r\nD \t E\r\n" — trailing empty lines removed,
    // everything else untouched.
    assert_eq!(body(Canon::Simple, EXAMPLE_BODY), b" C \r\nD \t E\r\n");
}

#[test]
fn relaxed_headers_match_the_rfc_vector() {
    // "A: X" -> "a:X" and "B : Y\t\r\n\tZ  " (unfolded) -> "b:Y Z"
    assert_eq!(header(Canon::Relaxed, "A", " X"), "a:X\r\n");
    assert_eq!(header(Canon::Relaxed, "B ", " Y\t Z  "), "b:Y Z\r\n");
}

#[test]
fn simple_headers_are_left_exactly_alone() {
    // Including the original case and spacing, which is the entire point.
    assert_eq!(header(Canon::Simple, "A", " X"), "A: X\r\n");
    assert_eq!(header(Canon::Simple, "SuBjEcT", "  Hello  "), "SuBjEcT:  Hello  \r\n");
}

#[test]
fn an_empty_body_canonicalizes_differently_under_each_algorithm() {
    // A difference that trips people up: relaxed yields nothing at all, simple
    // yields a single CRLF. Getting it wrong breaks every signature over an
    // empty body.
    assert_eq!(body(Canon::Relaxed, b""), b"");
    assert_eq!(body(Canon::Simple, b""), b"\r\n");
    assert_eq!(body(Canon::Relaxed, b"\r\n\r\n\r\n"), b"");
    assert_eq!(body(Canon::Simple, b"\r\n\r\n\r\n"), b"\r\n");
}

#[test]
fn trailing_empty_lines_are_removed_but_interior_ones_survive() {
    // Blank lines inside a body are content; blank lines at the end are not.
    let with_gap = b"one\r\n\r\ntwo\r\n\r\n\r\n";
    assert_eq!(body(Canon::Relaxed, with_gap), b"one\r\n\r\ntwo\r\n");
}

#[test]
fn a_body_with_no_final_newline_still_ends_with_one() {
    // Both algorithms guarantee the canonical body ends in CRLF, so a message
    // truncated mid-line still hashes predictably.
    assert_eq!(body(Canon::Relaxed, b"no newline"), b"no newline\r\n");
    assert_eq!(body(Canon::Simple, b"no newline"), b"no newline\r\n");
}

#[test]
fn bare_lf_line_endings_are_treated_as_line_endings() {
    // Real mail arrives with LF endings after passing through something
    // careless. Treating them as body bytes would change the hash.
    assert_eq!(body(Canon::Relaxed, b"one\ntwo\n"), b"one\r\ntwo\r\n");
}

#[test]
fn relaxed_collapses_tabs_and_runs_of_spaces_alike() {
    assert_eq!(header(Canon::Relaxed, "X", "a  \t  b"), "x:a b\r\n");
    assert_eq!(body(Canon::Relaxed, b"a  \t  b\r\n"), b"a b\r\n");
}
