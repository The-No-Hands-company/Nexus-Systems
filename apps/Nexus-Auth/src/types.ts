export type IdentityRole = "admin" | "user";

export type SeedIdentity = {
  id: string;
  username: string;
  role: IdentityRole;
  createdAt: string;
};

export type ServiceTokenPayload = {
  iss: "nexus-auth";
  sub: string;
  aud: string;
  scopes: string[];
  iat: number;
  exp: number;
  jti: string;
};

export type ValidateTokenResult =
  | {
      valid: true;
      payload: ServiceTokenPayload;
    }
  | {
      valid: false;
      reason: string;
    };
