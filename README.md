Web Power Switch - C++ Library

Features:
  - auto discovery (finds switch on the same network as host)
  - cached recollection of switches (refreshed forcefully or automatically)

Build

  - sudo apt install cmake libtidy-dev libyaml-cpp-dev libcurl4-gnutls-dev libssl-dev libabsl-dev libcxxopts-dev
  - (optional) meson wrap update-db
  - meson setup build
  - ninja -C build

Mostly works on ubuntu and raspios.
