# Dev Container

GPU-enabled dev environment (OpenGL + Vulkan, Wayland-only windowing) for Linux
and Windows (via Docker Desktop's WSL2 backend). No native Windows-container
support — Docker Desktop on Windows always runs these as Linux containers.

## Files

| File | Purpose |
|---|---|
| [`Dockerfile`](../Dockerfile) (repo root) | Ubuntu 26.04 image: build toolchain, OpenGL/Vulkan/Wayland dev libs, pinned CMake + Vulkan SDK, SwiftShader, non-root `developer` user. Same image for every platform — only the *host* GPU/display passthrough differs. |
| [`docker-compose.yml`](../docker-compose.yml) (repo root) | Base compose service definition (builds the image above, mounts the repo at `/workspace`). No GPU/display wiring of its own — that comes from an override file. |
| [`devcontainer.json`](devcontainer.json) | VS Code Dev Containers config: builds via `docker-compose.yml` + `docker-compose.override.json` (see below), sets up the C++/CMake extensions, bash terminal, `developer` remote user. |
| [`docker-compose.linux.override.json`](docker-compose.linux.override.json) | **Template.** GPU/Wayland forwarding for Linux hosts: `/dev/dri` + the host Wayland socket. |
| [`docker-compose.windows.override.json`](docker-compose.windows.override.json) | **Template.** Same for Windows hosts (Docker Desktop + WSL2/WSLg): `/dev/dxg`, `/mnt/wslg`, `/usr/lib/wsl`. |
| `docker-compose.override.json` | **Not committed.** Your personal, host-specific copy — see below. Drives both the manual `docker compose` commands and `devcontainer.json`'s `dockerComposeFile`. |

## Activate your platform

The `.linux.` / `.windows.` files are templates, not live config. Copy the one
matching your host to the non-suffixed name:

```bash
# Linux
cp .devcontainer/docker-compose.linux.override.json .devcontainer/docker-compose.override.json

# Windows (run from inside your WSL2 distro shell, not PowerShell)
cp .devcontainer/docker-compose.windows.override.json .devcontainer/docker-compose.override.json
```

`docker-compose.override.json` is gitignored on purpose — **never commit
it**. It's your local copy and may need host-specific tweaks (see the
caveats below). Do this **before** opening the folder in VS Code — see
"Known limitation" below.

### Windows caveats

The `.windows.` templates follow [Microsoft's own WSLg container sample](https://github.com/microsoft/wslg/blob/main/samples/container/Containers.md).
They have **not been verified on real hardware** from this environment —
treat them as a starting point:

- Everything (`docker compose`, `docker build`, opening in VS Code) must run
  from **inside the WSL2 distro** (or with Docker Desktop's WSL2 integration
  enabled for that distro) — `/dev/dxg` and `/mnt/wslg` only exist there, not
  in native Windows.
- `/dev/dri/card0` and `/dev/dri/renderD128` aren't present on every
  WSL/driver version. If the container fails to start complaining about a
  missing device, delete those two lines and keep only `/dev/dxg`.
- The override's `LD_LIBRARY_PATH` **replaces** the Dockerfile's own
  `ENV LD_LIBRARY_PATH` (Vulkan SDK + SwiftShader) rather than appending to
  it — that's why the template repeats `/opt/vulkan/x86_64/lib` and
  `/opt/swiftshader` alongside `/usr/lib/wsl/lib`. If either path changes in
  the `Dockerfile`, update it here too.

## Commands

`docker compose` does not auto-discover a `.json`-named override file (only
`docker-compose.override.yml`/`.yaml` are picked up automatically), so pass
both files explicitly every time:

```bash
# Build (or rebuild after a Dockerfile change)
docker compose -f docker-compose.yml -f .devcontainer/docker-compose.override.json build

# Start the container in the background
docker compose -f docker-compose.yml -f .devcontainer/docker-compose.override.json up -d

# Get a shell inside the running container
docker compose -f docker-compose.yml -f .devcontainer/docker-compose.override.json exec app bash

# Stop and remove the container
docker compose -f docker-compose.yml -f .devcontainer/docker-compose.override.json down
```

Consider a shell alias/function for the repeated `-f -f` pair if you use this
often, e.g. `alias dcnf='docker compose -f docker-compose.yml -f .devcontainer/docker-compose.override.json'`.

## Known limitation

`devcontainer.json` references `docker-compose.override.json` directly via
`dockerComposeFile`, so it must already exist before you run "Reopen in
Container" — do the copy step above first, or VS Code fails with a
missing-file error.
