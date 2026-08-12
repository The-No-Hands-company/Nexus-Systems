use std::collections::HashMap;
use std::sync::Mutex;
#[cfg(target_arch = "wasm32")]
use wasm_bindgen::prelude::*;
use pqcrypto_traits::kem::{Ciphertext as _, PublicKey as KemPk, SecretKey as KemSk, SharedSecret as _};
use pqcrypto_traits::sign::{PublicKey as SignPk, SecretKey as SignSk, DetachedSignature};

// ── Core identity store (pure Rust, testable) ─────────────────────

pub struct Identity {
    pub did: String,
    kem_public: Vec<u8>,
    kem_secret: Vec<u8>,
    signing_public: Vec<u8>,
    signing_secret: Vec<u8>,
}

pub struct IdentityStore {
    store: Mutex<HashMap<u64, Identity>>,
    next: std::sync::atomic::AtomicU64,
}

impl IdentityStore {
    pub fn new() -> Self {
        Self { store: Mutex::new(HashMap::new()), next: std::sync::atomic::AtomicU64::new(1) }
    }

    pub fn generate(&self, name: &str) -> u64 {
        use pqcrypto_kyber::kyber1024;
        use pqcrypto_dilithium::dilithium5;

        let (kem_pk, kem_sk) = kyber1024::keypair();
        let (sig_pk, sig_sk) = dilithium5::keypair();
        let did = format!("did:phantom:{}", &hex::encode(blake3::hash(name.as_bytes()).as_bytes())[..16]);

        let id = Identity {
            did,
            kem_public: kem_pk.as_bytes().to_vec(),
            kem_secret: kem_sk.as_bytes().to_vec(),
            signing_public: sig_pk.as_bytes().to_vec(),
            signing_secret: sig_sk.as_bytes().to_vec(),
        };
        let h = self.next.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
        self.store.lock().unwrap().insert(h, id);
        h
    }

    pub fn get_public_info(&self, handle: u64) -> Option<(String, Vec<u8>, Vec<u8>)> {
        self.store.lock().unwrap().get(&handle).map(|id| {
            (id.did.clone(), id.kem_public.clone(), id.signing_public.clone())
        })
    }

    pub fn sign(&self, handle: u64, message: &[u8]) -> Option<Vec<u8>> {
        use pqcrypto_dilithium::dilithium5;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let sk = dilithium5::SecretKey::from_bytes(&id.signing_secret).ok()?;
        let sig = dilithium5::detached_sign(message, &sk);
        Some(sig.as_bytes().to_vec())
    }

    pub fn verify(&self, handle: u64, message: &[u8], signature: &[u8]) -> Option<bool> {
        use pqcrypto_dilithium::dilithium5;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let pk = dilithium5::PublicKey::from_bytes(&id.signing_public).ok()?;
        let sig = DetachedSignature::from_bytes(signature).ok()?;
        Some(dilithium5::verify_detached_signature(&sig, message, &pk).is_ok())
    }

    pub fn encapsulate(&self, handle: u64) -> Option<(Vec<u8>, Vec<u8>)> {
        use pqcrypto_kyber::kyber1024;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let pk = kyber1024::PublicKey::from_bytes(&id.kem_public).ok()?;
        let (ss, ct) = kyber1024::encapsulate(&pk);
        Some((ct.as_bytes().to_vec(), ss.as_bytes().to_vec()))
    }

    pub fn decapsulate(&self, handle: u64, ciphertext: &[u8]) -> Option<Vec<u8>> {
        use pqcrypto_kyber::kyber1024;
        use pqcrypto_traits::kem::Ciphertext;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let ct = Ciphertext::from_bytes(ciphertext).ok()?;
        let sk = kyber1024::SecretKey::from_bytes(&id.kem_secret).ok()?;
        let ss = kyber1024::decapsulate(&ct, &sk);
        Some(ss.as_bytes().to_vec())
    }

    pub fn release(&self, handle: u64) {
        self.store.lock().unwrap().remove(&handle);
    }
}

// ── Shared handle store ────────────────────────────────────────────

// ── WASM bindings (thin wrapper) ───────────────────────────────────

static STORE: std::sync::LazyLock<IdentityStore> = std::sync::LazyLock::new(|| IdentityStore::new());

#[cfg(target_arch = "wasm32")]
fn wasm_json(val: serde_json::Value) -> Result<JsValue, JsValue> {
    #[cfg(target_arch = "wasm32")]
    { serde_wasm_bindgen::to_value(&val).map_err(|e| JsValue::from_str(&e.to_string())) }
    #[cfg(not(target_arch = "wasm32"))]
    { Ok(JsValue::from_str(&val.to_string())) }
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn generate_identity(name: &str) -> Result<JsValue, JsValue> {
    let handle = STORE.generate(name);
    let (did, pk, sig_pk) = STORE.get_public_info(handle).unwrap();
    wasm_json(serde_json::json!({
        "handle": handle, "did": did,
        "publicKey": hex::encode(pk), "signingPublicKey": hex::encode(sig_pk),
    }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn sign(handle: u64, message_hex: &str) -> Result<JsValue, JsValue> {
    let msg = hex::decode(message_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let sig = STORE.sign(handle, &msg).ok_or_else(|| JsValue::from_str("signing failed"))?;
    wasm_json(serde_json::json!({ "signature": hex::encode(sig) }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn verify(handle: u64, message_hex: &str, signature_hex: &str) -> Result<JsValue, JsValue> {
    let msg = hex::decode(message_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let sig_bytes = hex::decode(signature_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let valid = STORE.verify(handle, &msg, &sig_bytes).unwrap_or(false);
    wasm_json(serde_json::json!({ "valid": valid }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn encapsulate(handle: u64) -> Result<JsValue, JsValue> {
    let (ct, ss) = STORE.encapsulate(handle).ok_or_else(|| JsValue::from_str("encapsulate failed"))?;
    wasm_json(serde_json::json!({ "ciphertext": hex::encode(ct), "sharedSecret": hex::encode(ss) }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn decapsulate(handle: u64, ciphertext_hex: &str) -> Result<JsValue, JsValue> {
    let ct = hex::decode(ciphertext_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let ss = STORE.decapsulate(handle, &ct).ok_or_else(|| JsValue::from_str("decapsulate failed"))?;
    wasm_json(serde_json::json!({ "sharedSecret": hex::encode(ss) }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn get_did(handle: u64) -> Result<JsValue, JsValue> {
    let (did, _, _) = STORE.get_public_info(handle).ok_or_else(|| JsValue::from_str("not found"))?;
    wasm_json(serde_json::json!({ "did": did }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn release(handle: u64) { STORE.release(handle); }

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn phantom_hash(data_hex: &str) -> Result<JsValue, JsValue> {
    let data = hex::decode(data_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let h = blake3::hash(&data);
    wasm_json(serde_json::json!({ "hash": hex::encode(h.as_bytes()) }))
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen]
pub fn version() -> String { "phantom-sdk-wasm/0.1.0".to_string() }

// ── Native tests ───────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_and_sign() {
        let store = IdentityStore::new();
        let handle = store.generate("alice");
        let (did, _, _) = store.get_public_info(handle).unwrap();
        assert!(did.starts_with("did:phantom:"));

        let sig = store.sign(handle, b"hello phantom").unwrap();
        assert!(store.verify(handle, b"hello phantom", &sig).unwrap());
        assert!(!store.verify(handle, b"wrong message", &sig).unwrap());
        store.release(handle);
    }

    #[test]
    fn test_kem() {
        let store = IdentityStore::new();
        let h = store.generate("bob");
        let (ct, ss1) = store.encapsulate(h).unwrap();
        let ss2 = store.decapsulate(h, &ct).unwrap();
        assert_eq!(ss1, ss2);
        store.release(h);
    }

    #[test]
    fn test_multiple_identities() {
        let store = IdentityStore::new();
        let a = store.generate("alice");
        let b = store.generate("bob");
        assert_ne!(a, b);
        let sig_a = store.sign(a, b"msg").unwrap();
        assert!(store.verify(a, b"msg", &sig_a).unwrap());
        assert!(!store.verify(b, b"msg", &sig_a).unwrap());
        store.release(a);
        store.release(b);
    }
}

// ── C ABI (native builds) ──────────────────────────────────────────
//
// The same IdentityStore above, reachable from Bun through bun:ffi. This
// exists because every consumer of the Phantom SDK is a server-side Bun
// process, not a browser: a browser-targeted WASM bundle was never what they
// needed, and pqcrypto's C sources cannot compile for wasm32-unknown-unknown
// anyway, which has no libc. Natively they compile without complaint.
//
// Everything crosses the boundary as hex in NUL-terminated C strings. Binary
// buffers would be faster, but they would also mean length parameters, manual
// lifetime rules on both sides, and a much easier way to be wrong; the JS API
// already speaks hex, and identity operations are rare.
//
// Every returned string is heap-allocated here and must be handed back to
// phantom_free_string. Anything else leaks.
#[cfg(not(target_arch = "wasm32"))]
mod ffi {
    use super::STORE;
    use std::ffi::{CStr, CString};
    use std::os::raw::c_char;

    /// Move a Rust string across the boundary. Caller owns it afterwards.
    fn out(s: String) -> *mut c_char {
        match CString::new(s) {
            Ok(c) => c.into_raw(),
            Err(_) => std::ptr::null_mut(),
        }
    }

    /// Borrow a C string as &str, or None if it is null or not UTF-8.
    unsafe fn input<'a>(p: *const c_char) -> Option<&'a str> {
        if p.is_null() {
            return None;
        }
        CStr::from_ptr(p).to_str().ok()
    }

    #[no_mangle]
    pub extern "C" fn phantom_free_string(p: *mut c_char) {
        if p.is_null() {
            return;
        }
        unsafe { drop(CString::from_raw(p)) };
    }

    /// Returns a handle, or 0 on failure. Handles start at 1.
    #[no_mangle]
    pub unsafe extern "C" fn phantom_generate_identity(name: *const c_char) -> u64 {
        match input(name) {
            Some(n) => STORE.generate(n),
            None => 0,
        }
    }

    /// `{"did":…,"publicKey":…,"signingPublicKey":…}`, or null for an unknown handle.
    #[no_mangle]
    pub extern "C" fn phantom_identity_info(handle: u64) -> *mut c_char {
        match STORE.get_public_info(handle) {
            Some((did, kem_pk, sig_pk)) => out(
                serde_json::json!({
                    "did": did,
                    "publicKey": hex::encode(kem_pk),
                    "signingPublicKey": hex::encode(sig_pk),
                })
                .to_string(),
            ),
            None => std::ptr::null_mut(),
        }
    }

    #[no_mangle]
    pub unsafe extern "C" fn phantom_sign(handle: u64, message_hex: *const c_char) -> *mut c_char {
        let Some(msg) = input(message_hex).and_then(|h| hex::decode(h).ok()) else {
            return std::ptr::null_mut();
        };
        match STORE.sign(handle, &msg) {
            Some(sig) => out(hex::encode(sig)),
            None => std::ptr::null_mut(),
        }
    }

    /// 1 verified, 0 rejected, -1 could not be evaluated. Three outcomes, not
    /// two: "unknown handle" must not be indistinguishable from "bad signature".
    #[no_mangle]
    pub unsafe extern "C" fn phantom_verify(
        handle: u64,
        message_hex: *const c_char,
        signature_hex: *const c_char,
    ) -> i32 {
        let (Some(msg), Some(sig)) = (
            input(message_hex).and_then(|h| hex::decode(h).ok()),
            input(signature_hex).and_then(|h| hex::decode(h).ok()),
        ) else {
            return -1;
        };
        match STORE.verify(handle, &msg, &sig) {
            Some(true) => 1,
            Some(false) => 0,
            None => -1,
        }
    }

    /// `{"ciphertext":…,"sharedSecret":…}`, hex.
    #[no_mangle]
    pub extern "C" fn phantom_encapsulate(handle: u64) -> *mut c_char {
        match STORE.encapsulate(handle) {
            Some((ct, ss)) => out(
                serde_json::json!({
                    "ciphertext": hex::encode(ct),
                    "sharedSecret": hex::encode(ss),
                })
                .to_string(),
            ),
            None => std::ptr::null_mut(),
        }
    }

    #[no_mangle]
    pub unsafe extern "C" fn phantom_decapsulate(
        handle: u64,
        ciphertext_hex: *const c_char,
    ) -> *mut c_char {
        let Some(ct) = input(ciphertext_hex).and_then(|h| hex::decode(h).ok()) else {
            return std::ptr::null_mut();
        };
        match STORE.decapsulate(handle, &ct) {
            Some(ss) => out(hex::encode(ss)),
            None => std::ptr::null_mut(),
        }
    }

    #[no_mangle]
    pub unsafe extern "C" fn phantom_hash(data_hex: *const c_char) -> *mut c_char {
        let Some(data) = input(data_hex).and_then(|h| hex::decode(h).ok()) else {
            return std::ptr::null_mut();
        };
        out(hex::encode(blake3::hash(&data).as_bytes()))
    }

    #[no_mangle]
    pub extern "C" fn phantom_release(handle: u64) {
        STORE.release(handle);
    }

    #[no_mangle]
    pub extern "C" fn phantom_version() -> *mut c_char {
        out(format!("phantom-native/{}", env!("CARGO_PKG_VERSION")))
    }
}

#[cfg(all(test, not(target_arch = "wasm32")))]
mod store_tests {
    use super::IdentityStore;

    #[test]
    fn kem_round_trip_recovers_the_same_secret() {
        // Guards the direction of the tuple as much as the maths: kyber's
        // encapsulate returns (shared_secret, ciphertext), and swapping them
        // still type-checks because both are Vec<u8>.
        let store = IdentityStore::new();
        let h = store.generate("kem-round-trip");
        let (ciphertext, shared) = store.encapsulate(h).expect("encapsulate");
        let recovered = store.decapsulate(h, &ciphertext).expect("decapsulate");
        assert_eq!(recovered, shared, "decapsulate must recover the encapsulated secret");
    }

    #[test]
    fn signatures_verify_and_tampering_does_not() {
        let store = IdentityStore::new();
        let h = store.generate("sig");
        let sig = store.sign(h, b"hello").expect("sign");
        assert_eq!(store.verify(h, b"hello", &sig), Some(true));
        assert_eq!(store.verify(h, b"hell0", &sig), Some(false));
    }
}
