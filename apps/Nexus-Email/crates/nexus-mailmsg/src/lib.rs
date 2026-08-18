//! RFC 5322 and MIME: parsing the mail the world sends us, and generating the
//! mail we send back.
//!
//! Two rules run through this crate.
//!
//! **Parsing is total and lenient.** These functions face bytes chosen by
//! anonymous strangers on a public port. Nothing here panics, every loop is
//! bounded, and a single malformed header does not lose a message — real mail
//! is full of small violations that every other client tolerates.
//!
//! **Generation is strict.** CRLF everywhere, RFC 5322 dates, encoded words
//! for non-ASCII headers. Mail we emit has to survive strict receivers and,
//! later, carry a DKIM signature computed over exactly these bytes.

pub mod build;
pub mod encoding;
pub mod error;
pub mod headers;
pub mod mime;
pub mod parse;

pub use build::{Attachment, MessageBuilder};
pub use error::{MsgError, Result};
pub use headers::Headers;
pub use mime::{tree, ContentType, Part};
pub use parse::{parse, Limits, ParsedMessage};
