-- TNHC zero-retention: no client addresses / user agents in Supabase auth tables.
-- Wipe existing values, then force them on every INSERT/UPDATE.

-- auth.audit_log_entries.ip_address (varchar NOT NULL) -> ''
UPDATE auth.audit_log_entries SET ip_address = '' WHERE ip_address <> '';
CREATE OR REPLACE FUNCTION public.tnhc_blank_audit_ip() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN NEW.ip_address := ''; RETURN NEW; END $$;
DROP TRIGGER IF EXISTS tnhc_blank_audit_ip ON auth.audit_log_entries;
CREATE TRIGGER tnhc_blank_audit_ip BEFORE INSERT OR UPDATE ON auth.audit_log_entries
  FOR EACH ROW EXECUTE FUNCTION public.tnhc_blank_audit_ip();

-- auth.sessions.ip (inet NULL) and user_agent (text NULL) -> NULL
UPDATE auth.sessions SET ip = NULL, user_agent = NULL WHERE ip IS NOT NULL OR user_agent IS NOT NULL;
CREATE OR REPLACE FUNCTION public.tnhc_blank_session_client() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN NEW.ip := NULL; NEW.user_agent := NULL; RETURN NEW; END $$;
DROP TRIGGER IF EXISTS tnhc_blank_session_client ON auth.sessions;
CREATE TRIGGER tnhc_blank_session_client BEFORE INSERT OR UPDATE ON auth.sessions
  FOR EACH ROW EXECUTE FUNCTION public.tnhc_blank_session_client();

-- auth.mfa_challenges.ip_address (inet NOT NULL) -> 0.0.0.0
UPDATE auth.mfa_challenges SET ip_address = '0.0.0.0' WHERE ip_address <> '0.0.0.0';
CREATE OR REPLACE FUNCTION public.tnhc_blank_mfa_ip() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN NEW.ip_address := '0.0.0.0'; RETURN NEW; END $$;
DROP TRIGGER IF EXISTS tnhc_blank_mfa_ip ON auth.mfa_challenges;
CREATE TRIGGER tnhc_blank_mfa_ip BEFORE INSERT OR UPDATE ON auth.mfa_challenges
  FOR EACH ROW EXECUTE FUNCTION public.tnhc_blank_mfa_ip();
