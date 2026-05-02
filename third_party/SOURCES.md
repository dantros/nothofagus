# third_party — Upstream Sources

Dependencies are vendored via `git subtree`. Use the commands below to update a dependency to a new upstream commit or tag.

| Directory    | Remote URL                                    |
|--------------|-----------------------------------------------|
| `spdlog`     | https://github.com/gabime/spdlog.git          |
| `glfw`       | https://github.com/glfw/glfw.git              |
| `glm`        | https://github.com/g-truc/glm.git             |
| `imgui`      | https://github.com/ocornut/imgui.git          |
| `font8x8`    | https://github.com/dantros/font8x8.git        |
| `vk-bootstrap` | https://github.com/charles-lunarg/vk-bootstrap |
| `VulkanMemoryAllocator` | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator |
| `imgui-filebrowser` | https://github.com/AirGuanZ/imgui-filebrowser.git |

`glad` and `imgui_cmake` are custom local code — not managed by subtree.

## Updating a dependency

```bash
git subtree pull --prefix=third_party/<name> <url> <new-tag-or-commit> --squash
```

Example — update glfw to tag `3.5`:

```bash
git subtree pull --prefix=third_party/glfw https://github.com/glfw/glfw.git 3.5 --squash
```
