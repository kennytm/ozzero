ozzero
======

ZeroMQ binding in Mozart/Oz 1.4.

Instructions
============

1. Install [ZeroMQ](http://www.zeromq.org/area:download). Only 2.2, 3.1.0 and
   3.1.1 are currently supported. Make sure it is installed as a 32-bit library.

    ```bash
    # Debian/Ubuntu 32-bit
    apt-get install libzmq-dev

    # Arch Linux 32-bit
    pacman -S zeromq        # 2.2
    packer -S zeromq-dev    # 3.1.0

    # Arch Linux 64-bit
    packer -S lib32-zeromq      # 2.2
    packer -S lib32-zeromq-dev  # 3.1.0
    packer -S lib32-zeromq-git  # 3.1.1 (git master)
    ```

2. Run `ozmake`.
3. Check the `samples/` directory to see some sample code.

