//! Generation, and round trips through the parser. If these two halves
//! disagree, mail we send is mail we cannot read back.

use nexus_mailmsg::encoding::decode_encoded_words;
use nexus_mailmsg::{parse, tree, Attachment, Limits, MessageBuilder};

fn built(b: MessageBuilder) -> nexus_mailmsg::Part {
    let raw = b.build();
    let text = String::from_utf8_lossy(&raw).into_owned();
    assert!(
        !text.contains('\n') || text.contains("\r\n"),
        "generated mail must use CRLF"
    );
    // Every bare LF must be part of a CRLF: SMTP requires it, and a message
    // assembled with bare LF gets rewritten in transit, which breaks any DKIM
    // signature computed over the original bytes.
    for (i, w) in raw.windows(2).enumerate() {
        if w[1] == b'\n' {
            assert_eq!(w[0], b'\r', "bare LF at offset {}", i + 1);
        }
    }
    tree(&parse(&raw, Limits::default()).unwrap())
}

#[test]
fn a_plain_message_round_trips() {
    let part = built(
        MessageBuilder::new("alice@tnhc.dev", "<id-1@tnhc.dev>")
            .to("bob@example.test")
            .subject("Hello there")
            .text("This is the body."),
    );
    assert_eq!(part.headers.get("from").unwrap(), "alice@tnhc.dev");
    assert_eq!(part.headers.get("to").unwrap(), "bob@example.test");
    assert_eq!(part.headers.get("subject").unwrap(), "Hello there");
    assert_eq!(part.text().trim(), "This is the body.");
}

#[test]
fn a_non_ascii_subject_survives_the_round_trip() {
    // Headers are an ASCII protocol; a raw UTF-8 subject is mangled by strict
    // receivers, so it goes out as an RFC 2047 encoded word and must come back
    // identical.
    let subject = "Møte på fredag — café ☕";
    let part = built(
        MessageBuilder::new("a@tnhc.dev", "<id-2@tnhc.dev>")
            .subject(subject)
            .text("x"),
    );
    let raw_header = part.headers.get("subject").unwrap();
    assert!(raw_header.starts_with("=?utf-8?B?"), "should be an encoded word");
    assert_eq!(decode_encoded_words(raw_header), subject);
}

#[test]
fn an_ascii_subject_is_left_alone() {
    // Encoding an ASCII subject would be needless and makes headers unreadable
    // to humans debugging with a text editor.
    let part = built(
        MessageBuilder::new("a@tnhc.dev", "<id-3@tnhc.dev>")
            .subject("Plain ASCII subject")
            .text("x"),
    );
    assert_eq!(part.headers.get("subject").unwrap(), "Plain ASCII subject");
}

#[test]
fn text_and_html_become_a_readable_alternative() {
    let part = built(
        MessageBuilder::new("a@tnhc.dev", "<id-4@tnhc.dev>")
            .subject("both")
            .text("plain body")
            .html("<p>html body</p>"),
    );
    assert!(part.content_type.mime_type.starts_with("multipart/alternative"));
    let texts: Vec<String> = part.children.iter().map(|c| c.text()).collect();
    assert!(texts.iter().any(|t| t.contains("plain body")));
    assert!(texts.iter().any(|t| t.contains("html body")));
}

#[test]
fn an_attachment_survives_the_round_trip_byte_for_byte() {
    // Arbitrary bytes, including NUL and high bytes, must come back unchanged.
    let data: Vec<u8> = (0u8..=255).cycle().take(5000).collect();
    let part = built(
        MessageBuilder::new("a@tnhc.dev", "<id-5@tnhc.dev>")
            .subject("with attachment")
            .text("see attached")
            .attach(Attachment {
                filename: "data.bin".into(),
                mime_type: "application/octet-stream".into(),
                data: data.clone(),
            }),
    );

    let attachments: Vec<_> = part.walk().into_iter().filter(|p| p.is_attachment()).collect();
    assert_eq!(attachments.len(), 1);
    assert_eq!(attachments[0].filename().unwrap(), "data.bin");
    assert_eq!(attachments[0].body, data, "attachment bytes must be identical");
}

#[test]
fn threading_headers_are_emitted_for_replies() {
    // These are what let the store put a reply in its parent's thread.
    let part = built(
        MessageBuilder::new("a@tnhc.dev", "<child@tnhc.dev>")
            .subject("Re: design")
            .in_reply_to("<parent@tnhc.dev>")
            .references(vec!["<root@tnhc.dev>".into(), "<parent@tnhc.dev>".into()])
            .text("agreed"),
    );
    assert_eq!(part.headers.get("in-reply-to").unwrap(), "<parent@tnhc.dev>");
    assert_eq!(
        part.headers.get("references").unwrap(),
        "<root@tnhc.dev> <parent@tnhc.dev>"
    );
}

#[test]
fn the_date_is_in_rfc_5322_format_not_iso_8601() {
    // Mail has its own, older date format. Strict clients reject ISO 8601.
    let part = built(MessageBuilder::new("a@tnhc.dev", "<id-6@tnhc.dev>").text("x"));
    let date = part.headers.get("date").unwrap();
    assert!(
        chrono::DateTime::parse_from_rfc2822(date).is_ok(),
        "Date must parse as RFC 2822/5322, got {date:?}"
    );
}

#[test]
fn a_body_with_long_lines_and_equals_signs_round_trips() {
    // Quoted-printable has to wrap at 76 columns and escape '='. Getting the
    // soft-break wrong corrupts the body silently.
    let body = format!("{}={}", "x".repeat(200), "y".repeat(200));
    let part = built(
        MessageBuilder::new("a@tnhc.dev", "<id-7@tnhc.dev>")
            .subject("long")
            .text(&body),
    );
    assert_eq!(part.text().trim_end(), body);
}
