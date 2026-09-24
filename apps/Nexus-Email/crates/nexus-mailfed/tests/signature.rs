use nexus_mailfed::{sign, verify, NodeKey, SignedRequest};

fn request() -> SignedRequest {
    SignedRequest {
        method: "POST".into(),
        path: "/federation/v1/mail".into(),
        host: "mail.peer.example".into(),
        timestamp: 1_790_000_000,
        nonce: "test-nonce-one".into(),
        envelope_from: "info@tnhc.dev".into(),
        recipients: vec!["someone@example.com".into(), "other@example.com".into()],
        body_sha256: SignedRequest::body_digest(b"Subject: hi\r\n\r\nhello\r\n"),
    }
}

#[test]
fn a_signed_request_verifies_with_the_signers_public_key() {
    let key = NodeKey::generate();
    let req = request();
    let sig = sign(&key, &req);
    verify(&key.public_b64(), &req, &sig).expect("genuine signature must verify");
}

#[test]
fn every_signed_field_is_covered() {
    let key = NodeKey::generate();
    let sig = sign(&key, &request());

    // Each mutation alone must break verification. If one of these passes,
    // an attacker could change that field on a captured request for free.
    let mutations: Vec<(&str, Box<dyn Fn(&mut SignedRequest)>)> = vec![
        ("method", Box::new(|r| r.method = "PUT".into())),
        ("path", Box::new(|r| r.path = "/federation/v1/other".into())),
        ("host", Box::new(|r| r.host = "mail.attacker.example".into())),
        ("timestamp", Box::new(|r| r.timestamp += 1)),
        ("nonce", Box::new(|r| r.nonce = "test-nonce-two".into())),
        ("envelope_from", Box::new(|r| r.envelope_from = "ceo@tnhc.dev".into())),
        ("recipients", Box::new(|r| r.recipients.push("extra@example.com".into()))),
        ("body", Box::new(|r| r.body_sha256 = SignedRequest::body_digest(b"tampered"))),
    ];

    for (field, mutate) in mutations {
        let mut req = request();
        mutate(&mut req);
        assert!(
            verify(&key.public_b64(), &req, &sig).is_err(),
            "changing {field} did not invalidate the signature"
        );
    }
}

#[test]
fn recipient_order_and_boundaries_are_signed() {
    // "a,b" + "c" must not collide with "a" + "b,c": the list is encoded, not
    // just concatenated.
    let key = NodeKey::generate();
    let mut req = request();
    req.recipients = vec!["a@x.example".into(), "b@x.example".into()];
    let sig = sign(&key, &req);
    req.recipients = vec!["b@x.example".into(), "a@x.example".into()];
    assert!(verify(&key.public_b64(), &req, &sig).is_err());
}

#[test]
fn a_different_key_does_not_verify() {
    let signer = NodeKey::generate();
    let other = NodeKey::generate();
    let req = request();
    let sig = sign(&signer, &req);
    assert!(verify(&other.public_b64(), &req, &sig).is_err());
}

#[test]
fn malformed_keys_and_signatures_are_errors_not_panics() {
    let key = NodeKey::generate();
    let req = request();
    let sig = sign(&key, &req);
    assert!(verify("not base64!!", &req, &sig).is_err());
    assert!(verify("AAAA", &req, &sig).is_err());
    assert!(verify(&key.public_b64(), &req, "not base64!!").is_err());
    assert!(verify(&key.public_b64(), &req, "AAAA").is_err());
}

#[test]
fn load_or_create_is_stable_across_calls() {
    let dir = std::env::temp_dir().join(format!("nexus-mailfed-key-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    let path = dir.join("node.key");

    let first = NodeKey::load_or_create(&path).expect("create");
    let second = NodeKey::load_or_create(&path).expect("load");
    assert_eq!(first.public_b64(), second.public_b64());

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn a_corrupt_key_file_is_refused_rather_than_replaced() {
    // Silently generating a new key over a damaged one would change this
    // node's identity and break every peer that pinned it.
    let dir = std::env::temp_dir().join(format!("nexus-mailfed-bad-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("node.key");
    std::fs::write(&path, b"short").unwrap();

    assert!(NodeKey::load_or_create(&path).is_err());
    assert_eq!(std::fs::read(&path).unwrap(), b"short");

    std::fs::remove_dir_all(&dir).unwrap();
}
