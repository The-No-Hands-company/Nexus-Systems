UPDATE auth_audit_log SET ip = NULL, user_agent = NULL;
ALTER TABLE auth_audit_log DROP COLUMN IF EXISTS ip;
ALTER TABLE auth_audit_log DROP COLUMN IF EXISTS user_agent;
ALTER TABLE auth_audit_log ADD COLUMN IF NOT EXISTS device_id text;
DELETE FROM auth_audit_log WHERE created_at < now() - interval '30 days';
