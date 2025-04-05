#!/bin/sh

. /etc/os-release

case "${NAME}" in
    "Arch"*)
        echo "Installing Arch Linux packages"
        sudo pacman -S --needed \
            autoconf \
            autoconf-archive \
            automake \
            base-devel \
            bash \
            bison \
            clang \
            cmake \
            extra-cmake-modules \
            flex \
            fluidsynth \
            gcc \
            gdb \
            git \
            gperf \
            libevdev \
            libfreetype \
            libpng \
            libserialport \
            libslirp \
            libsndfile \
            libxkbcommon \
            libxkbcommon-x11 \
            make \
            meson \
            multilib-devel \
            ninja \
            openal \
            parted \
            patch \
            pkgconf \
            python \
            python-jinja \
            qt5 \
            rsync \
            rtmidi \
            sdl2-compat \
            sed \
            subversion \
            texinfo \
            vulkan-devel
        ;;
    *)
        echo "Unsupported OS"
        ;;
esac
