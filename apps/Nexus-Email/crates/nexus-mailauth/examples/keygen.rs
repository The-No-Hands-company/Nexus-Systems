//! Generate a DKIM key pair and print the DNS record to publish.
//!
//! The private key goes to a file the operator controls; only the public half
//! is ever published. Printing the private key to a terminal would put it in
//! shell history and scrollback.

use nexus_mailauth::{generate, private_key_pem, public_key_record};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let selector = std::env::args().nth(1).unwrap_or_else(|| "nexus".into());
    let domain = std::env::args().nth(2).unwrap_or_else(|| "tnhc.dev".into());
    // Defaults under $HOME, not the working directory. The project lives on an
    // NTFS/fuseblk volume where chmod is a no-op, so a key written there cannot
    // be protected by file permissions at all — and a DKIM private key is the
    // authority to send as this domain.
    let out = std::env::args().nth(3).unwrap_or_else(|| {
        let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
        format!("{home}/.config/nexus-email/dkim-private.pem")
    });
    if let Some(dir) = std::path::Path::new(&out).parent() {
        std::fs::create_dir_all(dir)?;
    }

    let key = generate(2048)?;
    std::fs::write(&out, private_key_pem(&key)?)?;
    // 0600: a DKIM private key is the authority to send as this domain.
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&out, std::fs::Permissions::from_mode(0o600))?;
    }

    // Verified rather than assumed: chmod silently does nothing on filesystems
    // without Unix permissions, and reporting 0600 when it did not happen is
    // worse than saying nothing.
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let mode = std::fs::metadata(&out)?.permissions().mode() & 0o777;
        if mode == 0o600 {
            println!("private key written to {out} (mode 0600) — never publish this file");
        } else {
            println!("private key written to {out}");
            println!("WARNING: permissions are {mode:o}, not 0600. This filesystem does not");
            println!("         enforce Unix permissions; move the key to one that does.");
        }
    }
    println!();
    println!("Publish this TXT record:");
    println!("  name:  {selector}._domainkey.{domain}");
    println!("  value: {}", public_key_record(&key)?);
    Ok(())
}
