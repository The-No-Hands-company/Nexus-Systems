# Swedish data-retention law audit

**Audit date:** 2026-10-16  
**Scope:** The No Hands Company (TNHC) — Nexus Systems services reachable via tnhc.dev  
**Conclusion:** TNHC is not subject to mandatory traffic-data retention under Swedish law; GDPR requires only that we process personal data lawfully and minimally, which our zero-retention policy already satisfies.

---

## 1. Applicable law

TNHC is an informal project registered and operated in Sweden. Two bodies of law are relevant.

### 1.1 GDPR (EU Regulation 2016/679)

GDPR applies because TNHC processes personal data of persons in the EU/EEA. Its core obligations:

- **Lawful basis.** Processing must rest on a legal basis (contract, legitimate interest, consent, …). TNHC's position: we store only what is strictly necessary to provide the service (account credentials, sign-in events for 30 days, mail and files the user chooses to store).
- **Data minimisation.** We must not collect more than necessary. Our zero-retention policy — no visitor IP addresses stored; technical logs capped at 5 MB each and overwritten; no analytics — is the gold standard for minimisation.
- **Storage limitation.** Personal data must not be kept longer than needed. Sign-in events deleted after 30 days; waitlist email deleted on invite; no logs retained beyond their rolling cap.
- **Right to erasure, portability, access.** Partially implemented; tracked in the privacy status page.

No Swedish or EU law _requires_ TNHC to retain visitor IP addresses or traffic data. Collecting them would need a lawful basis; under GDPR it is safer and simpler not to collect them at all, which is what we do.

### 1.2 Mandatory traffic-data retention (LEK / Directive 2006/24/EC)

The EU Data Retention Directive 2006/24/EC, which required member states to compel retention of traffic data, was **declared invalid** by the Court of Justice of the EU in _Digital Rights Ireland_ (C-293/12, C-594/12, 2014) and the _Tele2/Watson_ follow-up (C-203/15, C-698/15, 2016). Sweden's implementing legislation has accordingly not been enforced since the 2016 ruling, and targeted retention (under court order for specific investigations) is the only form the CJEU permits.

The Swedish Act on Electronic Communications (_Lag (2003:389) om elektronisk kommunikation_, "LEK") — now replaced and updated — imposes traffic-data retention only on **publicly available electronic communications services** in the telecom/ISP sense (telephone operators, internet access providers). It does **not** apply to application-layer service providers such as TNHC.

**TNHC does not provide internet access or public telephone services.** It provides application-layer services (chat, file hosting, authentication, email relay). It is therefore not an operator within the meaning of LEK and is **not subject** to any mandatory traffic-data retention obligation under Swedish law.

### 1.3 Swedish police/intelligence access

Swedish law enforcement may compel disclosure of data under court orders (_tvångsmedel_), which are generally public. Sweden's intelligence service (SÄPO) and signals intelligence agency (FRA) operate under separate legislation. Key points:

- There are no US-style National Security Letters that carry a mandatory gag preventing disclosure of the existence of a request.
- Swedish court orders are typically not secret; their existence can be disclosed.
- The FRA operates on foreign signals and is not generally relevant to a domestic application service.
- Any compulsory process served on TNHC would ordinarily be disclosable. The warrant canary at `docs/privacy/warrant-canary.md` (and `status.tnhc.dev/canary.json`) states whether any such order has been received.

---

## 2. Audit result

| Question | Answer |
|---|---|
| Is TNHC required to retain visitor IP addresses or traffic data? | **No.** We are not a telecom operator; mandatory retention rules do not apply. |
| Does our zero-retention policy comply with GDPR? | **Yes.** Collecting no visitor IP addresses satisfies data-minimisation and storage-limitation requirements. |
| Does GDPR require us to retain anything we currently do not? | **No.** What we retain (account details, sign-in events 30 days, user-stored content) has a clear lawful basis (contract performance). |
| Can Swedish authorities compel secret data disclosure? | **Not in general.** Court orders are ordinarily public; there are no NSL-equivalent instruments. The warrant canary tracks this. |

---

## 3. Residual risks and mitigations

- **Cloudflare as a data processor.** All traffic passes through Cloudflare, which holds IP addresses in its own logs. TNHC cannot control what Cloudflare retains. Mitigation: documented as a known limit; planned future mitigation via onion/Phantom routing.
- **Resend and Cloudflare Email Routing.** See known-limits section of the privacy status page. No additional retention obligation created for TNHC by GDPR.
- **GDPR Article 6(1)(f) legitimate interest.** If we ever needed to retain data for fraud prevention, a legitimate-interest assessment (LIA) would be required. Currently not applicable.

---

_This document is reviewed whenever the privacy architecture programme advances. Next review trigger: any change to TNHC's service scope or registered entity status._
