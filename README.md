ozzero
======

ZeroMQ binding in Mozart/Oz 1.4.

Instructions
============

1. Install [ZeroMQ 2.2 or 3.1](http://www.zeromq.org/area:download). Make sure
   it is installed as a 32-bit library.

    ```bash
    # Debian/Ubuntu 32-bit
    apt-get install libzmq-dev

    # Arch Linux 32-bit
    pacman -S zeromq

    # Arch Linux 64-bit
    wget https://aur.archlinux.org/packages/li/lib32-zeromq/PKGBUILD
    makepkg -i
    ```

2. Run `ozmake`.
3. Run `./testZmq.exe` to test.

