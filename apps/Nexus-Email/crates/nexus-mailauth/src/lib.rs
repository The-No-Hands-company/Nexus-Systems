//! DKIM: signing what we send, verifying what we receive.
//!
//! Canonicalization is tested against RFC 6376's own vectors rather than our
//! expectations, because the failure mode is asymmetric — a signature that
//! verifies only in this implementation means every message we send is treated
//! as forged, and we would not find out from our own tests.
//!
//! Verification is split from the DNS lookup on purpose: the cryptographic
//! half is pure and testable without a resolver, so a logic bug cannot hide
//! behind a network call.

pub mod canon;
pub mod error;
pub mod key;
pub mod sign;
pub mod verify;

pub use canon::Canon;
pub use error::{DkimError, Result};
pub use key::{generate, private_key_pem, public_key_record};
pub use sign::{sign, signed_message, Signer_ as DkimSigner, DEFAULT_SIGNED_HEADERS};
pub use verify::{parse_signature, public_key_from_record, verify_with_key, key_record_name};
