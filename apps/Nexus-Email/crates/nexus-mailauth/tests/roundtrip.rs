//! Sign, then verify. And then break things, because a signature that cannot
//! detect tampering is worse than none — it asserts authenticity it has not
//! checked.

use nexus_mailauth::{
    generate, public_key_record, sign, signed_message, verify_with_key, Canon, DkimError,
    DkimSigner, DEFAULT_SIGNED_HEADERS,
};
use rsa::RsaPrivateKey;
use std::sync::OnceLock;

/// One key for the whole suite: 2048-bit generation is slow enough that doing
/// it per test would dominate the runtime.
fn key() -> &'static RsaPrivateKey {
    static KEY: OnceLock<RsaPrivateKey> = OnceLock::new();
    KEY.get_or_init(|| generate(2048).expect("generate a signing key"))
}

fn signer(hc: Canon, bc: Canon) -> DkimSigner {
    DkimSigner {
        domain: "tnhc.dev".into(),
        selector: "nexus".into(),
        key: key().clone(),
        header_canon: hc,
        body_canon: bc,
    }
}

const MESSAGE: &[u8] = b"From: founder@tnhc.dev\r\n\
To: someone@example.test\r\n\
Subject: A signed message\r\n\
Date: Tue, 18 Aug 2026 12:00:00 +0000\r\n\
Message-ID: <abc@tnhc.dev>\r\n\
\r\n\
This body is covered by the signature.\r\n";

fn sign_it(hc: Canon, bc: Canon) -> (Vec<u8>, String) {
    let cfg = signer(hc, bc);
    let header = sign(&cfg, MESSAGE, DEFAULT_SIGNED_HEADERS).expect("sign");
    let record = public_key_record(key()).expect("public record");
    (signed_message(&header, MESSAGE), record)
}

#[test]
fn a_signed_message_verifies() {
    let (msg, record) = sign_it(Canon::Relaxed, Canon::Relaxed);
    let sig = verify_with_key(&msg, &record).expect("should verify");
    assert_eq!(sig.domain, "tnhc.dev");
    assert_eq!(sig.selector, "nexus");
}

#[test]
fn it_verifies_under_every_canonicalization_pairing() {
    // c= has four combinations and receivers use all of them. A bug in one is
    // invisible if only the common pairing is tested.
    for hc in [Canon::Relaxed, Canon::Simple] {
        for bc in [Canon::Relaxed, Canon::Simple] {
            let (msg, record) = sign_it(hc, bc);
            assert!(
                verify_with_key(&msg, &record).is_ok(),
                "failed for c={}/{}",
                hc.as_str(),
                bc.as_str()
            );
        }
    }
}

#[test]
fn a_tampered_body_is_caught_as_a_body_hash_mismatch() {
    // Reported distinctly from a bad signature: it points at a relay rewriting
    // content rather than at a forgery.
    let (msg, record) = sign_it(Canon::Relaxed, Canon::Relaxed);
    let tampered = String::from_utf8_lossy(&msg)
        .replace("covered by the signature", "quietly rewritten")
        .into_bytes();

    assert_eq!(verify_with_key(&tampered, &record).unwrap_err(), DkimError::BodyHashMismatch);
}

#[test]
fn a_tampered_subject_is_caught() {
    // Subject is signed precisely so a relay cannot rewrite what the message
    // appears to say while keeping the signature intact.
    let (msg, record) = sign_it(Canon::Relaxed, Canon::Relaxed);
    let tampered = String::from_utf8_lossy(&msg)
        .replace("A signed message", "Your account is suspended")
        .into_bytes();

    assert_eq!(verify_with_key(&tampered, &record).unwrap_err(), DkimError::SignatureMismatch);
}

#[test]
fn a_forged_from_is_caught() {
    // The whole question DKIM answers. If From could be swapped without
    // breaking the signature, signing would prove nothing.
    let (msg, record) = sign_it(Canon::Relaxed, Canon::Relaxed);
    let tampered = String::from_utf8_lossy(&msg)
        .replace("From: founder@tnhc.dev", "From: attacker@evil.test")
        .into_bytes();

    assert_eq!(verify_with_key(&tampered, &record).unwrap_err(), DkimError::SignatureMismatch);
}

#[test]
fn another_domains_key_does_not_verify_our_signature() {
    let (msg, _) = sign_it(Canon::Relaxed, Canon::Relaxed);
    let other = generate(2048).unwrap();
    let other_record = public_key_record(&other).unwrap();

    assert_eq!(verify_with_key(&msg, &other_record).unwrap_err(), DkimError::SignatureMismatch);
}

#[test]
fn a_revoked_key_is_refused_rather_than_ignored() {
    // An empty p= is how a key is revoked. Treating it as "no opinion" would
    // let a compromised key keep working.
    let (msg, _) = sign_it(Canon::Relaxed, Canon::Relaxed);
    match verify_with_key(&msg, "v=DKIM1; k=rsa; p=") {
        Err(DkimError::BadPublicKey(_)) => {}
        other => panic!("a revoked key must fail, got {other:?}"),
    }
}

#[test]
fn an_unsigned_message_is_reported_as_unsigned() {
    let record = public_key_record(key()).unwrap();
    assert_eq!(verify_with_key(MESSAGE, &record).unwrap_err(), DkimError::NoSignature);
}

#[test]
fn appending_to_the_body_is_caught() {
    // Trailing-content injection: the original text is untouched, so a naive
    // check that "the message still contains what it did" would pass.
    let (msg, record) = sign_it(Canon::Relaxed, Canon::Relaxed);
    let mut extended = msg.clone();
    extended.extend_from_slice(b"PS: send money to this account instead.\r\n");

    assert_eq!(verify_with_key(&extended, &record).unwrap_err(), DkimError::BodyHashMismatch);
}

#[test]
fn the_published_record_is_a_usable_dkim_txt_record() {
    let record = public_key_record(key()).unwrap();
    assert!(record.starts_with("v=DKIM1; k=rsa; p="));
    // Long enough to be a real 2048-bit key rather than a truncated one.
    assert!(record.len() > 300, "record looked too short: {}", record.len());
}
