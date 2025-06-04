# HDF5-Encryption
Copyrigth by Lifeboat, LLC https://www.lifeboat.llc

This repo holds HDF5 Encryption VFD prototype and documentation. **The code can be used for testing purposes only.**

Below are instructions how to build in "place" the version of HDF5 with encryption enables. Please notice that currently key and magic number are hard coded for tools to work and for tests to pass. 

See `hdf5/share/hdf5_examples/c/h5ex_d_rdwr_crypt.c` for how to use new APIs to enable encryption.

* Checkout from the reporistory

  `git clone git@github.com:LifeboatLLC/HDF5-Encryption.git`

* Change directory to build the library in place

  `cd HDF5-Encryption/hdf5/hdf5-1_14_3`

* Set compiler and linker flags to find `gcyrpt` library

  `export CFLAGS=-I/gcrypt_install_dir/include`

  `export LDFLAGS=-L/gcrypt_install_dir/lib/`

  `export LIBS=-lgcrypt`

  `export LD_LIBRARY_PATH=/gcrypt_install_dir/lib/`

* Run the `autogen.sh` script to create `configure` script and several header and source files.
* Build as usual

  `./configure`
  
  `make`
  
  `make check`
  
  `make install`

  Library will be installed in the `hdf5` subdirectory

 * Change directory to the installed examples and compile and run `h5ex_d_rdwr_crypt.c` to create the `h5ex_d_rdwr_crypt.h5` file.
   
   ` cd hdf5/share/hdf5_examples/c`

   `../../../bin/h5cc h5ex_d_rdwr_crypt.c`

   `./a.out`

  * Use `head` command to see encryption plaintext header

    `head h5ex_d_rdwr_crypt.h5`

  * Use tools to see the content and to repack the file to cleartext and back to encrypted one

     `../../../bin/h5dump  h5ex_d_rdwr_crypt.h5`

     `../../../bin/h5repack --src-vfd-name crypt h5ex_d_rdwr_crypt.h5 h5ex_d_rdwr.h5`
     

  * Check that `h5ex_d_rdwr.h5` is cleartext now.
    
  * Try `h5dump` on both files. The tool is smart enough to dump both ciphertext and cleartext files without specifyng the required driver driver.

  * Repack clear text file into encrypted file

      `../../../bin/h5repack --dst-vfd-name crypt h5ex_d_rdwr.h5 h5ex_d_rdwr_crypt_new.h5`

  * Run `ls -al *.h5` to see the size of the files. 
