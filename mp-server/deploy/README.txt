HYRULE REBORN PUBLIC RELAY — CLUSTER OPERATIONS

Public directory: https://mlapi.us/z64/games/
WebSocket endpoint: wss://mlapi.us/z64/relay/ws
HTTP directory: https://mlapi.us/z64/relay/rooms
Health: https://mlapi.us/z64/relay/healthz

Deployed 2026-10-13 through dev / 192.168.50.4. Namespace z64, Deployment relay,
Service relay. The scheduler may place the pod on another cluster node.
Current service IP: 10.43.31.138:8080. Nginx on node-4 proxies /z64/relay/ via
/etc/nginx/conf.d/apps/mlapi.us/z64-relay.conf. The existing Cloudflare tunnel
provides public HTTPS/WSS. No public raw TCP port or router change is required.

The image is 192.168.50.78:30500/z64/relay:20261013.2. The deployment runs one
non-root instance, with read-only filesystem, no service-account token, bounded
CPU/memory, and HTTP readiness/liveness checks. Keep replicas at one: rooms and
quest records are in memory, so independent replicas would split game state.
Normal saves remain required. A restart drops connections and empty room state;
updated game clients reconnect and repopulate their saved shared quest state.

Only explicitly listed rooms appear in /rooms and LIST_ROOMS. Older clients
remain unlisted unless the public connector marks a newly created room listed.
Eight players per room, 128 rooms and 512 simultaneous connections are bounded.
Unlisted is discovery privacy, not authentication. There are no room passwords
or accounts in this version. The relay assigns sender IDs, enforces ownership,
normalizes roster/settings, caps frames/rates/retained queues, and scopes quest
state by room, team and randomizer seed. Native combat remains Anchor's existing
simulation; this is not an authoritative world server.

Build and release from mp-server:
  go test -race ./...
  CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -trimpath -ldflags='-s -w' -o deploy/go-anchor .
  scp deploy/go-anchor deploy/Dockerfile deploy/relay.yaml dev:/home/alec/z64-relay-stage/
  ssh dev 'sudo -n docker build -t 192.168.50.78:30500/z64/relay:NEW_VERSION /home/alec/z64-relay-stage'
  ssh dev 'sudo -n docker --config /home/alec/.docker push 192.168.50.78:30500/z64/relay:NEW_VERSION'
Update the version in relay.yaml before applying it. Never overwrite a published
tag. The relay-registry imagePullSecret already exists in namespace z64; its
credential is deliberately absent from this repository. A fresh namespace needs
the existing cluster registry credential provisioned by an administrator.
  ssh dev 'sudo -n k3s kubectl apply -f /home/alec/z64-relay-stage/relay.yaml'
  ssh dev 'sudo -n k3s kubectl -n z64 rollout status deployment/relay --timeout=60s'

If the Service is recreated, render z64-relay.nginx.conf.template with its new
ClusterIP. Install only this application's nginx file, run nginx -t, and reload
nginx. Existing site/download routes should be preserved. Updates interrupt live
rooms; check /rooms before scheduling maintenance.

Rollback a bad deployment:
  ssh dev 'sudo -n k3s kubectl -n z64 rollout undo deployment/relay'
  ssh dev 'sudo -n k3s kubectl -n z64 rollout status deployment/relay --timeout=60s'
This reverts the container, not player saves. Keeping an older image is necessary.
Do not remove the namespace, registry secret or unrelated nginx configuration.

The game enables SOH_PUBLIC_RELAY by default on desktop. IXWebSocket is pinned by
version and SHA-256 in CMake; native macOS trust uses Secure Transport, Windows
uses OpenSSL with its system certificate store. TCP remains available for LANs.
Windows desktop builds now require the openssl vcpkg dependency. Updated games
expose Public Games / Server Browser before connecting. The browser supports
public/unlisted creation, teams, counts, room settings and direct WSS joining.

For already published game releases, cmd/public-client bridges loopback TCP to
WSS. It is separately downloadable from /z64/games/. It does not upgrade older
clients' campaign implementation. Source and checksums ship in its ZIPs. Windows
x64 is cross-compiled but has not been run natively in this pass.

Validation: ../public_test.go, ../quests_test.go, ../cmd/public-client/main_test.go,
and ../../tests/public-relay/. The native C++ relay_probe uses the exact game
transport to verify real TLS, public discovery, two clients, quest party scaling,
and shared six-kill completion. Native macOS UI testing created and joined a real
public room from two separate game processes. The public page's Choose this game
flow and downloaded connector checksums were also verified.

Native Public Games menu (20261013.3)
-----------------------------------
The updated game opens Public Games when title transitions into file select.
Select Save returns to the original save screen; its Public Games button or
N64 Z shortcut reopens the directory. Tabs use LB/RB or [ / ]; A/Enter selects,
B/Escape backs out. Mouse and keyboard search remain available.

The list searches room, team, story, quest and chapter names. Filters cover
open/full parties, co-op/PvP and main-story stage. Counts include players still
in menus; in-game counts are separate. Public listings refresh every 15 seconds.
Selecting a room preserves its identity across refreshes. Join is disabled for
full rooms, failed/pending refreshes, missing names and missing teams.

Campaign summaries come from the loaded host, or the lowest-ID loaded client
that reports campaign metadata when the host cannot. The source player/team
is shown. The main-story stage is the first unfinished story milestone in the
normal dungeon order; randomized saves are labeled Randomized Adventure.
A tracked active/ready campaign quest includes its canonical title/chapter
and completed objective count. Titles come from the exported quest catalog;
clients cannot inject arbitrary quest titles. Game clients publish updates
on scene changes and every 100 player updates. Legacy clients show an honest
unreported status; players in file select have no campaign summary.

Source: soh/soh/Network/Anchor/ServerBrowserWindow.cpp and ServerBrowserModel.h.
Relay metadata: mp-server/campaign.go. Re-export quest_catalog.json with the
arpg_quest_party_catalog test target when changing campaign definitions.
Verification for this menu is in tests/public-relay/native-menu/.
