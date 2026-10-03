/**
 * The Nexus Cloud discovery manifest. It advertises only what this node
 * actually serves; the peer discovery and registration endpoints it used to
 * list were placeholders that reported success without doing anything.
 */
export const federationEndpoints = {
  wellKnown: () => ({
    service: "nexus-forge",
    version: "0.2.0",
    capabilities: ["git-smart-http", "signed-push-policy", "ref-log"],
    endpoints: {
      repositories: "/api/repos",
    },
  }),
};
