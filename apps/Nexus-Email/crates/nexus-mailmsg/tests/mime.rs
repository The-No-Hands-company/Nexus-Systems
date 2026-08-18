//! MIME structure, transfer encodings, charsets and attachments.

use nexus_mailmsg::{parse, tree, ContentType, Limits};

fn t(raw: &str) -> nexus_mailmsg::Part {
    tree(&parse(raw.as_bytes(), Limits::default()).unwrap())
}

#[test]
fn quoted_printable_bodies_are_decoded() {
    let m = t("Content-Transfer-Encoding: quoted-printable\r\n\r\nCaf=C3=A9 =\r\nau lait");
    // The trailing '=' is a soft line break: the line continues, no newline.
    assert_eq!(m.text(), "Café au lait");
}

#[test]
fn base64_bodies_are_decoded_across_wrapped_lines() {
    let m = t("Content-Transfer-Encoding: base64\r\n\r\naGVsbG8g\r\nd29ybGQ=");
    assert_eq!(m.text(), "hello world");
}

#[test]
fn a_legacy_charset_is_honoured() {
    // Mail predates UTF-8 and still carries ISO-8859-1. Assuming UTF-8 turns
    // ordinary European mail into replacement characters.
    let raw: Vec<u8> = [
        b"Content-Type: text/plain; charset=iso-8859-1\r\n\r\n".to_vec(),
        vec![0x48, 0xE9, 0x6C, 0x6C, 0x6F], // "Héllo" in latin-1
    ]
    .concat();
    let part = tree(&parse(&raw, Limits::default()).unwrap());
    assert_eq!(part.text(), "Héllo");
}

#[test]
fn multipart_alternative_yields_both_parts() {
    let raw = "Content-Type: multipart/alternative; boundary=\"bnd\"\r\n\
\r\n\
--bnd\r\n\
Content-Type: text/plain\r\n\
\r\n\
plain version\r\n\
--bnd\r\n\
Content-Type: text/html\r\n\
\r\n\
<p>html version</p>\r\n\
--bnd--\r\n";
    let m = t(raw);
    assert!(m.content_type.is_multipart());
    assert_eq!(m.children.len(), 2);
    assert!(m.children[0].text().contains("plain version"));
    assert!(m.children[1].text().contains("html version"));
}

#[test]
fn an_attachment_is_recognised_and_decoded() {
    let raw = "Content-Type: multipart/mixed; boundary=\"b\"\r\n\
\r\n\
--b\r\n\
Content-Type: text/plain\r\n\
\r\n\
see attached\r\n\
--b\r\n\
Content-Type: application/pdf; name=\"report.pdf\"\r\n\
Content-Transfer-Encoding: base64\r\n\
Content-Disposition: attachment; filename=\"report.pdf\"\r\n\
\r\n\
aGVsbG8=\r\n\
--b--\r\n";
    let m = t(raw);
    let attachments: Vec<_> = m.walk().into_iter().filter(|p| p.is_attachment()).collect();
    assert_eq!(attachments.len(), 1);
    assert_eq!(attachments[0].filename().unwrap(), "report.pdf");
    assert_eq!(attachments[0].body, b"hello");
}

#[test]
fn the_epilogue_after_the_closing_boundary_is_not_a_part() {
    let raw = "Content-Type: multipart/mixed; boundary=\"b\"\r\n\
\r\n\
preamble text ignored\r\n\
--b\r\n\
Content-Type: text/plain\r\n\
\r\n\
real part\r\n\
--b--\r\n\
epilogue junk that is not a part\r\n";
    let m = t(raw);
    assert_eq!(m.children.len(), 1, "preamble and epilogue must not become parts");
    assert!(m.children[0].text().contains("real part"));
}

#[test]
fn deeply_nested_multiparts_terminate() {
    // Unbounded recursion on attacker-supplied nesting is a stack overflow.
    let mut raw = String::from("Content-Type: multipart/mixed; boundary=\"b0\"\r\n\r\n");
    for i in 0..200 {
        raw.push_str(&format!(
            "--b0\r\nContent-Type: multipart/mixed; boundary=\"b{i}\"\r\n\r\n"
        ));
    }
    raw.push_str("--b0--\r\n");
    let m = t(&raw); // must return, not overflow
    assert!(m.content_type.is_multipart());
}

#[test]
fn a_multipart_with_no_boundary_is_treated_as_content() {
    // Broken sender. Losing the body would be worse than showing it raw.
    let m = t("Content-Type: multipart/mixed\r\n\r\nsome content");
    assert!(m.children.is_empty());
    assert_eq!(m.body, b"some content");
}

#[test]
fn content_type_parameters_are_parsed_and_case_folded() {
    let ct = ContentType::parse("TEXT/Plain; CharSet=\"UTF-8\"; format=flowed");
    assert_eq!(ct.mime_type, "text/plain");
    assert_eq!(ct.param("charset").unwrap(), "UTF-8");
    assert_eq!(ct.param("FORMAT").unwrap(), "flowed");
}
