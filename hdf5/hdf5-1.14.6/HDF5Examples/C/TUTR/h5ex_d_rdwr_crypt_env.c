/************************************************************

  This example shows how to read and write data to a
  dataset.  The program first writes integers to a dataset
  with dataspace dimensions of DIM0xDIM1, then closes the
  file.  Next, it reopens the file, reads back the data, and
  outputs it to the screen.

 ************************************************************/

#include "hdf5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE    "h5ex_d_rdwr_crypt_env.h5"
#define DATASET "DS1"
#define DIM0    10
#define DIM1    256

#define SET_ENV_USING_TERMINAL 0

int
main(void)
{
    hid_t   file  = H5I_INVALID_HID;
    hid_t   space = H5I_INVALID_HID;
    hid_t   dset  = H5I_INVALID_HID;
    hid_t   fapl_id = H5I_INVALID_HID;
    herr_t  status;
    hsize_t dims[2] = {DIM0, DIM1};
    int     wdata[DIM0][DIM1]; /* Write buffer */
    int     rdata[DIM0][DIM1]; /* Read buffer */
    hsize_t i, j;
    const char *vfd_name = "page_buffer";
    char       *returned_env_var = NULL;

    char config_str[] =
        "( page_buffer "
        "  ( ( page_size 4096 )"
        "    ( max_num_pages 16 )"
        "    ( replacement_policy 0 )"
        "    ( underlying_VFD "
        "      ( encryption_VFD "
        "        ( ( plaintext_page_size  4096 )"
        "          ( ciphertext_page_size 4112 )"
        "          ( encryption_buffer_size 65792 )"
        "          ( cipher 0 )"
        "          ( cipher_block_size 16 )"
        "          ( key_size  32 )"
        "          ( key --5E73C3BFC3A22CC2AA54055DC3B56169C38E5F7DC395C2AC23C2BE4C14C3B33B )"
        "          ( iv_size 16 )"
        "          ( mode 0 )"
        "          ( underlying_VFD ( sec2 () ) )"
        "        )"
        "      )"
        "    )"
        "  )"
        ")";

/**
 * If SET_ENV_USING_TERMINAL is 0 then the environment variables will be set 
 * through the setenv functions below. 
 * 
 * Otherwise if wanting to test setting the environment variables via terminal
 * set SET_ENV_USING_TERMINAL to 1 and they won't be set again here, but will
 * still call getenv() to ensure the environment variables are set correctly
 * and match the configuration language in vfd_name and config_str.
 */
#if ! SET_ENV_USING_TERMINAL
    /* Set environment variable */
    setenv(HDF5_DRIVER, vfd_name, 1);
    setenv(HDF5_DRIVER_CONFIG, config_str, 1);
#endif /* SET_ENV_USING_TERMINAL */


    /* Double check the environment variables were set correctly */
    returned_env_var = getenv(HDF5_DRIVER);
    strcmp(vfd_name, returned_env_var);

    returned_env_var = getenv(HDF5_DRIVER_CONFIG);
    strcmp(config_str, returned_env_var);


    fapl_id = H5Pcreate(H5P_FILE_ACCESS);

    /*
     * Initialize data.
     */
    for (i = 0; i < DIM0; i++)
        for (j = 0; j < DIM1; j++)
            wdata[i][j] = i * j - j;

    /*
     * Create a new file using the access properties set above.
     */
    file = H5Fcreate(FILE, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);

    /*
     * Create dataspace.  Setting maximum size to NULL sets the maximum
     * size to be the current size.
     */
    space = H5Screate_simple(2, dims, NULL);

    /*
     * Create the dataset.  We will use all default properties for this
     * example.
     */
    dset = H5Dcreate(file, DATASET, H5T_STD_I32LE, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    /*
     * Write the data to the dataset.
     */
    status = H5Dwrite(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, wdata[0]);

    /*
     * Close and release resources.
     */
    status = H5Dclose(dset);
    status = H5Sclose(space);
    status = H5Fclose(file);

    /*
     * Now we begin the read section of this example.
     */

    /*
     * Open file and dataset using the access properties set above.
     */
    file = H5Fopen(FILE, H5F_ACC_RDONLY, fapl_id);
    dset = H5Dopen(file, DATASET, H5P_DEFAULT);

    /*
     * Read the data using the default properties.
     */
    status = H5Dread(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, rdata[0]);

    /*
     * Output the data to the screen.
     */
    printf("%s:\n", DATASET);
    for (i = 0; i < DIM0; i++) {
        printf(" [");
        for (j = 0; j < DIM1; j++)
            printf(" %3d", rdata[i][j]);
        printf("]\n");
    }

    unsetenv(HDF5_DRIVER);
    unsetenv(HDF5_DRIVER_CONFIG);

    /*
     * Close and release resources.
     */
    status = H5Dclose(dset);
    status = H5Fclose(file);
    status = H5Pclose (fapl_id);

    return 0;
}
