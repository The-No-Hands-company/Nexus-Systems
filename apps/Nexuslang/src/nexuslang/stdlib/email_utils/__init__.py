"""
NLPL Standard Library - Email/SMTP Module
Email composition and sending via SMTP
"""

import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email import encoders
from typing import Any, Dict, List, Optional


def register_email_functions(runtime: Any) -> None:
    """Register email functions with the runtime."""
    
    # SMTP connection
    runtime.register_function("smtp_connect", smtp_connect)
    runtime.register_function("smtp_send", smtp_send)
    runtime.register_function("smtp_close", smtp_close)
    
    # Message creation
    runtime.register_function("create_email", create_email)
    runtime.register_function("email_add_recipient", email_add_recipient)
    runtime.register_function("email_add_cc", email_add_cc)
    runtime.register_function("email_add_bcc", email_add_bcc)
    runtime.register_function("email_set_body", email_set_body)
    runtime.register_function("email_add_attachment", email_add_attachment)
    
    # Quick send
    runtime.register_function("send_email", send_email)
    runtime.register_function("send_html_email", send_html_email)


# Global SMTP connection storage
_smtp_connections: Dict[str, smtplib.SMTP] = {}
_connection_counter = 0


# =======================
# SMTP Connection
# =======================

def smtp_connect(host: str, port: int = 587, use_tls: bool = True, 
                 username: str = "", password: str = "") -> str:
    """
    Connect to SMTP server.
    Returns connection ID for use with other functions.
    """
    global _connection_counter
    try:
        smtp = smtplib.SMTP(host, port)
        
        if use_tls:
            smtp.starttls()
        
        if username and password:
            smtp.login(username, password)
        
        conn_id = f"smtp_{_connection_counter}"
        _smtp_connections[conn_id] = smtp
        _connection_counter += 1
        
        return conn_id
    except smtplib.SMTPException as e:
        raise RuntimeError(f"SMTP connection error: {e}")
    except Exception as e:
        raise RuntimeError(f"Error connecting to SMTP server: {e}")


def smtp_send(conn_id: str, message: MIMEMultipart) -> bool:
    """Send email message using SMTP connection."""
    try:
        if conn_id not in _smtp_connections:
            raise RuntimeError(f"Invalid SMTP connection ID: {conn_id}")
        
        smtp = _smtp_connections[conn_id]
        
        from_addr = message['From']
        to_addrs = message['To'].split(',')
        
        smtp.send_message(message)
        return True
    except smtplib.SMTPException as e:
        raise RuntimeError(f"SMTP send error: {e}")
    except Exception as e:
        raise RuntimeError(f"Error sending email: {e}")


def smtp_close(conn_id: str) -> bool:
    """Close SMTP connection."""
    try:
        if conn_id in _smtp_connections:
            _smtp_connections[conn_id].quit()
            del _smtp_connections[conn_id]
            return True
        return False
    except Exception as e:
        raise RuntimeError(f"Error closing SMTP connection: {e}")


# =======================
# Message Creation
# =======================

def create_email(from_addr: str, subject: str) -> MIMEMultipart:
    """Create new email message."""
    try:
        msg = MIMEMultipart()
        msg['From'] = from_addr
        msg['Subject'] = subject
        return msg
    except Exception as e:
        raise RuntimeError(f"Error creating email: {e}")


def email_add_recipient(message: MIMEMultipart, email: str) -> bool:
    """Add recipient to To field."""
    try:
        current = message.get('To', '')
        if current:
            message['To'] = f"{current},{email}"
        else:
            message['To'] = email
        return True
    except Exception as e:
        raise RuntimeError(f"Error adding recipient: {e}")


def email_add_cc(message: MIMEMultipart, email: str) -> bool:
    """Add recipient to CC field."""
    try:
        current = message.get('Cc', '')
        if current:
            message['Cc'] = f"{current},{email}"
        else:
            message['Cc'] = email
        return True
    except Exception as e:
        raise RuntimeError(f"Error adding CC: {e}")


def email_add_bcc(message: MIMEMultipart, email: str) -> bool:
    """Add recipient to BCC field."""
    try:
        current = message.get('Bcc', '')
        if current:
            message['Bcc'] = f"{current},{email}"
        else:
            message['Bcc'] = email
        return True
    except Exception as e:
        raise RuntimeError(f"Error adding BCC: {e}")


def email_set_body(message: MIMEMultipart, body: str, is_html: bool = False) -> bool:
    """Set email body (plain text or HTML)."""
    try:
        content_type = 'html' if is_html else 'plain'
        msg_body = MIMEText(body, content_type)
        message.attach(msg_body)
        return True
    except Exception as e:
        raise RuntimeError(f"Error setting email body: {e}")


def email_add_attachment(message: MIMEMultipart, filepath: str, filename: str = "") -> bool:
    """Add file attachment to email."""
    try:
        import os
        
        if not filename:
            filename = os.path.basename(filepath)
        
        with open(filepath, 'rb') as f:
            attachment = MIMEBase('application', 'octet-stream')
            attachment.set_payload(f.read())
        
        encoders.encode_base64(attachment)
        attachment.add_header('Content-Disposition', f'attachment; filename={filename}')
        message.attach(attachment)
        
        return True
    except FileNotFoundError:
        raise RuntimeError(f"Attachment file not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error adding attachment: {e}")


# =======================
# Quick Send Functions
# =======================

def send_email(host: str, port: int, username: str, password: str,
               from_addr: str, to_addr: str, subject: str, body: str,
               use_tls: bool = True) -> bool:
    """
    Quick send plain text email.
    All-in-one function for simple emails.
    """
    try:
        # Create message
        msg = MIMEMultipart()
        msg['From'] = from_addr
        msg['To'] = to_addr
        msg['Subject'] = subject
        msg.attach(MIMEText(body, 'plain'))
        
        # Connect and send
        smtp = smtplib.SMTP(host, port)
        if use_tls:
            smtp.starttls()
        if username and password:
            smtp.login(username, password)
        
        smtp.send_message(msg)
        smtp.quit()
        
        return True
    except smtplib.SMTPException as e:
        raise RuntimeError(f"Email send error: {e}")
    except Exception as e:
        raise RuntimeError(f"Error sending email: {e}")


def send_html_email(host: str, port: int, username: str, password: str,
                    from_addr: str, to_addr: str, subject: str, 
                    html_body: str, use_tls: bool = True) -> bool:
    """
    Quick send HTML email.
    All-in-one function for HTML emails.
    """
    try:
        # Create message
        msg = MIMEMultipart('alternative')
        msg['From'] = from_addr
        msg['To'] = to_addr
        msg['Subject'] = subject
        msg.attach(MIMEText(html_body, 'html'))
        
        # Connect and send
        smtp = smtplib.SMTP(host, port)
        if use_tls:
            smtp.starttls()
        if username and password:
            smtp.login(username, password)
        
        smtp.send_message(msg)
        smtp.quit()
        
        return True
    except smtplib.SMTPException as e:
        raise RuntimeError(f"Email send error: {e}")
    except Exception as e:
        raise RuntimeError(f"Error sending email: {e}")
