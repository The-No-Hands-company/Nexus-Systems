//! The IMAP conversation. Access control gets the most attention: a server
//! that answers before authentication discloses which mailboxes and messages
//! exist to anyone who connects.

use nexus_mailimap::{expand_set, Action, Request, Session};

fn tagged(s: &mut Session, line: &str) -> Action {
    s.line(line)
}

fn text(a: &Action) -> String {
    match a {
        Action::Reply(r) | Action::ReplyAndClose(r) => r.clone(),
        Action::Need(_) => String::new(),
    }
}

fn authed() -> Session {
    let mut s = Session::new("mail.tnhc.dev");
    s.mark_authenticated();
    s
}

fn selected() -> Session {
    let mut s = authed();
    s.mark_selected(vec![1, 2, 3, 7, 9]);
    s
}

// ── Access control ──────────────────────────────────────────────────────────

#[test]
fn nothing_useful_is_answered_before_login() {
    // Each of these would disclose something to an anonymous client.
    let mut s = Session::new("mail.tnhc.dev");
    for line in [
        "a1 LIST \"\" *",
        "a2 SELECT INBOX",
        "a3 UID FETCH 1:* (FLAGS)",
        "a4 UID SEARCH ALL",
    ] {
        let r = text(&tagged(&mut s, line));
        assert!(r.contains("NO Authenticate first"), "{line} was answered: {r}");
    }
}

#[test]
fn capability_and_logout_work_before_login() {
    // A client has to be able to discover what the server supports, and must
    // always be able to hang up cleanly.
    let mut s = Session::new("mail.tnhc.dev");
    assert!(text(&tagged(&mut s, "a1 CAPABILITY")).contains("IMAP4rev1"));
    assert!(matches!(tagged(&mut s, "a2 LOGOUT"), Action::ReplyAndClose(_)));
}

#[test]
fn login_is_handed_to_the_server_rather_than_decided_here() {
    // The state machine deliberately cannot authenticate anyone: it holds no
    // credentials and no database handle.
    let mut s = Session::new("mail.tnhc.dev");
    match tagged(&mut s, "a1 LOGIN founder secret") {
        Action::Need(Request::Authenticate { user, password, .. }) => {
            assert_eq!(user, "founder");
            assert_eq!(password, "secret");
        }
        other => panic!("expected an auth request, got {other:?}"),
    }
}

#[test]
fn a_quoted_password_containing_spaces_survives_parsing() {
    // Passphrases have spaces; a naive split would truncate one silently and
    // the user would just see "login failed".
    let mut s = Session::new("mail.tnhc.dev");
    match tagged(&mut s, "a1 LOGIN \"founder\" \"a long pass phrase\"") {
        Action::Need(Request::Authenticate { user, password, .. }) => {
            assert_eq!(user, "founder");
            assert_eq!(password, "a long pass phrase");
        }
        other => panic!("got {other:?}"),
    }
}

#[test]
fn fetching_before_selecting_a_mailbox_is_refused() {
    let mut s = authed();
    assert!(text(&tagged(&mut s, "a1 UID FETCH 1:* (FLAGS)")).contains("BAD"));
}

// ── UID sets ────────────────────────────────────────────────────────────────

#[test]
fn uid_ranges_expand_against_what_exists() {
    let known = [1i64, 2, 3, 7, 9];
    assert_eq!(expand_set("1:3", &known), vec![1, 2, 3]);
    assert_eq!(expand_set("7", &known), vec![7]);
    assert_eq!(expand_set("1,9", &known), vec![1, 9]);
    // A range naming UIDs that do not exist yields only those that do.
    assert_eq!(expand_set("4:8", &known), vec![7]);
}

#[test]
fn a_star_means_the_highest_uid_that_exists() {
    // "1:*" is how every client asks for everything, and "N:*" is how it asks
    // for new mail. Treating * as unbounded would return nothing.
    let known = [1i64, 2, 3, 7, 9];
    assert_eq!(expand_set("1:*", &known), known.to_vec());
    assert_eq!(expand_set("8:*", &known), vec![9]);
    // RFC 3501: a range whose low end is past the highest UID still returns
    // the highest one, which is what "fetch anything new" depends on.
    assert_eq!(expand_set("100:*", &known), vec![9]);
}

#[test]
fn a_uid_set_against_an_empty_mailbox_is_empty_not_an_error() {
    assert!(expand_set("1:*", &[]).is_empty());
    assert!(expand_set("5", &[]).is_empty());
}

#[test]
fn a_reversed_range_is_read_the_way_round_it_was_meant() {
    // RFC 3501 says 5:1 and 1:5 are the same set. Clients do emit the former.
    let known = [1i64, 2, 3, 7, 9];
    assert_eq!(expand_set("3:1", &known), vec![1, 2, 3]);
}

// ── Selected state ──────────────────────────────────────────────────────────

#[test]
fn fetch_asks_the_server_for_exactly_the_uids_that_exist() {
    let mut s = selected();
    match tagged(&mut s, "a1 UID FETCH 1:* (FLAGS BODY[])") {
        Action::Need(Request::Fetch { uids, items, .. }) => {
            assert_eq!(uids, vec![1, 2, 3, 7, 9]);
            assert!(items.contains("FLAGS"));
        }
        other => panic!("got {other:?}"),
    }
}

#[test]
fn store_carries_the_action_and_flags_through() {
    // Marking read from a mail client is the most common write there is.
    let mut s = selected();
    match tagged(&mut s, "a1 UID STORE 7 +FLAGS (\\Seen)") {
        Action::Need(Request::Store { uids, action, flags, .. }) => {
            assert_eq!(uids, vec![7]);
            assert_eq!(action, "+FLAGS");
            assert!(flags.contains("\\Seen"));
        }
        other => panic!("got {other:?}"),
    }
}

#[test]
fn close_returns_to_the_authenticated_state() {
    let mut s = selected();
    assert!(text(&tagged(&mut s, "a1 CLOSE")).contains("OK"));
    // And a fetch is refused again, because no mailbox is open.
    assert!(text(&tagged(&mut s, "a2 UID FETCH 1 (FLAGS)")).contains("BAD"));
}

#[test]
fn every_response_echoes_the_clients_own_tag() {
    // A client that sees the wrong tag treats the connection as broken.
    let mut s = selected();
    for tag in ["a1", "xyz", "A0042"] {
        let r = text(&tagged(&mut s, &format!("{tag} NOOP")));
        assert!(r.starts_with(tag), "expected {tag} in {r}");
    }
}

#[test]
fn an_unknown_command_is_bad_not_ignored() {
    let mut s = selected();
    assert!(text(&tagged(&mut s, "a1 FROBNICATE now")).contains("BAD"));
}

// ── What a FETCH actually asks for ──────────────────────────────────────────

/// Mirrors the server's decision about whether a FETCH wants message bodies.
/// Pinned here because the loose version of this check made every header-only
/// listing download every message in full — the exact request a client sends
/// to avoid doing that, turned into the worst case.
fn wants_body(items: &str) -> bool {
    let upper = items.to_uppercase();
    upper.contains("BODY[")
        || upper.contains("BODY.PEEK[")
        || upper.contains("RFC822.TEXT")
        || upper
            .split(|c: char| !c.is_ascii_alphanumeric() && c != '.')
            .any(|item| item == "RFC822")
}

#[test]
fn a_header_only_fetch_does_not_pull_message_bodies() {
    // "RFC822.SIZE" contains "RFC822" and "BODYSTRUCTURE" contains "BODY";
    // matching loosely is how a message list becomes a full download.
    assert!(!wants_body("(FLAGS RFC822.SIZE)"));
    assert!(!wants_body("(UID FLAGS INTERNALDATE RFC822.SIZE)"));
    assert!(!wants_body("(BODYSTRUCTURE)"));
    assert!(!wants_body("(ENVELOPE FLAGS)"));
}

#[test]
fn a_fetch_that_really_wants_the_body_gets_one() {
    assert!(wants_body("(BODY[])"));
    assert!(wants_body("(BODY.PEEK[])"));
    assert!(wants_body("(RFC822)"));
    assert!(wants_body("(RFC822.TEXT)"));
    assert!(wants_body("(FLAGS BODY[])"));
}
