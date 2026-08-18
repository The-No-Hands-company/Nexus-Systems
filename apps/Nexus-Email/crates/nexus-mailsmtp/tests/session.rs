//! The SMTP conversation, exhaustively. No sockets: the session is a pure
//! state machine, which is exactly why these rules can be pinned down.

use nexus_mailsmtp::{Action, Limits, RelayPolicy, Role, Session};

/// Hosts one domain, like a real node would.
struct Local;
impl RelayPolicy for Local {
    fn is_local(&self, address: &str) -> bool {
        address.to_ascii_lowercase().ends_with("@tnhc.dev")
    }
}

fn mx() -> Session<Local> {
    Session::new("mail.tnhc.dev", Role::Mx, Local, Limits::default())
}

fn submission(authenticated: bool) -> Session<Local> {
    let mut s = Session::new("mail.tnhc.dev", Role::Submission, Local, Limits::default());
    s.set_authenticated(authenticated);
    s
}

fn say(s: &mut Session<Local>, line: &str) -> Action {
    s.line(line.as_bytes())
}

fn reply(a: &Action) -> String {
    match a {
        Action::Reply(r) | Action::ReplyAndClose(r) => r.clone(),
        Action::Deliver { reply, .. } => reply.clone(),
        Action::Continue => String::new(),
    }
}

/// Drive a session to the point of accepting message data.
fn open_transaction(s: &mut Session<Local>, from: &str, to: &str) -> Action {
    say(s, "EHLO client.test");
    say(s, &format!("MAIL FROM:<{from}>"));
    let r = say(s, &format!("RCPT TO:<{to}>"));
    r
}

// ── The open-relay guard ────────────────────────────────────────────────────

#[test]
fn an_anonymous_sender_cannot_relay_to_a_third_party() {
    // THE test. An open relay is found by the internet within hours and the
    // reputational damage is not recoverable.
    let mut s = mx();
    let r = open_transaction(&mut s, "spammer@evil.test", "victim@gmail.com");
    assert!(reply(&r).starts_with("550"), "must refuse to relay, got {}", reply(&r));
}

#[test]
fn an_anonymous_sender_may_deliver_to_a_mailbox_we_host() {
    // The other half: refusing everything would make the MX useless.
    let mut s = mx();
    let r = open_transaction(&mut s, "someone@example.test", "info@tnhc.dev");
    assert!(reply(&r).starts_with("250"), "should accept local mail, got {}", reply(&r));
}

#[test]
fn an_unauthenticated_submission_session_cannot_send_anywhere() {
    // Submission exists to send outward, so it is gated on authentication
    // rather than on the recipient — including to local addresses.
    let mut s = submission(false);
    for target in ["victim@gmail.com", "info@tnhc.dev"] {
        let mut s2 = submission(false);
        let r = open_transaction(&mut s2, "a@tnhc.dev", target);
        assert!(reply(&r).starts_with("550"), "unauthenticated must be refused for {target}");
    }
    let _ = &mut s;
}

#[test]
fn an_authenticated_submission_session_may_send_outward() {
    let mut s = submission(true);
    let r = open_transaction(&mut s, "founder@tnhc.dev", "someone@gmail.com");
    assert!(reply(&r).starts_with("250"), "authenticated should be allowed, got {}", reply(&r));
}

#[test]
fn a_session_does_not_start_out_authenticated() {
    // A bug that flips this default is an open relay, so it is asserted rather
    // than assumed.
    let mut s = Session::new("mail.tnhc.dev", Role::Submission, Local, Limits::default());
    let r = open_transaction(&mut s, "a@tnhc.dev", "b@tnhc.dev");
    assert!(reply(&r).starts_with("550"));
}

#[test]
fn auth_is_not_offered_on_the_mx_port() {
    // Accepting AUTH on port 25 invites credential stuffing against a service
    // with no reason to accept it.
    let mut s = mx();
    say(&mut s, "EHLO client.test");
    let r = say(&mut s, "AUTH LOGIN");
    assert!(reply(&r).starts_with("503"), "got {}", reply(&r));
}

// ── Sequence rules ──────────────────────────────────────────────────────────

#[test]
fn commands_out_of_order_are_refused() {
    let mut s = mx();
    assert!(reply(&say(&mut s, "MAIL FROM:<a@b.test>")).starts_with("503"));

    let mut s = mx();
    say(&mut s, "EHLO x");
    assert!(reply(&say(&mut s, "RCPT TO:<info@tnhc.dev>")).starts_with("503"));

    let mut s = mx();
    say(&mut s, "EHLO x");
    say(&mut s, "MAIL FROM:<a@b.test>");
    assert!(reply(&say(&mut s, "DATA")).starts_with("503"));
}

#[test]
fn rset_abandons_the_transaction() {
    let mut s = mx();
    open_transaction(&mut s, "a@b.test", "info@tnhc.dev");
    assert!(reply(&say(&mut s, "RSET")).starts_with("250"));
    // The recipient is gone, so DATA has nothing to attach to.
    assert!(reply(&say(&mut s, "DATA")).starts_with("503"));
}

#[test]
fn quit_closes_the_connection() {
    let mut s = mx();
    let r = say(&mut s, "QUIT");
    assert!(matches!(r, Action::ReplyAndClose(_)));
    assert!(s.is_closed());
}

// ── Hostile input ───────────────────────────────────────────────────────────

#[test]
fn a_flood_of_bad_commands_ends_the_connection() {
    // Otherwise a stranger holds a connection open indefinitely feeding junk.
    let mut s = Session::new(
        "mail.tnhc.dev", Role::Mx, Local,
        Limits { max_errors: 3, ..Default::default() },
    );
    say(&mut s, "NONSENSE");
    say(&mut s, "NONSENSE");
    let r = say(&mut s, "NONSENSE");
    assert!(matches!(r, Action::ReplyAndClose(_)));
    assert!(reply(&r).starts_with("421"));
}

#[test]
fn an_overlong_command_line_is_refused_not_buffered() {
    let mut s = mx();
    let r = s.line(&vec![b'A'; 5000]);
    assert!(reply(&r).starts_with("500"));
}

#[test]
fn invalid_utf8_in_a_command_does_not_panic() {
    let mut s = mx();
    let r = s.line(&[0xff, 0xfe, 0x00, 0x80]);
    assert!(reply(&r).starts_with("500"));
}

#[test]
fn vrfy_never_reveals_whether_an_address_exists() {
    // Answering truthfully hands a stranger a list of who lives here.
    let mut s = mx();
    say(&mut s, "EHLO x");
    let real = reply(&say(&mut s, "VRFY info@tnhc.dev"));
    let fake = reply(&say(&mut s, "VRFY nobody@tnhc.dev"));
    assert_eq!(real, fake, "the answer must not depend on whether the address exists");
    assert!(real.starts_with("252"));
}

#[test]
fn too_many_recipients_is_refused() {
    let mut s = Session::new(
        "mail.tnhc.dev", Role::Mx, Local,
        Limits { max_recipients: 2, ..Default::default() },
    );
    say(&mut s, "EHLO x");
    say(&mut s, "MAIL FROM:<a@b.test>");
    say(&mut s, "RCPT TO:<one@tnhc.dev>");
    say(&mut s, "RCPT TO:<two@tnhc.dev>");
    assert!(reply(&say(&mut s, "RCPT TO:<three@tnhc.dev>")).starts_with("452"));
}

// ── DATA ────────────────────────────────────────────────────────────────────

#[test]
fn a_complete_message_is_handed_over_for_delivery() {
    let mut s = mx();
    open_transaction(&mut s, "sender@example.test", "info@tnhc.dev");
    say(&mut s, "DATA");
    say(&mut s, "Subject: hello");
    say(&mut s, "");
    say(&mut s, "body line");
    let r = say(&mut s, ".");

    match r {
        Action::Deliver { from, recipients, data, reply } => {
            assert_eq!(from, "sender@example.test");
            assert_eq!(recipients, vec!["info@tnhc.dev"]);
            assert!(String::from_utf8_lossy(&data).contains("body line"));
            assert!(reply.starts_with("250"));
        }
        other => panic!("expected delivery, got {other:?}"),
    }
}

#[test]
fn dot_stuffing_is_undone() {
    // A body line of "..hidden" is really ".hidden". Getting this wrong
    // corrupts any message containing a line that starts with a dot.
    let mut s = mx();
    open_transaction(&mut s, "a@b.test", "info@tnhc.dev");
    say(&mut s, "DATA");
    say(&mut s, "..hidden");
    let r = say(&mut s, ".");
    match r {
        Action::Deliver { data, .. } => {
            assert_eq!(String::from_utf8_lossy(&data), ".hidden\r\n");
        }
        other => panic!("expected delivery, got {other:?}"),
    }
}

#[test]
fn an_oversized_message_is_refused_but_the_session_survives() {
    // Dropping the connection instead would leave the sender retrying forever
    // without ever learning why.
    let mut s = Session::new(
        "mail.tnhc.dev", Role::Mx, Local,
        Limits { max_message_bytes: 64, ..Default::default() },
    );
    open_transaction(&mut s, "a@b.test", "info@tnhc.dev");
    say(&mut s, "DATA");
    for _ in 0..50 {
        say(&mut s, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    }
    let r = say(&mut s, ".");

    assert!(matches!(r, Action::Reply(_)), "must not deliver an oversized message");
    assert!(reply(&r).starts_with("552"));
    assert!(!s.is_closed(), "the session should continue");

    // And the next transaction still works.
    let r2 = open_transaction(&mut s, "a@b.test", "info@tnhc.dev");
    assert!(reply(&r2).starts_with("250"));
}

#[test]
fn the_advertised_size_matches_the_enforced_one() {
    // A sender that trusts the advertised SIZE and is then refused has been
    // lied to, and will keep retrying a message that can never fit.
    let mut s = Session::new(
        "mail.tnhc.dev", Role::Mx, Local,
        Limits { max_message_bytes: 12345, ..Default::default() },
    );
    let banner = reply(&say(&mut s, "EHLO x"));
    assert!(banner.contains("SIZE 12345"), "got {banner}");
}
