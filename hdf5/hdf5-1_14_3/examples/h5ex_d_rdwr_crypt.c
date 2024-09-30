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

#define FILE    "h5ex_d_rdwr_crypt.h5"
#define DATASET "DS1"
#define DIM0    10
#define DIM1    256

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

    H5FD_pb_vfd_config_t     pb_vfd_config =
    {
        /* magic          = */ H5FD_PB_CONFIG_MAGIC,
        /* version        = */ H5FD_CURR_PB_VFD_CONFIG_VERSION,
        /* page_size      = */ H5FD_PB_DEFAULT_PAGE_SIZE,
        /* max_num_pages  = */ H5FD_PB_DEFAULT_MAX_NUM_PAGES,
        /* rp             = */ H5FD_PB_DEFAULT_REPLACEMENT_POLICY,
        /* fapl_id        = */ H5P_DEFAULT  /* will overwrite */
    };
    H5FD_crypt_vfd_config_t  crypt_vfd_config =
    {
        /* magic                  = */ H5FD_CRYPT_CONFIG_MAGIC,
        /* version                = */ H5FD_CURR_CRYPT_VFD_CONFIG_VERSION,
        /* plaintext_page_size    = */ 4096,
        /* ciphertext_page_size   = */ 4112,
        /* encryption_buffer_size = */ H5FD_CRYPT_DEFAULT_ENCRYPTION_BUFFER_SIZE,
        /* cipher                 = */ 0,   /* AES256 */
        /* cipher_block_size      = */ 16,
        /* key_size               = */ 32,
        /* key                    = */ H5FD_CRYPT_TEST_KEY,
        /* iv_size                = */ 16,
        /* mode                   = */ 0,
        /* fapl_id                = */ H5P_DEFAULT
    };
         hid_t                    crypt_fapl_id    = H5I_INVALID_HID;


         crypt_fapl_id = H5Pcreate(H5P_FILE_ACCESS);
         H5Pset_fapl_crypt(crypt_fapl_id, &crypt_vfd_config);
         pb_vfd_config.fapl_id = crypt_fapl_id;
         fapl_id = H5Pcreate(H5P_FILE_ACCESS);
         H5Pset_fapl_pb(fapl_id, &pb_vfd_config);
   

    /*
     * Initialize data.
     */
    for (i = 0; i < DIM0; i++)
        for (j = 0; j < DIM1; j++)
            wdata[i][j] = i * j - j;

    /*
     * Create a new file using the default properties.
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
     * Open file and dataset using the default properties.
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

    /*
     * Close and release resources.
     */
    status = H5Dclose(dset);
    status = H5Fclose(file);
    status = H5Pclose (fapl_id);
    status = H5Pclose (crypt_fapl_id);

    return 0;
}
