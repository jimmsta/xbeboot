xbeboot - *the original Xbox Linux bootloader*
==============================================
xbeboot is a Linux bootloader for the original Xbox. xbeboot was originally developed by the [Xbox Linux Project](https://web.archive.org/web/20100617000252/http://www.xbox-linux.org/wiki/Main_Page).

xbeboot is noteworthy for not only allowing booting from DVD/CD and Hard Drive, but also allowing bundling a kernel into the .xbe itself

Status
------
xbeboot can currently "run", however it doesn't yet have graphics support working so you can't do much with it yet

Getting Started
---------------
### Prerequisites
You will need the following tools:
- make
- gcc
- git

#### Linux (Ubuntu)

    sudo apt-get install build-essential make git

### Download & Build xbeboot
    git clone https://github.com/Xbox-Linux-2/xbeboot.git
    cd xbeboot
    make all
