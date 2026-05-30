# third_party — Upstream Sources

Dependencies are vendored via `git subtree`. Use the commands below to update a dependency to a new upstream commit or tag.

| Directory    | Remote URL                                    | Current version |
|--------------|-----------------------------------------------|-----------------|
| `spdlog`     | https://github.com/gabime/spdlog.git          | 1.15.1 |
| `glfw`       | https://github.com/glfw/glfw.git              | 3.5.0 |
| `glm`        | https://github.com/g-truc/glm.git             | 1.0.2 |
| `imgui`      | https://github.com/ocornut/imgui.git          | v1.92.8 |
| `font8x8`    | https://github.com/dantros/font8x8.git        | 394cef70 |
| `vk-bootstrap` | https://github.com/charles-lunarg/vk-bootstrap | 6dc61686 |
| `VulkanMemoryAllocator` | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator | afd1c074 |
| `imgui-filebrowser` | https://github.com/AirGuanZ/imgui-filebrowser.git | — |
| `md4c`       | https://github.com/mity/md4c.git              | fb4d03d |
| `imgui_md`   | https://github.com/dantros/imgui_md.git       | 645f10d4 |

`glad` and `imgui_cmake` are custom local code — not managed by subtree.

## Updating a dependency

```bash
git subtree pull --prefix=third_party/<name> <url> <new-tag-or-commit> --squash
```

Example — update glfw to tag `3.5`:

```bash
git subtree pull --prefix=third_party/glfw https://github.com/glfw/glfw.git 3.5 --squash
```
