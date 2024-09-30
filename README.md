# HDF5-Encryption
This repo to hold HDF5 Encryption VFD prototype.

Here are instructions how to build in "place".

* Checkout from the reporistory

  `git clone git@github.com:LifeboatLLC/HDF5-Encryption.git`

* Change directory to the build the library in place

  `cd HDF5-Encryption/hdf5/hdf5-1_14_3`

* Set compiler and linker flags to find `gcyrpt` library

  `export CFLAGS=-I/gcrypt_install_dir/include`

  `export LDFLAGS=-L/gcrypt_install_dir/lib/`

  `export LIBS=-lgcrypt`

  `export LD_LIBRARY_PATH=/gcrypt_install_dir/lib/`


