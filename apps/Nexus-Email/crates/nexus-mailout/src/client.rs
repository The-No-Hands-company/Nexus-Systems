use std::time::Duration;

use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpStream;

use crate::reply::{classify, code_of, is_final_line, Disposition};

/// The outcome of one delivery attempt to one host.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Attempt {
    Delivered,
    /// The server refused permanently. Bounce; do not retry.
    Rejected(String),
    /// Anything that might work later: 4xx, a dropped connection, a timeout,
    /// a filtered port.
    Deferred(String),
}

/// Per-step timeout. A server that accepts a connection and then says nothing
/// would otherwise pin a delivery worker indefinitely.
const STEP_TIMEOUT: Duration = Duration::from_secs(60);

/// Deliver one message to one host over SMTP.
///
/// Everything that is not an explicit permanent refusal is deferred. That
/// asymmetry is deliberate: deferring a message that was truly undeliverable
/// costs a few days of retries and one bounce, while bouncing a message that
/// would have gone through loses it, and the sender has no copy.
pub async fn deliver(
    host: &str,
    port: u16,
    ehlo_name: &str,
    from: &str,
    recipient: &str,
    data: &[u8],
) -> Attempt {
    let stream = match tokio::time::timeout(STEP_TIMEOUT, TcpStream::connect((host, port))).await {
        Ok(Ok(s)) => s,
        // A refused or filtered port is the case that matters most on a
        // residential connection: port 25 is blocked, and this must read as
        // "cannot deliver from here", never as "this address is invalid".
        Ok(Err(e)) => return Attempt::Deferred(format!("connect to {host}:{port} failed: {e}")),
        Err(_) => return Attempt::Deferred(format!("connect to {host}:{port} timed out")),
    };

    let (read_half, mut write) = stream.into_split();
    let mut reader = BufReader::new(read_half);

    macro_rules! expect {
        ($what:expr) => {
            match read_reply(&mut reader).await {
                Ok((code, text)) => match classify(code) {
                    Disposition::Ok => text,
                    Disposition::Permanent => {
                        return Attempt::Rejected(format!("{} rejected {}: {}", host, $what, text))
                    }
                    Disposition::Temporary => {
                        return Attempt::Deferred(format!("{} deferred {}: {}", host, $what, text))
                    }
                },
                Err(e) => return Attempt::Deferred(format!("reading {} from {host}: {e}", $what)),
            }
        };
    }

    macro_rules! say {
        ($line:expr) => {
            if let Err(e) = write.write_all(format!("{}\r\n", $line).as_bytes()).await {
                return Attempt::Deferred(format!("writing to {host}: {e}"));
            }
        };
    }

    expect!("greeting");
    say!(format!("EHLO {ehlo_name}"));
    expect!("EHLO");

    // The empty reverse path is legal and is how bounces are sent.
    say!(format!("MAIL FROM:<{from}>"));
    expect!("MAIL FROM");

    say!(format!("RCPT TO:<{recipient}>"));
    expect!("RCPT TO");

    say!("DATA");
    expect!("DATA");

    // Dot-stuff: a body line starting with a dot must be doubled, or the
    // receiving server reads it as the end of the message and truncates.
    let mut payload = Vec::with_capacity(data.len() + 64);
    for line in data.split(|b| *b == b'\n') {
        let line = line.strip_suffix(b"\r").unwrap_or(line);
        if line.starts_with(b".") {
            payload.push(b'.');
        }
        payload.extend_from_slice(line);
        payload.extend_from_slice(b"\r\n");
    }
    payload.extend_from_slice(b".\r\n");

    if let Err(e) = write.write_all(&payload).await {
        return Attempt::Deferred(format!("writing message to {host}: {e}"));
    }
    let accepted = expect!("end of DATA");

    say!("QUIT");
    // The reply to QUIT is not worth waiting for: the message is already
    // accepted, and a server that hangs up rudely has still taken it.
    tracing::info!(%host, %recipient, "delivered: {}", accepted.trim());
    Attempt::Delivered
}

/// Read a complete reply, following multi-line continuations.
async fn read_reply<R>(reader: &mut BufReader<R>) -> Result<(u16, String), String>
where
    R: tokio::io::AsyncRead + Unpin,
{
    let mut collected = String::new();

    loop {
        let mut line = String::new();
        let read = tokio::time::timeout(STEP_TIMEOUT, reader.read_line(&mut line))
            .await
            .map_err(|_| "timed out".to_string())?
            .map_err(|e| e.to_string())?;
        if read == 0 {
            return Err("connection closed".into());
        }

        let code = code_of(&line).ok_or_else(|| format!("unparseable reply: {}", line.trim()))?;
        collected.push_str(line.trim());
        if is_final_line(&line) {
            return Ok((code, collected));
        }
        collected.push(' ');
    }
}
