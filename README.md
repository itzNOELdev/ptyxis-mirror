![Ptyxis Logo](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/icons/ptyxis.svg)

[![License: GPL v3+](https://img.shields.io/badge/License-GPL%20v3%2B-blue.svg)](https://gitlab.gnome.org/chergert/ptyxis/-/blob/main/COPYING)
[![GitLab Pipeline Status](https://gitlab.gnome.org/chergert/ptyxis/badges/main/pipeline.svg)](https://gitlab.gnome.org/chergert/ptyxis/-/pipelines)
[![Distribution: Flatpak (Stable)](https://img.shields.io/badge/Available%20on-Flatpak%20(Stable)-4d9c9a.svg?logo=flatpak&logoColor=white)](https://flathub.org/apps/app.devsuite.Ptyxis)
[![Distribution: Flatpak (Nightly)](https://img.shields.io/badge/Available%20on-Flatpak%20(Nightly)-4d9c9a.svg?logo=flatpak&logoColor=white)](https://gitlab.gnome.org/chergert/ptyxis#flatpak)

# Ptyxis

Ptyxis is a simple, clean terminal for GNOME with container integration.
You can open a terminal on your host or in a Podman, Toolbox, or Distrobox
container. New tabs can keep the current container and working directory.

![Ptyxis Screenshot - Tab Overview](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/tab-overview.png)
_Tab overview._

---

- [About](#about)
- [Key Features](#key-features)
- [Screenshots](#screenshots)
- [Installation](#installation)
- [Basic Usage & Command-Line Options](#basic-usage--command-line-options)
- [Container Integration](#container-integration)
- [Contributing: Feature Requests and Design Changes](#contributing-feature-requests-and-design-changes)
- [Report an Issue](https://gitlab.gnome.org/chergert/ptyxis/-/issues)

---

## About

Ptyxis uses GTK 4, libadwaita, and VTE. It has tabs, profiles, color palettes,
and configurable shortcuts. Container support is built into the terminal, so
you can choose a container when opening a tab or set one as the default for a
profile.

The stable Flatpak still uses the application ID `app.devsuite.Ptyxis`.

## Key Features

- **Containers:** Discover local Podman, Toolbox, and Distrobox containers and
  open tabs in them. New tabs can inherit the current container and directory.
- **Tabs:** Use the tab overview to find a tab. Pinned tabs can be restored when
  Ptyxis starts again.
- **Profiles:** Set a default container, startup command, font, palette, and
  other terminal preferences for a profile.
- **Appearance:** Choose from built-in palettes or install your own. Palettes
  support light and dark styles; window styling and transparency are optional.
- **Shortcuts:** Change keyboard shortcuts in Preferences.
- **Process status:** See when a terminal is running `sudo` or connected over
  SSH.
- **Terminal inspector:** Inspect terminal sequences while developing terminal
  applications.

---

## Screenshots

![Container Menu shows automatically discovered containers](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/containers-menu.png)
_Container menu._

![Tab Overview screen shows multiple tabs with previews](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/tab-overview.png)
_Tab overview._

![Behavior Settings window for tweaking terminal behavior](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/change-behavior.png)
_Behavior settings._

![Palette Selector screen for choosing color schemes](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/palette-selector.png)
_Palette selection._

![Profile Editor for customizing profiles](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/edit-profile.png)
_Profile settings._

![Shortcut Editor for remapping keyboard shortcuts](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/shortcut-editing.png)
_Shortcut settings._

![Sudo Tracking indicator in the terminal](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/sudo-tracking.png)
_Sudo indicator._

![SSH Tracking indicator in the terminal](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/ssh-tracking.png)
_SSH indicator._

![Terminal with a partially transparent background](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/transparency.png)
_Background transparency._

![Terminal showing an overlay when the column or row size changes](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/columns-and-rows.png)
_Column and row size while resizing._

---

## Installation

### Flatpak

The stable Flatpak is available from Flathub. A development build is available
from GNOME Nightly.

- **Stable (Flathub):** `app.devsuite.Ptyxis`

  ```bash
  flatpak install flathub app.devsuite.Ptyxis
  ```

  To run: `flatpak run app.devsuite.Ptyxis`

- **Development (GNOME Nightly):** `org.gnome.Ptyxis.Devel`

  ```bash
  flatpak install --user --from https://nightly.gnome.org/repo/appstream/org.gnome.Ptyxis.Devel.flatpakref
  ```

  To run: `flatpak run org.gnome.Ptyxis.Devel`

### Building from Source

Ptyxis uses the Meson build system.

1. **Dependencies:** Install the build tools and development packages:

    - C compiler (e.g., GCC, Clang)
    - Meson (version 1.0.0 or newer)
    - Ninja
    - GLib (version 2.90 or newer, e.g., `libglib2.0-dev`)
    - GTK4 (version 4.14 or newer, e.g., `libgtk-4-dev`)
    - libadwaita (version 1.8 or newer, e.g., `libadwaita-1-dev`)
    - JSON-GLib (version 1.6 or newer, e.g., `libjson-glib-dev`)
    - VTE (GTK4 version, 0.79 or newer, e.g., `libvte-2.91-gtk4-dev`)
    - libportal-gtk4 (on Linux, e.g., `libportal-gtk4-dev`)

2. **Clone the repository:**

    ```bash
    git clone https://gitlab.gnome.org/chergert/ptyxis.git
    cd ptyxis
    ```

3. **Configure the build using Meson:**

    ```bash
    meson setup _build --prefix=/usr/local --buildtype=release
    # Or --buildtype=debug for development
    ```

4. **Compile:**

    ```bash
    meson compile -C _build
    ```

5. **Install (optional):**

    ```bash
    sudo meson install -C _build
    ```

**GNOME Builder:** Open the cloned `ptyxis/` directory in
[Builder](https://apps.gnome.org/Builder/) to build and run it there.

---

## Basic Usage & Command-Line Options

Launch Ptyxis from the app grid or run `ptyxis` in a terminal if it is installed
on your `PATH`.

**Common options:**

- `--version`: Show the program version.
- `--preferences`: Open the preferences window directly.
- `--new-window`: Open a new window.
- `--tab`: Open a tab in the active window.
- `--tab-with-profile=PROFILE_UUID`: Open a tab using a profile. Copy its UUID
  from Preferences → Profiles.
  Example: `ptyxis --tab-with-profile=a1b2c3d4-e5f6-7890-1234-567890abcdef`
- `-x "COMMAND"`, `--execute "COMMAND"`: Run a command in a new window.
  Example: `ptyxis -x "htop"`
- `-- PROGRAM [ARGUMENTS...]`: Run a program with arguments in a new window.
  Example: `ptyxis -- htop -u myuser`
- `-d DIR`, `--working-directory DIR`: Set the working directory for a new tab,
  window, or command.
  Example: `ptyxis -d /home/user/myproject`
- `--title=TITLE`: Set the initial tab title.
- `--maximize`: Open the new window maximized.
- `--fullscreen`: Open the new window in fullscreen mode.
- `--import-palette=FILE`: Import a Ptyxis `.palette` file.
- `-s`, `--standalone`: Start a separate instance.
- `-h`, `--help`: Show a summary of options.

See `ptyxis --help` or the `ptyxis(1)` man page for the full list.

---

## How Ptyxis Works

Ptyxis has a GTK 4 application and a separate `ptyxis-agent` process.

### UI Application

The application handles:

- Windows and tabs.
- Profiles, palettes, and shortcuts.
- Terminal display through VTE.
- Communication with `ptyxis-agent`.

### `ptyxis-agent`

The agent usually runs on the host, including when the application is a
Flatpak. It handles:

- Creating PTYs and starting processes.
- Finding containers and starting shells in them.
- Reporting process and system information to the application.

The application and agent communicate over a local socket.

---

## Container Integration

Ptyxis can open terminals in local containers and show which container a tab
uses.

### Technical Details

1. **Discovery:** `ptyxis-agent` looks for containers on the host and reports
    them to the application.
2. **Starting a terminal:** The agent creates a PTY and starts the shell in the
    selected container.
3. **Tab context:** Ptyxis tracks the container and working directory so a new
    tab can start in the same place. Profiles can specify a default container.

### `ptyxis-agent` Functions in Container Management

- Discovers containers and starts processes in them.
- Tracks processes started in tabs and reports their exit status.
- Can place tabs in systemd user scopes when supported by the host.

### Supported Container Technologies

- **Podman:** Supported.
- **Toolbox:** Supported.
- **Distrobox:** Supported.
- **JHBuild:** Can be identified as a separate environment. To mark a JHBuild
  session, add this to your shell configuration:

  ```bash
  if [ -t 1 -a x$UNDER_JHBUILD != x ]; then
      printf "\033]777;container;push;%s;jhbuild\033\\" "JHBuild"
      function pop_container {
          printf "\033]777;container;pop;;\033\\"
      }
      trap pop_container EXIT
  fi
  ```

- **Docker:** You can run the `docker` command, but Ptyxis does not list Docker
  containers in its container menu.
- **systemd-nspawn:** There is no dedicated integration.

### Note on Container Security

Container integration does not change the isolation provided by your container
runtime. Check that runtime's settings before running untrusted software.

---

## Appearance and Profiles

- **Color Palettes:**
  - Built-in palettes have light and dark variants.
  - Put custom `.palette` files in:
    - Native: `~/.local/share/org.gnome.Ptyxis/palettes/`
    - Flatpak (Stable): `~/.var/app/app.devsuite.Ptyxis/data/app.devsuite.Ptyxis/palettes/`
    - Flatpak (Nightly): `~/.var/app/org.gnome.Ptyxis.Devel/data/org.gnome.Ptyxis.Devel/palettes/`
  - You can also import a palette with `ptyxis --import-palette FILE`.
  - Window Dressing applies palette colors to the window.
- **Keyboard Shortcuts:**
  - Change shortcuts in Preferences.
- **Transparency:**
  - Set a profile's `opacity` key to a value between 0 and 1. These examples
    use `$PTYXIS_PROFILE` for the profile UUID and set opacity to 0.85:

    ```bash
    # If using distro packaging
    gsettings set org.gnome.Ptyxis.Profile:/org/gnome/Ptyxis/Profiles/$PTYXIS_PROFILE/ opacity .85

    # If using app.devsuite.Ptyxis from Flathub
    flatpak run --command=gsettings app.devsuite.Ptyxis set org.gnome.Ptyxis.Profile:/org/gnome/Ptyxis/Profiles/$PTYXIS_PROFILE/ opacity .85

    # If using org.gnome.Ptyxis.Devel Nightly Flatpak
    flatpak run --command=gsettings org.gnome.Ptyxis.Devel set org.gnome.Ptyxis.Devel.Profile:/org/gnome/Ptyxis/Devel/Profiles/$PTYXIS_PROFILE/ opacity .85
    ```

- **User Profiles:**
  - Profiles can set a default container, command, font, palette, and terminal
    behavior.

---

## Flatpak and the Agent

The application talks to `ptyxis-agent` over a local socket. The agent creates
PTYs and starts processes on the host or in containers.

The Flatpak normally starts the agent on the host with `flatpak-spawn --host`.

If that fails, Ptyxis tries to run the agent inside the Flatpak. Host and
container access may be limited in that case.

---

## Contributing: Feature Requests and Design Changes

Use the [issue tracker](https://gitlab.gnome.org/chergert/ptyxis/-/issues) for
bugs. Discuss larger changes on the
[GNOME Design Whiteboards](https://gitlab.gnome.org/Teams/Design/whiteboards/)
before filing an issue. A proposal should explain:

- What the change would do.
- How it fits with existing behavior.
- Any migration or testing needed.
- UI sketches, if the change affects the interface.

Once the design is clear, file an issue linking to the discussion.

---

## Troubleshooting

- **Command completion notifications:** Your shell may need to load
  `/etc/profile.d/vte.sh` for VTE shell integration.

  - Check that the file exists and that your shell loads it.
  - If needed, add this to your shell's startup file:
    `if [ -f /etc/profile.d/vte.sh ]; then . /etc/profile.d/vte.sh; fi`

- **Rendering or font problems:** Record your Ptyxis, GTK, and VTE versions,
  graphics driver, display scaling, and steps to reproduce the problem.

  - Try another font or rendering setting.
  - Report reproducible problems to the relevant project:
    [Ptyxis](https://gitlab.gnome.org/chergert/ptyxis/-/issues),
    [GTK](https://gitlab.gnome.org/GNOME/gtk/-/issues), or
    [VTE](https://gitlab.gnome.org/GNOME/vte/-/issues).

- **Current container is not shown:** Tracking a container entered from the
  shell depends on the command emitting a terminal sequence such as OSC 777.
  Check the container tool's shell integration. You can also open a tab in a
  container from the container menu.

---

## License

Ptyxis is licensed under the [GNU GPL, version 3 or later](COPYING).

---

## Acknowledgments

Ptyxis is developed by Christian Hergert. Some of its ideas came from
creating [GNOME Builder](https://apps.gnome.org/app/org.gnome.Builder/).
