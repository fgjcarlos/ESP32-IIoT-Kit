# local-portal — Capability Spec

## Requirements

### Requirement: Protected access flow
The portal MUST require a valid session before serving protected operations
and data. The system MUST reject unauthorized access with an HTTP 401 response
and an error payload the interface can render.

##### Scenario: Unlock with the correct credential
- **Given** the gateway AP is up and the portal is served
- **When** the owner submits the correct credential to the login endpoint
- **Then** the response succeeds, sets an HttpOnly session cookie, and
  subsequent protected requests are served

##### Scenario: Reject an incorrect credential
- **Given** the portal is served
- **When** a request submits an incorrect credential
- **Then** the response is 401 and no session token is minted

##### Scenario: Reject unauthorized access to protected data
- **Given** no valid session
- **When** a request targets a protected endpoint (status, credential change)
- **Then** the response is 401 with an error body, and no protected data is
  returned

##### Scenario: Credential change requires the current credential
- **Given** a valid session
- **When** the owner submits a credential change with a wrong current
  credential or mismatched repetition
- **Then** the change is rejected and the stored credential remains unchanged

### Requirement: First-use provisioning
The initial portal credential MUST be provisioned by the mechanism agreed in
the provisioning ADR (recorded before implementation). Credentials MUST be
stored only as digests in NVS; secrets MUST NOT appear in the repository. The
interface MUST prompt a change while the initial credential is in use.

##### Scenario: First boot provisions the credential
- **Given** the gateway boots with no stored portal credential
- **When** initialization completes
- **Then** the credential is provisioned per the ADR and only its digest is
  persisted in the `"portal"` NVS namespace

##### Scenario: First login prompts a change
- **Given** the initial provisioned credential is still in use
- **When** the owner unlocks the portal
- **Then** the interface prompts a credential change

### Requirement: Portal served from flash without external resources
The portal assets (HTML/CSS/JS) MUST be served from the firmware image in
flash. Pages MUST NOT require external resources; the portal MUST be usable
from a browser connected to the gateway AP with no STA and no Internet.

##### Scenario: Root and assets served from flash
- **Given** a client connected to the gateway AP
- **When** it requests the portal root and its assets
- **Then** each is served from the embedded image with the correct MIME type

##### Scenario: No external resource references
- **Given** the committed served assets
- **When** the embedded-asset integrity test runs
- **Then** every served path resolves to an embedded asset and no asset
  references an external origin

### Requirement: Status reflects real gateway state
The status endpoint MUST report the gateway's actual runtime state (AP, STA,
uptime) obtained from the firmware, not fixture data. API errors MUST be
communicated to the interface.

##### Scenario: AP up, STA down
- **Given** the gateway AP is active and STA is not connected
- **When** an authorized status request is made
- **Then** the response reports AP available and STA disconnected

##### Scenario: STA connected
- **Given** STA has an IP address
- **When** an authorized status request is made
- **Then** the response reports the STA state and its IP address

### Requirement: Testability
Auth rules, routes, API serialization and embedded assets MUST be covered by
Unity tests runnable host-side and under QEMU, plus interface tests for the
SPA. Browser-over-AP verification on physical hardware MUST be tagged `[hw]`.

##### Scenario: API, interface and embedded resource tests
- **Given** the implemented portal
- **When** the verification battery runs
- **Then** host tests, device tests under QEMU and the SPA test suite are
  green, and the browser walkthrough is documented as `[hw]` pending physical
  verification
